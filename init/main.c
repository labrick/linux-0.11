/*
 *  linux/init/main.c
 *
 *  (C) 1991  Linus Torvalds
 */

// 定义宏"__LIBRARY__"是为了包括定义在unistd.h中的内嵌汇编代码等信息
#define __LIBRARY__
// *.h头文件所在的默认目录是include/，则在代码中就不用明确指明其位置。如果不是UNIX的标准
// 头文件，则需要指明所在的目录（为什么？谁指定的include/？），并用双引号括住。unistd.h是
// 标准符号常数与类型文件，其中定义了各种符号常数和类型，并声明了各种函数。如果还定义了
// 符号__LIBRARY__，则还会包括系统调用号和内嵌汇编代码syscall0()等。（找到原因....）
#include <unistd.h>
#include <time.h>		// 时间类型头文件。其中最主要定义了tm结构和一些有关事件的函数原型

/*
 * we need this inline - forking from kernel space will result
 * in NO COPY ON WRITE (!!!), until an execve is executed. This
 * is no problem, but for the stack. This is handled by not letting
 * main() use the stack at all after fork(). Thus, no function
 * calls - which means inline code for fork too, as otherwise we
 * would use the stack upon exit from 'fork()'.
 *
 * Actually only pause and fork are needed inline, so that there
 * won't be any messing with the stack from main(), but we define
 * some others too.
 */
/*
 * 我们需要下面这些内嵌语句 - 从内核空间创建进程将导致没有写时复制(COPY ON WRITE)!!!
 * 直到执行一个execve调用。这对堆栈可能带来问题。处理方法是在fork()调用后不让main()使用
 * 任何堆栈。因此就不能有函数调用 - 这意味着fork也要使用内嵌的代码，否则我们在从fork()
 * 退出时就要使用堆栈了。
 *
 * 事实上只有pause和fork需要使用内嵌方式，以保证从main()中不会弄乱堆栈，但是我们同时还
 * 定义了其他一些函数。
 */
// Linux在内核空间创建时不使用写时复制技术(Copy on write)。main()在移动到用户模式
// (到任务0)后执行内嵌方式的fork()和pause()，因此可保证不使用任务0的用户栈。
// 在执行moveto_user_mode()之后，本程序main()就以任务0的身份在运行了。而任务0是所有
// 将创建子进程的父进程。当创建一个子进程时(init进程)，由于任务1代码属于内核空间，
// 因此没有使用写时复制功能。此时任务0的用户栈就是任务1的用户栈，即它们共同使用一个
// 栈空间。因此希望在main.c运行在任务0的环境下时不要有对堆栈的任何操作，以免弄乱
// 堆栈。而在再次执行fork()并执行过execve()函数后，被加载程序已不属于内核空间，
// 因此可以使用写时复制技术了。参见“Linux内核使用内存的方法”一节内容。

// 下面__syscall0()是unistd.h中的内嵌宏代码。以嵌入汇编的形式调用Linux的系统调用
// 中断0x80。该中断时所有系统调用的入口。该条语句实际上是int fork()创建进程系统调用。
// 可参见include/unistd.h，133行。
static inline _syscall0(int,fork)
// int pause()系统调用：暂停进程的执行，直接收到一个信号
static inline _syscall0(int,pause)
// int setup(void * BIOS)系统调用，仅用于Linux初始化(仅在这个程序中被调用)
static inline _syscall1(int,setup,void *,BIOS)
// int sync()系统调用：更新文件系统
static inline _syscall0(int,sync)

#include <linux/tty.h>		// tty头文件，定义了有关tty_io，串行通信方面的参数、常数
#include <linux/sched.h>	// 调用程序头文件，定义了任务结构task_struct、第1个初始任务
							// 的数据。还有一些以宏的形式定义的有关描述符参数设置和获取
							// 的嵌入式汇编函数程序。
#include <linux/head.h>		// head头文件，定义了段描述符的简单结构，和几个选择符常量
#include <asm/system.h>		// 系统头文件。以宏的形式定义了许多有关设置或修改
							// 描述符/中断门等的嵌入式汇编子程序。
#include <asm/io.h>			// io头文件。以宏的嵌入汇编程序形式定义对io端口操作的函数。

#include <stddef.h>			// 标准定义头文件。定义了NULL, offsetof(TYPE, MEMBER)
#include <stdarg.h>			// 标准参数头文件。以宏的形式定义变量参数列表。主要说明了
							// 一个类型(va_list)和三个宏(va_start, va_arg和va_end),
							// vsprintf、vprintf、vfprintf
