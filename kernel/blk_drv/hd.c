/*
 *  linux/kernel/hd.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 * This is the low-level hd interrupt support. It traverses the
 * request-list, using interrupts to jump between functions. As
 * all the functions are called within interrupts, we may not
 * sleep. Special care is recommended.
 * 
 *  modified by Drew Eckhardt to check nr of hd's from the CMOS.
 */
/* 
 * 本程序是底层硬盘中断辅助程序。主要用于扫描请求项队列，使用中断
 * 在函数之间跳转。由于所有的函数都是在中断里调用的，所以这些函数
 * 不可以休眠。请特别注意。
 *
 * 由Drew Eckhardt修改，利用CMOS信息检测硬盘数。
 */

#include <linux/config.h>   // 内核配置头文件。定义键盘语言和硬盘类型(HD_TYPE)选项。
#include <linux/sched.h>    // 调度程序头文件，定义任务结构task_struct、任务0数据等。
#include <linux/fs.h>       // 文件系统头文件。定义文件表结构(file、m_inode)等。
#include <linux/kernel.h>   // 内核头文件。含有一些内核常用函数的原形定义。
#include <linux/hdreg.h>    // 硬盘参数头文件。定义硬盘寄存器端口、状态码、分区表等信息。
#include <asm/system.h>     // 系统头文件。定义设置或修改描述符/中断门等的汇编宏。
#include <asm/io.h>         // io头文件。定义硬件端口输入/输出宏汇编语句。
#include <asm/segment.h>    // 段操作头文件。定义了有关段寄存器操作的嵌入式汇编函数。

// 定义硬盘主设备号符号常数。在驱动程序中主设备号必须在包含blk.h文件之前被定义。
// 因为blk.h文件中要用到这个符号常数值来确定一些列其他相关符号常数和宏。
#define MAJOR_NR 3          // 硬盘主设备号是3.
#include "blk.h"            // 块设备头文件。定义请求数据结构、块设备数据结构和宏等信息。

// 读CMOS参数宏函数。
// 这段宏读取CMOS中硬盘信息。outb_p、inb_p是include/asm/io.h中定义的端口输入输出宏。
// 与init/main.c中读取CMOS时钟信息的宏完全一样。
#define CMOS_READ(addr) ({ \
outb_p(0x80|addr,0x70); \       // 0x70是写端口号，0x80|addr是要读的CMOS内存地址。
inb_p(0x71); \                  // 0x71是读端口号
})

/* Max read/write errors/sector */
/* 每扇区读/写操作允许的最多出错次数 */
#define MAX_ERRORS	7           // 读/写一个扇区时允许的最多出错次数。
#define MAX_HD		2           // 系统支持的最多硬盘数

// 重新校正处理函数。
// 复位操作时在硬盘中断处理程序中调用的重新校正函数（287行）
static void recal_intr(void);

// 重新校正标志。当设置了该标志，程序中会调用recal_intr()以将磁头移动到0柱面。
static int recalibrate = 1;
// 复位标志。当发生读写操作时会设置该标志并调用相关复位函数以复位硬盘和控制器。
static int reset = 1;

/*
 *  This struct defines the HD's and their types.
 */
/*
 * 下面结构定义了硬盘参数及类型
 */
// 硬盘信息结构(Harddisk information struct)
// 各字段分别是磁头数、每磁道扇区数、柱面数、写前预补偿柱面号、磁头着陆区柱面号、
// 控制字节。他们的含义请参见程序列表后的说明。
struct hd_i_struct {                    // 一个结构体就代表着一个硬盘
	int head,sect,cyl,wpcom,lzone,ctl;
	};
// 如果已经在include/linux/config.h配置文件中定义了符号常数HD_TYPE，就取其中定义
// 好的参数作为硬盘信息数组hd_info[]中的数据。否则先默认都设为0值，在setup()函数
// 中会重新进行设置。
// hd_info[]中存放着所有硬盘的信息
#ifdef HD_TYPE
struct hd_i_struct hd_info[] = { HD_TYPE };                         // 硬盘信息数组
#define NR_HD ((sizeof (hd_info))/(sizeof (struct hd_i_struct)))    // 计算硬盘个数
#else
struct hd_i_struct hd_info[] = { {0,0,0,0,0,0},{0,0,0,0,0,0} };
static int NR_HD = 0;
#endif