#include <unistd.h>
#include <fcntl.h>			// 文件控制头文件。用于文件及描述符的操作控制常数符号的定义
#include <sys/types.h>		// 类型头文件。定义了基本的系统数据类型

#include <linux/fs.h>		// 文件系统头文件。定义文件表结构(file, buffer_head, m_inode等)

static char printbuf[1024];	// 静态字符串数组，用户内核显示信息的缓存

extern int vsprintf();		// 送格式化输出到一字符串中(vsprintf.c，92行)
extern void init(void);		// 函数原型，初始化(在168行)
extern void blk_dev_init(void);		// 块设备初始化子程序(blk_drv/ll_rw_blk.c，157行)
extern void chr_dev_init(void);		// 字符设备初始化(chr_dev/tty_io.c，347行)
extern void hd_init(void);			// 硬盘初始化程序(blk_drv/hd.c，343行)
extern void floppy_init(void);		// 软驱初始化程序(blk_drv/floppy.c，457行)
extern void mem_init(long start, long end);		// 内存管理初始化(mm/memory.c，399行)
extern long rd_init(long mem_start, int length);// 虚拟盘初始化(blk_drv/randisk.c，52行)
extern long kernel_mktime(struct tm * tm);		// 计算系统开机启动时间(秒)
extern long startup_time;						// 内核启动时间(开机时间)(秒)

/*
 * This is set up by the setup-routine at boot-time
 */
/*
 * 以下这些数据是在内核引导期间由setup.s程序设置的
 */
// 下面三行将指定的线性地址强行转换为给定数据类型的指针，并获取指针所指内容。由于内核
// 代码段被映射到从物理地址零开始的地方，因此这些线性地址正好也是对应的物理地址。
// 这些指定地址处内存值得含义请参见第6章的表6-2(setup程序读取并保存的参数)。
// drive_info结构请参见下面第102行。
#define EXT_MEM_K (*(unsigned short *)0x90002)		// 1MB以后的扩展内存大小(KB)
#define DRIVE_INFO (*(struct drive_info *)0x90080)	// 硬盘参数表的32字节内容
#define ORIG_ROOT_DEV (*(unsigned short *)0x901FC)	// 根文件系统所在设备号

/*
 * Yeah, yeah, it's ugly, but I cannot find how to do this correctly
 * and this seems to work. I anybody has more info on the real-time
 * clock I'd be interested. Most of this was trial and error, and some
 * bios-listing reading. Urghh.
 */
/*
 * 是啊，是啊，下面这段程序很差劲，但我不知道如何正确地实现，而且好像它还能运行。如果
 * 有关于实时时钟更多的资料，那我很感兴趣。这些都是试探出来的，另外还看了一些bios程序，呵
 */

// 这段宏读取CMOS实时时钟信息。outb_p和inb_p是incude/asm/io.h中定义的端口输入输出宏
#define CMOS_READ(addr) ({ \
outb_p(0x80|addr,0x70); \		// 0x70是写地址端口号，0x80|addr是要读取的CMOS内存地址
inb_p(0x71); \					// 0x71是读数据端口号
})

// 定义宏。将BCD码转换成二进制数值。BCD码利用半个字节(4比特)表示一个10进制数，因此一个
// 字节表示2个10进制数。(val)&15取BCD表示的10进制个位数，而(val)>>4取BCD表示的10进制
// 十位数，再乘以10.因此最后两者相加就是一个字节BCD码的实际二进制数值。
#define BCD_TO_BIN(val) ((val)=((val)&15) + ((val)>>4)*10)