// 定义硬盘分区结构。给出每个分区从硬盘0道开始算起的物理起始扇区号和分区扇区总数。
// 其中5的倍数处的项(例如hd[0]和hd[5]等)代表整个硬盘的参数。
// 查看/dev/sd...应该能看个大概，sda是整个硬盘，sda1是sda硬盘的第一个分区......
static struct hd_struct {
	long start_sect;        // 分区在硬盘中的起始物理(绝对)扇区。
	long nr_sects;          // 分区中扇区总数。
} hd[5*MAX_HD]={{0,0},};

// 读端口嵌入汇编宏。读端口port，共读nr字，保存在buf中。
#define port_read(port,buf,nr) \
__asm__("cld;rep;insw"::"d" (port),"D" (buf),"c" (nr):"cx","di")

// 写端口嵌入汇编宏。写端口port，共写nr字，从buf中取数据。
#define port_write(port,buf,nr) \
__asm__("cld;rep;outsw"::"d" (port),"S" (buf),"c" (nr):"cx","si")

extern void hd_interrupt(void);     // 硬盘中断过程(system_call.s 221)
extern void rd_load(void);          // 虚拟盘创建加载函数(ramdisk.c 71)

/* This may be used only once, enforced by 'static int callable' */
/* 下面该函数只在初始化时被调用一次。用静态变量callable作为可调用标志。 */
// 系统设置函数。
// 函数参数BIOS是由初始化程序init/main.c中init子程序设置为指向硬盘参数表结构的指针。
// 该硬盘参数表结构包含2个硬盘参数表的内容(共32字节)，是从内存0x90080处复制而来。
// 0x90080处的硬盘参数表是由setup.s程序利用ROM BIOS功能取得。硬盘参数表信息参见程序
// 列表后的说明。本函数主要功能是读取CMOS和硬盘参数表信息，用于设置硬盘分区结构hd，
// 并尝试加载RAM虚拟盘和根文件系统。
//
// 主要是读取硬盘信息，加载虚拟盘和根文件系统
int sys_setup(void * BIOS)
{
	static int callable = 1;        // 限制本函数只能被调用1次的标志
	int i,drive;
	unsigned char cmos_disks;
	struct partition *p;
	struct buffer_head * bh;

// 首先设置callable标志，使得本函数只能被调用1次。然后设置硬盘信息数组hd_info[]。
// 如果在include/linux/config.h文件中已定义了符号常数HD_TYPE，那么hd_info[]数组
// 已经在前面第49行上设置好了。否则就需要读取boot/setup.s程序存放在内存0x90080处
// 开始的硬盘参数表。setup.s程序在内存此处连续存放着一到两个硬盘参数表。
	if (!callable)
		return -1;
	callable = 0;
#ifndef HD_TYPE         // 如果没有定义HD_TYPE，则读取。
	for (drive=0 ; drive<2 ; drive++) {
		hd_info[drive].cyl = *(unsigned short *) BIOS;          // 柱面数
		hd_info[drive].head = *(unsigned char *) (2+BIOS);      // 磁头数
		hd_info[drive].wpcom = *(unsigned short *) (5+BIOS);    // 写前预补偿柱面号
		hd_info[drive].ctl = *(unsigned char *) (8+BIOS);       // 控制字节
		hd_info[drive].lzone = *(unsigned short *) (12+BIOS);   // 磁头着陆区柱面号
		hd_info[drive].sect = *(unsigned char *) (14+BIOS);     // 每磁道扇区数
		BIOS += 16;             // 每个硬盘参数表长16字节，这里BIOS指向下一表。
	}
// setup.s程序在取BIOS硬盘参数表信息时，如果系统中只有1个硬盘，就会将对应第2个
// 硬盘的16字节全部清零。因此这里只要判断第2个硬盘柱面数是否为0就可以知道是否有
// 第2个硬盘了。
	if (hd_info[1].cyl)
		NR_HD=2;        // 第二个硬盘有扇区，说明第二个硬盘存在，硬盘数设置为2
	else
		NR_HD=1;
#endif
// 到这里，硬盘信息数组hd_info[]已经设置好，并且确定了系统含有的硬盘数NR_HD。现在
// 开始设置硬盘分区结构数组hd[]。该数组的项0和项5分别表示两个硬盘的整体参数，而
// 项1-4和6-9分表表示两个硬盘的4个分区的参数。因此这里仅设置表示硬盘整体信息
// 的两项(项0和5)
	for (i=0 ; i<NR_HD ; i++) {
		hd[i*5].start_sect = 0;                     // 硬盘起始扇区号。
		hd[i*5].nr_sects = hd_info[i].head*
				hd_info[i].sect*hd_info[i].cyl;     // 硬盘总扇区数。
	}

	/*
		We querry CMOS about hard disks : it could be that 
		we have a SCSI/ESDI/etc controller that is BIOS
		compatable with ST-506, and thus showing up in our
		BIOS table, but not register compatable, and therefore
		not present in CMOS.

		Furthurmore, we will assume that our ST-506 drives
		<if any> are the primary drives in the system, and 
		the ones reflected as drive 1 or 2.

		The first drive is stored in the high nibble of CMOS
		byte 0x12, the second in the low nibble.  This will be
		either a 4 bit drive type or 0xf indicating use byte 0x19 
		for an 8 bit type, drive 1, 0x1a for drive 2 in CMOS.

		Needless to say, a non-zero value means we have 
		an AT controller hard disk for that drive.

		
	*/
    /* 我们对CMOS有关硬盘的信息有些怀疑：可能会出现这样的情况，
     * 我们有一块SCSI/ESDI等的控制器，它是以ST-506方式与BIOS
     * 相兼容的，因而会出现在我们的BIOS参数表中，但却又不是寄存器
     * 兼容的，因此这些参数在CMOS中又不存在。
     * 
     * 另外，我们假设ST-506驱动器(如果有的话)是系统中的基本驱动器，
     * 也即以驱动器1或2出现的驱动器。
     *
     * 第1个驱动器参数存放在CMOS字节0x12的高半字节中，第2个存放在
     * 低半字节中。该4位字节信息可以是驱动器类型，也可能仅是0xf。
     * 0xf表示使用CMOS中0x19字节作为驱动器1的8位类型字节，使用CMOS
     * 中0x1A字节作为驱动器2的类型字节。
     *
     * 总之，一个非零值意味着硬盘是一个AT控制器兼容硬盘。
     */

// 这里根据上述原理，下面代码用来检测硬盘到底是不是AT控制器兼容的。有关CMOS信息
// 请参见第4章中4.2.3.1节。这里从CMOS偏移地址0x12处读取硬盘类型字节。如果低半字
// 节值(存放着第2个硬盘类型值)不为0，则表示系统有两个硬盘，否则表示系统只有1个
// 硬盘。如果0x12处读出的值为0，则表示系统中没有AT兼容硬盘。
	if ((cmos_disks = CMOS_READ(0x12)) & 0xf0)
		if (cmos_disks & 0x0f)
			NR_HD = 2;
		else
			NR_HD = 1;
	else
		NR_HD = 0;
// 若NR_HD=0，则两个硬盘都不是AT控制器兼容的，两个硬盘数据结构全清零。
// 若NR_HD=1，则将第2个硬盘的参数清零。
	for (i = NR_HD ; i < 2 ; i++) {
		hd[i*5].start_sect = 0;
		hd[i*5].nr_sects = 0;
	}
// 好，到此为止我们已经真正确定了系统中所含的硬盘个数NR_HD。现在我们来读取两个硬盘
// 上第1个扇区中的分区表信息，用来设置分区结构数组hd[]中硬盘各分区的信息。首先利用
// 读块函数bread()读硬盘第1个数据块(fs/buffer.c 267)，第1个参数(0x300、0x305)分别
// 是两个硬盘的设备号，第2个参数(0)是所需读取的块号。若读操作成功，则数据会被存放在
// 缓冲块bh的数据区中。若缓冲块头指针bh为0，则说明读操作失败，则显示出错信息并停机。
// 否则我们根据硬盘第1个扇区最后两个字节应该是0xAA55来判断扇区中数据的有效性，从而
// 可以知道扇区中位于偏移0x1BE开始处的分区表是否有效。若有效则将硬盘分区表信息放入
// 硬盘分区结构数组hd[]中，最后释放bh缓冲区。
	for (drive=0 ; drive<NR_HD ; drive++) {
		if (!(bh = bread(0x300 + drive*5,0))) {     // 0x300、0x305是设备号
			printk("Unable to read partition table of drive %d\n\r",
				drive);
			panic("");
		}
		if (bh->b_data[510] != 0x55 || (unsigned char)
		    bh->b_data[511] != 0xAA) {              // 判断硬盘标志0xAA55
			printk("Bad partition table on drive %d\n\r",drive);
			panic("");
		}
		p = 0x1BE + (void *)bh->b_data;             // 分区表位于第1扇区0x1BE处
		for (i=1;i<5;i++,p++) {
			hd[i+5*drive].start_sect = p->start_sect;
			hd[i+5*drive].nr_sects = p->nr_sects;
		}
		brelse(bh);         // 释放为存放硬盘数据块而申请的缓冲区
	}
// 现在总算完成设置硬盘分区结构数组hd[]的任务。如果确实有硬盘存在并且已读入其分区
// 表，则显示“分区表正常”信息。然后尝试在系统内存虚拟盘中加载启动盘中包含的根文件系统
// 映像(blk_drv/ramdisk.c 71)。即在系统设置有虚拟盘的情况下判断启动盘上是否还含有
// 根文件系统的映像数据。如果有(此时该启动盘称为集成盘)则尝试把该映像加载并存放到
// 虚拟盘中，然后把此时的根文件系统设备号ROOT_DEV修改成虚拟盘的设备号。
// 最后安装根文件系统(fs/super.c 242)
	if (NR_HD)
		printk("Partition table%s ok.\n\r",(NR_HD>1)?"s":"");
	rd_load();              // 尝试创建并加载虚拟盘
	mount_root();           // 安装根文件系统
	return (0);
}

static int controller_ready(void)
{
	int retries=10000;

	while (--retries && (inb_p(HD_STATUS)&0xc0)!=0x40);
	return (retries);
}

static int win_result(void)
{
	int i=inb_p(HD_STATUS);

	if ((i & (BUSY_STAT | READY_STAT | WRERR_STAT | SEEK_STAT | ERR_STAT))
		== (READY_STAT | SEEK_STAT))
		return(0); /* ok */
	if (i&1) i=inb(HD_ERROR);
	return (1);
}

static void hd_out(unsigned int drive,unsigned int nsect,unsigned int sect,
		unsigned int head,unsigned int cyl,unsigned int cmd,
		void (*intr_addr)(void))
{
	register int port asm("dx");

	if (drive>1 || head>15)
		panic("Trying to write bad sector");
	if (!controller_ready())
		panic("HD controller not ready");
	do_hd = intr_addr;
	outb_p(hd_info[drive].ctl,HD_CMD);
	port=HD_DATA;
	outb_p(hd_info[drive].wpcom>>2,++port);
	outb_p(nsect,++port);
	outb_p(sect,++port);
	outb_p(cyl,++port);
	outb_p(cyl>>8,++port);
	outb_p(0xA0|(drive<<4)|head,++port);
	outb(cmd,++port);
}