// 该函数取CMOS实时时钟信息作为开机时间，并保存到全局变量startup_time(秒)中。参见后面
// CMOS内存列表说明。其中调用的函数kernel_mktime()用于计算从1970年1月1日0时起到开机当日
// 经过的秒数，作为开机时间(kernel/mktime.c 41行)
static void time_init(void)
{
	struct tm time;		// 时间结构tm定义在include/time.h中

// CMOS的访问速度很慢。为了减少时间误差，在读取了下面循环中所有数值后，若此时CMOS中秒值
// 发生了变化，那么就重新读取所有值。这样内核就能把与CMOS时间误差控制在1秒之内。
	do {
		time.tm_sec = CMOS_READ(0);			// 当前时间秒值(均是BCD码值)
		time.tm_min = CMOS_READ(2);			// 当前分钟值
		time.tm_hour = CMOS_READ(4);		// 当前小时值
		time.tm_mday = CMOS_READ(7);		// 一月中的当天日期
		time.tm_mon = CMOS_READ(8);			// 当月月份(1--12)
		time.tm_year = CMOS_READ(9);		// 当前年份
	} while (time.tm_sec != CMOS_READ(0));
	BCD_TO_BIN(time.tm_sec);				// 转换成二进制数值
	BCD_TO_BIN(time.tm_min);
	BCD_TO_BIN(time.tm_hour);
	BCD_TO_BIN(time.tm_mday);
	BCD_TO_BIN(time.tm_mon);
	BCD_TO_BIN(time.tm_year);
	time.tm_mon--;							// tm_mon中月份范围是0--11
	startup_time = kernel_mktime(&time);	// 计算开机时间。kernel/mktime.c 41行
}

// 下面定义一些局部变量
static long memory_end = 0;					// 机器具有的物理内存容量(字节数)
static long buffer_memory_end = 0;			// 高速缓冲区末端地址
static long main_memory_start = 0;			// 主内存(将用于分页)开始的位置

struct drive_info { char dummy[32]; } drive_info;	// 用于存放硬盘参数表信息

void main(void)		/* This really IS void, no error here. */
{					/* The startup routine assumes (well, ...) this */
					/* 这里确实是void，没错。在startup程序(head.s)中就是这样假设 */
					// 参见head.s程序第136行开始的几行代码
/*
 * Interrupts are still disabled. Do necessary setups, then
 * enable them
 */
/*
 * 此时中断仍被禁止着，做完必要的设置后就将其开启。
 */
// 下面这段代码用于保存：
// 跟设备号 --> ROOT_DEV;	高速缓存末端地址 --> buffer_memory_end;
// 机器内存数 --> memory_end;	主内存开始地址 --> main_memory_start;
// 其中ROOT_DEV已在前面包含进的include/linux/fs.h文件第198行上被声明为extern int
 	ROOT_DEV = ORIG_ROOT_DEV;				// ROOT_DEV定义在fs/super.c 29行
 	drive_info = DRIVE_INFO;				// 复制0x90080处的硬盘参数表
	memory_end = (1<<20) + (EXT_MEM_K<<10);	// 内存大小=1Mb + 扩展内存(k) * 1024字节
	memory_end &= 0xfffff000;				// 忽略不到4Kb(1页)的内存数
	if (memory_end > 16*1024*1024)			// 如果内存量超过16MB，则按16MB计
		memory_end = 16*1024*1024;
	if (memory_end > 12*1024*1024) 
		buffer_memory_end = 4*1024*1024;	// 如果内存>12MB，则设置缓冲区末端=4MB
	else if (memory_end > 6*1024*1024)
		buffer_memory_end = 2*1024*1024;	// 否则若内存>6MB，则设置缓冲区末端=2MB
	else
		buffer_memory_end = 1*1024*1024;	// 否则则设置缓冲区末端=1MB
	main_memory_start = buffer_memory_end;	// 主内存起始位置 = 缓冲区末端

// 如果在Makefile文件中定义了内存虚拟盘符号RAMDISK，则初始化虚拟盘。此时主内存将减少
// 参见kernel/blk_drv/ramdisk.c
#ifdef RAMDISK
	main_memory_start += rd_init(main_memory_start, RAMDISK*1024);
#endif
// 以下是内核进行所有方面的初始化工作。阅读时最好跟着调用的程序深入进去看，若实在看
// 不下去了，就先放一放，继续看下一个初始化调用 -- 这时经验之谈 :-)
	mem_init(main_memory_start,memory_end);	// 主内存区初始化(mm/memory.c 399)
	trap_init();		// 陷阱门(硬件中断向量)初始化(kernel/traps.c 181)
	blk_dev_init();		// 块设备初始化(kernel/blk_drv/ll_rw_blk.c 157)
	chr_dev_init();		// 字符设备初始化(kernel/chr_drv/tty_io.c 347)
	tty_init();			// tty初始化(kernel/chr_drv/tty_io.c 105)
	time_init();		// 设置开机启动时间 --> startup_time(76)
	sched_init();		// 调度程序初始化(加载任务0的tr,ldtr)(kernel/sched.c 385)
	buffer_init(buffer_memory_end);	// 缓冲管理初始化，建内存链表等(fs/buffer.c 348)
	hd_init();			// 硬盘初始化(kernel/blk_drv/hd.c 343)
	floppy_init();		// 软驱初始化(kernel/blk_drv/floppy.c 457)
	sti();				// 所有初始化工作都做完了，开启中断

// 下面过程通过在堆栈中设置的参数，利用中断返回指令启动任务0执行
	move_to_user_mode();	// 移到用户模式下执行(include/asm/system.h 1)
	if (!fork()) {		/* we count on this going ok */
		init();			// 在新建的子进程(任务1)中执行
	}

// 下面代码开始以任务0的身份运行
/*
 *   NOTE!!   For any other task 'pause()' would mean we have to get a
 * signal to awaken, but task0 is the sole exception (see 'schedule()')
 * as task 0 gets activated at every idle moment (when no other tasks
 * can run). For task0 'pause()' just means we go check if some other
 * task can run, and if not we return here.
 */
/*
 * 注意!! 对于任何其他的任务，'pause()'将意味着我们必须等待收到一个信号才会返回
 * 就绪态，但任务0(task0)是唯一例外情况(参见'schedule()')，因为任务0在任何空闲
 * 时间里就会被激活(当没有其他任务在运行时)，因此对于任务0'pause()'仅意味着我们
 * 返回来查看是否有其他任务可以运行，如果没有的话我们就回到这里，一直循环执行
 * 'pause()'。
 */
// pause()系统调用(kernel/sched.c 144)会把任务0转换成可中断等待状态，再执行调度程序
// 但是调度函数只要发现系统中没有其他任务可以运行时就会切换到任务0，而不依赖与任务0
// 的状态
	for(;;) pause();
}