static int drive_busy(void)
{
	unsigned int i;

	for (i = 0; i < 10000; i++)
		if (READY_STAT == (inb_p(HD_STATUS) & (BUSY_STAT|READY_STAT)))
			break;
	i = inb(HD_STATUS);
	i &= BUSY_STAT | READY_STAT | SEEK_STAT;
	if (i == READY_STAT | SEEK_STAT)
		return(0);
	printk("HD controller times out\n\r");
	return(1);
}

static void reset_controller(void)
{
	int	i;

	outb(4,HD_CMD);
	for(i = 0; i < 100; i++) nop();
	outb(hd_info[0].ctl & 0x0f ,HD_CMD);
	if (drive_busy())
		printk("HD-controller still busy\n\r");
	if ((i = inb(HD_ERROR)) != 1)
		printk("HD-controller reset failed: %02x\n\r",i);
}

static void reset_hd(int nr)
{
	reset_controller();
	hd_out(nr,hd_info[nr].sect,hd_info[nr].sect,hd_info[nr].head-1,
		hd_info[nr].cyl,WIN_SPECIFY,&recal_intr);
}

void unexpected_hd_interrupt(void)
{
	printk("Unexpected HD interrupt\n\r");
}

static void bad_rw_intr(void)
{
	if (++CURRENT->errors >= MAX_ERRORS)
		end_request(0);
	if (CURRENT->errors > MAX_ERRORS/2)
		reset = 1;
}