// 下面函数产生格式化输出信息并输出到标准输出设备stdout(1)，这里是屏幕上显示。参数'*fmt'
// 指定输出将采用的格式，参见标准C语言书籍。该子程序正好是vsprintf如何使用的一个简单例
// 子。改程序使用vsprintf()将格式化的字符串放入printbuf缓冲区，然后用write()将缓冲区的
// 内容输出到标准设备(1--stdout)。vsprintf()函数的实现见kernel/vsprintf.c
static int printf(const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	write(1,printbuf,i=vsprintf(printbuf, fmt, args));
	va_end(args);
	return i;
}

// 读取并执行/etc/rc文件时所使用的命令行参数和环境参数
static char * argv_rc[] = { "/bin/sh", NULL };		// 调用执行程序时参数的字符串数组
static char * envp_rc[] = { "HOME=/", NULL };		// 调用执行程序时的环境字符串数组

// 运行登录shell时所使用的命令行参数和环境参数
// 第165行中的argv[0]中的字符“-”是传递给shell程序sh的一个标志。通过识别该标志，
// sh程序会作为登录shell执行。其执行过程与在shell提示符下执行sh不一样
static char * argv[] = { "-/bin/sh",NULL };			// 同上
static char * envp[] = { "HOME=/usr/root", NULL };