static void read_intr(void)
{
	if (win_result()) {
		bad_rw_intr();
		do_hd_request();
		return;
	}
	port_read(HD_DATA,CURRENT->buffer,256);
	CURRENT->errors = 0;
	CURRENT->buffer += 512;
	CURRENT->sector++;
	if (--CURRENT->nr_sectors) {
		do_hd = &read_intr;
		return;
	}
	end_request(1);
	do_hd_request();
}

static void write_intr(void)
{
	if (win_result()) {
		bad_rw_intr();
		do_hd_request();
		return;
	}
	if (--CURRENT->nr_sectors) {
		CURRENT->sector++;
		CURRENT->buffer += 512;
		do_hd = &write_intr;
		port_write(HD_DATA,CURRENT->buffer,256);
		return;
	}
	end_request(1);
	do_hd_request();
}

static void recal_intr(void)
{
	if (win_result())
		bad_rw_intr();
	do_hd_request();
}

void do_hd_request(void)
{
	int i,r;
	unsigned int block,dev;
	unsigned int sec,head,cyl;
	unsigned int nsect;

	INIT_REQUEST;
	dev = MINOR(CURRENT->dev);
	block = CURRENT->sector;
	if (dev >= 5*NR_HD || block+2 > hd[dev].nr_sects) {
		end_request(0);
		goto repeat;
	}
	block += hd[dev].start_sect;
	dev /= 5;
	__asm__("divl %4":"=a" (block),"=d" (sec):"0" (block),"1" (0),
		"r" (hd_info[dev].sect));
	__asm__("divl %4":"=a" (cyl),"=d" (head):"0" (block),"1" (0),
		"r" (hd_info[dev].head));
	sec++;
	nsect = CURRENT->nr_sectors;
	if (reset) {
		reset = 0;
		recalibrate = 1;
		reset_hd(CURRENT_DEV);
		return;
	}
	if (recalibrate) {
		recalibrate = 0;
		hd_out(dev,hd_info[CURRENT_DEV].sect,0,0,0,
			WIN_RESTORE,&recal_intr);
		return;
	}	
	if (CURRENT->cmd == WRITE) {
		hd_out(dev,nsect,sec,head,cyl,WIN_WRITE,&write_intr);
		for(i=0 ; i<3000 && !(r=inb_p(HD_STATUS)&DRQ_STAT) ; i++)
			/* nothing */ ;
		if (!r) {
			bad_rw_intr();
			goto repeat;
		}
		port_write(HD_DATA,CURRENT->buffer,256);
	} else if (CURRENT->cmd == READ) {
		hd_out(dev,nsect,sec,head,cyl,WIN_READ,&read_intr);
	} else
		panic("unknown hd-command");
}

void hd_init(void)
{
	blk_dev[MAJOR_NR].request_fn = DEVICE_REQUEST;
	set_intr_gate(0x2E,&hd_interrupt);
	outb_p(inb_p(0x21)&0xfb,0x21);
	outb(inb_p(0xA1)&0xbf,0xA1);
}