// 在main()中已经进行了系统初始化，包括内存管理、各种硬件设备和驱动程序。init()函数
// 运行在任务0第1次创建的子程序(任务1)中。它首先对第一个将要执行的程序(shell)的环境
// 进行初始化，然后以登录shell方式加载改程序并执行之。
void init(void)
{
	int pid,i;

// setup()是一个系统调用。用于读取硬盘参数包括分区表信息并加载虚拟盘(若存在的话)和
// 安装根文件系统设备。该函数用25行上的宏定义，对应函数是sys_setup()，在块设备子目录
// kernel/blk_drv/hd.c 71
	setup((void *) &drive_info);		// drive_info结构中是2个硬盘参数表

// 下面以读写访问方式打开设备“/dev/tty0”，它对应中断控制台。由于这是第一个打开文件
// 操作，因此产生的文件句柄号(文件描述符)肯定是0。该句柄是UNIX类操作系统默认的控制台
// 标准输入句柄stdin。这里再把它以读和写的方式分别打开时为了复制产生标准输出(写)句柄
// stdout和标准出错输出句柄stderr。函数前面的“(void)”前缀用于表示强制函数无需返回值
	(void) open("/dev/tty0",O_RDWR,0);
	(void) dup(0);		// 复制句柄，产生句柄1号--stdout标准输出设备
	(void) dup(0);		// 复制句柄，产生句柄2号--stderr标准出错输出设备

// 下面打印缓冲区块数和总字节数，每块1024字节，以及主内存区空闲内存字节数
// NR_BUFFERS定义在fs/buffer.c中，BLOCK_SIZE定义在include/linux/fs.h中
	printf("%d buffers = %d bytes buffer space\n\r",NR_BUFFERS,
		NR_BUFFERS*BLOCK_SIZE);
	printf("Free mem: %d bytes\n\r",memory_end-main_memory_start);

// 下面fork()用于创建一个子进程(任务2)。对于被创建的子进程，fork()将返回0值，对于
// 原进程(父进程)则返回子进程的进程号pid。所以第180-184行驶子进程执行的内容。该
// 子进程关闭了句柄0(stdin)、以只读方式打开/etc/rc文件，并使用execve()函数将进程
// 自身替换成/bin/sh程序(即shell程序)，然后执行/bin/sh程序。所携带的参数和环境变量
// 分别由argv_rc和arvp_rc数组给出。关闭句柄0并立刻打开/etc/rc文件的作用是把标准输
// 入stdin重定向到/etc/rc文件。这样shell程序/bin/sh就可以运行rc文件中设置的命令。
// 由于这里sh的运行方式是非交互式的，因此在执行完rc文件中的命令后就会立刻退出，进程2
// 也随之结束。关于execve()函数说明请参见fs/exec.c程序 182行
// 函数_exit()退出时的出错码1 - 操作未许可； 2 - 文件或目录不存在。
	if (!(pid=fork())) {
		close(0);
		if (open("/etc/rc",O_RDONLY,0))
			_exit(1);						// 如果打开文件失败，则退出(lib/_exit.c 0)
		execve("/bin/sh",argv_rc,envp_rc);	// 替换成/bin/sh程序并执行
		_exit(2);							// 若execve()执行失败则退出
	}

// 下面是父进程(1)执行的语句。wait()等待子进程停止或终止，返回值应是子进程的进程号
// (pid)。这三句的作用是父进程等待子进程的结束。&i是存放返回状态信息的位置。如果
// wait()返回值不等于子进程号，则继续等待。
	if (pid>0)
		while (pid != wait(&i))	// 好像没有人为i赋值过
			/* nothing */;		/* 空循环 */

// 如果执行到这里，说明刚创建的子进程的执行已停止或终止了。下面循环中首先再创建一个
// 子进程，如果出错，则显示“初始化程序创建子进程失败”信息并继续执行。对于所创建的
// 子进程将关闭所有以前还遗留的句柄(stdin, stdout, stderr)，新创建一个会话并设置进程
// 组号，然后重新打开/dev/tty0作为stdin，并复制成stdout和stderr。再次执行系统解释程序
// /bin/sh。但这次所选用的参数和环境数组另选了一套(见上面165-167行)。然后父进程再次
// 运行wait()等待。如果子进程又停止了执行，则在标准输出上显示出错信息“子进程pid停止
// 了运行，返回码是i”，然后继续重试下去...，形成一个“大”循环。此外，wait()的另外一个
// 功能是处理孤儿进程。如果一个进程的父进程先终止了，那么这个进程的父进程就会被设置为
// 这里的init进程(进程1)，并由init进程负责释放一个已终止进程的任务数据结构等资源
//
// 这里也是守护进程的原理 by yn
	while (1) {
		if ((pid=fork())<0) {
			printf("Fork failed in init\r\n");
			continue;
		}
		if (!pid) {								// 新的子进程
			close(0);close(1);close(2);
			setsid();							// 创建一新的会话期，见后面说明
			(void) open("/dev/tty0",O_RDWR,0);
			(void) dup(0);
			(void) dup(0);
			_exit(execve("/bin/sh",argv,envp));
		}
		while (1)
			if (pid == wait(&i))
				break;
		printf("\n\rchild %d died with code %04x\n\r",pid,i);
		sync();									// 同步操作，刷新缓冲区
	}
	_exit(0);	/* NOTE! _exit, not exit() */	/* 注意！是_exit()，非exit() */
// _exit()和exit()都用于正常终止一个函数。但_exit()直接是一个sys_exit系统调度，而
// exit()则通常是普通函数库中的一个函数。它会先执行一些清除操作，例如调用执行各
// 终止处理程序、关闭所有标准IO等，然后调用sys_exit。
}
