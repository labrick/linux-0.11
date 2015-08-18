!
!	setup.s		(C) 1991 Linus Torvalds
!
! setup.s is responsible for getting the system data from the BIOS,
! and putting them into the appropriate places in system memory.
! both setup.s and system has been loaded by the bootblock.
!
! This code asks the bios for memory/disk/other parameters, and
! puts them in a "safe" place: 0x90000-0x901FF, ie where the
! boot-block used to be. It is then up to the protected mode
! system to read them from there before the area is overwritten
! for buffer-blocks.
!
! setup.s负责从BIOS中获取系统数据，并将这些数据存放到内存的适当位置。
! 此时setup.s和system已经由bootsect引导块加载到内存中。
!
! 这段代码询问bios有关内存/磁盘/其他参数，并将这些参数放到一个
! “安全的”地方:0x90000-0x901ff，也即原来bootsect代码块曾经在
! 的地方，然后在被缓冲块覆盖掉之前由保护模式的system读取。

! NOTE! These had better be the same as in bootsect.s!
! 以下这些参数最好和bootsect.s中的相同！

INITSEG  = 0x9000	! we move boot here - out of the way	! 原来bootsect所处的段
SYSSEG   = 0x1000	! system loaded at 0x10000 (65536).		! system在0x10000(64KB)处
SETUPSEG = 0x9020	! this is the current segment			! 本程序所在的段地址

.globl begtext, begdata, begbss, endtext, enddata, endbss
.text
begtext:
.data
begdata:
.bss
begbss:
.text

entry start
start:

! ok, the read went well so we get current cursor position and save it for
! posterity.
! ok，整个读磁盘过程都很正常，现在保存光标位置以备今后使用。

! 这段代码使用BIOS中断取屏幕当前光标位置（列、行），并保存在内存0x90000处（2字节）。
! 控制台初始化程序会到此处读取该值。
! BIOS中断0x10功能号ah = 0x03，读光标位置。
! 输入：bh = 页号
! 返回：ch = 扫描开始线；cl = 扫描结束线；dh = 行号(0x00顶端)；dl = 列号(0x00最左端)
!
! 下句将ds置成INITSEG(0x9000)。这已经在bootsect程序中设置过，但是现在是setup程序，
! Linus觉得需要再重新设置一下。
	mov	ax,#INITSEG	! this is done in bootsect already, but...
	mov	ds,ax
	mov	ah,#0x03	! read cursor pos
	xor	bh,bh
	int	0x10		! save it in known place, con_init fetches
	mov	[0],dx		! it from 0x90000.

! Get memory size (extended mem, kB)
! 取扩展内存的大小值（KB）
! 利用BIOS中断0x15功能号ah = 0x88取系统所含扩展内存大小并保存在内存0x90002处。
! 返回：ax = 从0x100000(1M)处开始的扩展内存大小（KB）。若出错则CF置位，ax = 出错码。
	mov	ah,#0x88
	int	0x15
	mov	[2],ax		! 将扩展内存数值存在0x90002处（1个字节）

! Get video-card data:
! 下面这段用于取显示卡当前显示模式。
! 调用BIOS中断0x10，功能号ah = 0x0f
! 返回：ah = 字符列数；al = 显示模式；bh = 当前显示页。
! 0x90004(1个字节)存放当前页；0x90006存放显示模式；0x90007存放字符列数。
	mov	ah,#0x0f
	int	0x10
	mov	[4],bx		! bh = display page
	mov	[6],ax		! al = video mode, ah = window width

! check for EGA/VGA and some config parameters
! 检查显示方式（EGA/VGA）并去参数。
! 调用BIOS中断0x10，附加功能选择方式信息。功能号：ah = 0x12，bl = 0x10
! 返回：bh = 显示状态。0x00 - 显示模式，I/O端口=0x3dX；0x01 - 单色模式，I/O端口=0x3bX；
! bl = 安装的显示内存。0x00 - 64K；0x01 - 128K；0x02 - 192K；0x03 - 256K。
! cx = 显示卡特性参数（参见程序后对BIOS视频中断0x10的说明）。
	mov	ah,#0x12
	mov	bl,#0x10
	int	0x10
	mov	[8],ax		! 0x90008 = ??
	mov	[10],bx		! 0x9000A = 安装的显示内存，0x9000B = 显示状态（彩色/单色）
	mov	[12],cx		! 0x9000C = 显示卡特性参数。

! Get hd0 data
! 取第一个硬盘的信息（复制硬盘参数表）。
! 第1个硬盘参数表的首地址竟然是中断向量0x41的向量值（这是一个巧合？）！
! 而第2个硬盘参数表紧接着第1个表的后面，
! 中断向量0x46的向量值也指向第2个硬盘的参数表首址。表的长度是16个字节(0x10)。
! 下面两段程序分别复制BIOS有关两个硬盘的参数表，0x90080处存放第1个硬盘的表，0x90090处
! 存放第2个硬盘的表。

! 第69行语句从内存指定位置处读取一个长指针值并放入ds和si寄存器中。ds中放段地址，si是段内偏移地址。
! 这里是把内存地址4\*0x41(=0x104)处保存的4个字节(段和偏移值)读出。
	mov	ax,#0x0000
	mov	ds,ax
	lds	si,[4*0x41]				! 取中断向量0x41的值，也即hd0参数表的地址-->ds:si
	mov	ax,#INITSEG
	mov	es,ax
	mov	di,#0x0080				! 传输的目的地址：0x9000:0x0080 --> es:di
	mov	cx,#0x10				! 共传输16个字节
	rep
	movsb

! Get hd1 data

	mov	ax,#0x0000
	mov	ds,ax
	lds	si,[4*0x46]				! 取中断向量0x46的值，也即hd1参数表的地址-->ds:si
	mov	ax,#INITSEG
	mov	es,ax
	mov	di,#0x0090				！传输的目的地址：0x9000:0x0090 --> es:di
	mov	cx,#0x10
	rep
	movsb

! Check that there IS a hd1 :-)
! 检查系统是否有第2个硬盘。如果没有则把第2个表清零。
! 利用BIOS中断调用0x13的取盘类型功能，功能号ah = 0x15；
! 输入：dl = 驱动器号（0x8X是硬盘：0x80指第1个硬盘，0x81指第2个硬盘）
! 输出：ah = 类型码：	00 - 没有这个盘，CF置位；01 - 是软驱，没有change-line支持；
!						02 - 是软驱（或其他可移动设备），有change-line支持；03 - 是硬盘
	mov	ax,#0x01500
	mov	dl,#0x81
	int	0x13
	jc	no_disk1
	cmp	ah,#3			! 是硬盘吗？（类型 = 3 ？）
	je	is_disk1
no_disk1:
	mov	ax,#INITSEG		! 第2个硬盘不存在，则对第2个硬盘清零
	mov	es,ax
	mov	di,#0x0090
	mov	cx,#0x10
	mov	ax,#0x00
	rep
	stosb
is_disk1:

! now we want to move to protected mode ...
! 现在我们要进入保护模式了...

	cli			! no interrupts allowed !	! 从此开始不允许中断

! first we move the system to it's rightful place
! 首先我们将system模块移到正确的位置。
! bootsect引导程序是将system模块读入到0x10000（64KB）开始的位置。由于当时假设system模块
! 最大长度不会超过0x80000(512KB)，即其末端不会超过内存地址0x90000，所以bootsect会把自己
! 移动到0x90000开始的地方，并把setup加载到它的后面。下面这段程序的用途是再把整个system
! 模块移动到0x00000位置，即把从0x10000到0x8ffff的内存数据块（512KB）整块地向内存低端移动
! 了0x10000（64KB）的位置。

	mov	ax,#0x0000
	cld			! 'direction'=0, movs moves forward
do_move:
	mov	es,ax		! destination segment	! es:di是目的地址（初始为0x0:0x0）
	add	ax,#0x1000
	cmp	ax,#0x9000	! 已经把最后一段（0x8000段开始的64KB）代码移动完？
	jz	end_move	! 是，则跳转
	mov	ds,ax		! source segment		! ds:si是源地址（初始为0x1000:0x0）
	sub	di,di
	sub	si,si
	mov 	cx,#0x8000	! 移动0x8000字（64KB）,需要循环8次
	rep
	movsw
	jmp	do_move

! then we load the segment descriptors
! 此后，我们加载段描述符。
! 从这里开始会遇到32位保护模式的操作，因此需要Inter 32位保护模式编程方面的知识，有关
! 这方面的信息请查阅列表后的简单介绍或附录中的详细说明。这里仅作概要说明。在进入保护模式
! 中运行之前，我们需要首先设置好需要使用的段描述符表。这里需要设置全局描述符表和中断描述符表。
!
! 下面指令lidt用于加载中断描述符表（IDT）寄存器。他的操作数(idt_48)有6个字节。前2字节（字节0-1）
! 是描述符表的字节长度值；后4字节(字节2-5)是描述符表的32位线性基地址，其形式参见下面218-220行
! 和222-224行说明。中断描述符表中的每一个8字节表项指出发生中断时需要调用的代码信息。
! 与中断向量有些相似，但要包含更多的信息。
!
! lgdt指令用于加载全局描述符表(GDT)寄存器，其操作数格式与lidt指令的相同。全局描述符表中的
! 每个描述符项(8字节)描述了保护模式下数据段和代码段(块)的信息。其中包括段的最大程度限制(16位)、
! 段的线性地址基址(32位)、段的特权级、段是否在内存、读写特许权以及其他一些保护模式运行的标志。
end_move:
	mov	ax,#SETUPSEG	! right, forgot this at first. didn't work :-)
	mov	ds,ax			! ds指向本程序（setup）段
	lidt	idt_48		! load idt with 0,0						! 加载IDT寄存器
	lgdt	gdt_48		! load gdt with whatever appropriate	! 加载GDT寄存器

! that was painless, now we enable A20
! 以上的操作很简单，现在我们开启A20地址线。
! 为了能够访问和使用1MB以上的物理内存（A20只能访问到1MB），我们需要首先开启A20地址线。
! 参见本程序列表后有关A20信号线的说明。关于所涉及的一些端口和命令，可参考kernel/chr_drv/
! keyboard.S程序后对键盘接口的说明。至于机器是否真正开启了A20地址线，我们还需要在进入保护
! 模式之后（能访问1MB以上内存之后）在测试一下，这个工作放在了head.S程序中（32-36行）。
! 这一部分需要需要了解8042芯片：<http://labrick.github.io/>
	call	empty_8042	! 测试8042状态寄存器，等待输入缓冲器空
						! 只有当输入缓冲期为空时才可以对其执行写命令
	mov	al,#0xD1		! command write	! 0xD1命令码 - 表示要写数据到
	out	#0x64,al		! 8042的P2端口，P2端口的位1用于A20线的选通
						! 数据要写到0x60口
	call	empty_8042	! 等待输入缓冲器空，看命令是否被接受
	mov	al,#0xDF		! A20 on	! 选通A20地址现代额参数
	out	#0x60,al
	call	empty_8042	! 若此时输入缓冲器为空，则表示A20线已经选通

! well, that went ok, I hope. Now we have to reprogram the interrupts :-(
! we put them right after the intel-reserved hardware interrupts, at
! int 0x20-0x2F. There they won't mess up anything. Sadly IBM really
! messed this up with the original PC, and they haven't been able to
! rectify it afterwards. Thus the bios puts interrupts at 0x08-0x0f,
! which is used for the internal hardware interrupts as well. We just
! have to reprogram the 8259's, and it isn't fun.
!
! 希望以上一切正常。现在我们必须重新对中断进行编程 :-( 
! 我们将它们放在正好处于Intel保留的硬件中断后面，即int 0x20-0x2F。在那里它们不会引起冲突。
! 不幸的是IBM在原PC机种搞糟了，以后也没有纠正过来。所以PC机BIOS把中断放在了0x08-0x0f，
! 这些中断也被用于内部硬件中断。所以我们就必须重新对8259中断控制器进行编程，这一点都没意思。
!
! PC机使用2个8259A芯片，关于对可编程控制器8259A芯片的编程方法请参见本程序后的介绍。
! 第156行上定义的两个字（0x00eb）是直接使用机器码表示的两条相对跳转指令，起延时作用。
! 0xeb是直接近跳转指令的操作码，带1个字节的相对位移值。因此跳转范围是-127和127。
! CPU通过把这个相对位移值加到EIP寄存器中就形成一个新的有效地址。此时EIP指向下一条被执行的
! 指令。执行时所花费的CPU时钟周期数是7--10个。0x00eb表示跳转值是0的一条指令，因此还是
! 直接执行下一条指令。这两条指令共可提供14--20个CPU时钟周期的延迟时间。在as86中没有表示
! 相应指令的助记符，因此Linus在setup.s等一些汇编程序中就直接使用机器码表示这种指令。
! 另外，每个空操作指令NOP的时钟周期数是3个，因此若要达到时间的延迟效果就需要6--7个NOP指令
!
! 8259芯片主片端口是0x20-0x2，从片端口是0xA0-0xA1。输出值0x11表示初始化命令开始，它是
! ICW1命令字，表示边沿触发、多片8259级联、最后要发送ICW4命令字。
	mov	al,#0x11		! initialization sequence
	out	#0x20,al		! send it to 8259A-1				! 发送到8259A主芯片
	.word	0x00eb,0x00eb		! jmp $+2, jmp $+2			! '$'表示当前指令的地址
	out	#0xA0,al		! and to 8259A-2					! 再发送到8250A从芯片
	.word	0x00eb,0x00eb
! Linux系统硬件中断号被设置成从0x20开始。参见表2-2:硬件中断请求信号与中断号对应表
	mov	al,#0x20		! start of hardware int's (0x20)
	out	#0x21,al		! 送主芯片ICW2命令字，设置起始中断号，要送奇端口
	.word	0x00eb,0x00eb
	mov	al,#0x28		! start of hardware int's 2 (0x28)
	out	#0xA1,al		! 送从芯片ICW2命令字，从芯片的起始中断号。
	.word	0x00eb,0x00eb
	mov	al,#0x04		! 8259-1 is master
	out	#0x21,al		! 送主芯片ICW3命令字，主芯片的IR2连从芯片INT
						! 参见代码列表后的说明
	.word	0x00eb,0x00eb
	mov	al,#0x02		! 8259-2 is slave
	out	#0xA1,al		! 送从芯片ICW3命令字，表示从芯片的INT连到主芯片IR2引脚上
	.word	0x00eb,0x00eb
	mov	al,#0x01		! 8086 mode for both
	out	#0x21,al		! 送主片ICW4命令字。8086模式:普通EOI、非缓冲方式
						! 需发送指令来复位。初始化结束，芯片就绪
	.word	0x00eb,0x00eb
	out	#0xA1,al		! 送从芯片ICW4命令字，内容同上
	.word	0x00eb,0x00eb
	mov	al,#0xFF		! mask off all interrupts for now
	out	#0x21,al		! 屏蔽主芯片所有中断请求
	.word	0x00eb,0x00eb
	out	#0xA1,al		! 屏蔽从芯片所有中断请求

! well, that certainly wasn't fun :-(. Hopefully it works, and we don't
! need no steenking BIOS anyway (except for the initial loading :-).
! The BIOS-routine wants lots of unnecessary data, and it's less
! "interesting" anyway. This is how REAL programmers do it.
!
! Well, now's the time to actually move into protected mode. To make
! things as simple as possible, we do no register set-up or anything,
! we let the gnu-compiled 32-bit programs do that. We just jump to
! absolute address 0x00000, in 32-bit protected mode.
!
! 哼，上面这段编程当然没劲 :-(，但希望这样能工作，而且我们也不再需要乏味的BIOS
! 了（除了初始加载:-）。BIOS子程序要求很多不必要的数据，而且它一点都没趣。
! 那是“真正的”程序员所做的事。
!
! 好了，现在是真正开始进入保护模式的时候了。为了把事情做得尽量简单，我们并不对寄存器
! 内容进行任何设置。我们让gnu编译的32位程序去处理这些事。在进入32位保护模式时我们仅
! 是简单地跳转到绝对地址0x00000处。
!
!下面设置并进入32位保护模式运行。首先加载机器状态字（lmsw-load Machine Status Word），
! 也称控制寄存器CR0，其比特位0置1将导致CPU切换到保护模式，并且运行在特权级0中，即当前特权级
! CPL=0。此时段寄存器仍然指向与实地址模式中相同的线性地址处（在实地址模式下线性地址与物理内存
! 地址相同）。在设置该比特位后，随后一条指令必须是一条段间跳转指令以用于刷新CPU当前指令队列。
! 因为CPU是在执行一条指令之前就已从内存读取该指令并对其进行解码。然后在进入保护模式以后那些
! 属于实模式的预先取得的指令信息就开始变得不再有效。而一条段间跳转指令就会刷新CPU的当前指令
! 队列，即丢弃这些无效信息。另外，在Intel公司的手册上建议80386或以上CPU应该使用指令
! "mov cr0,ax"切换到保护模式，lmsw指令仅用于兼容以前的286 CPU。
	mov	ax,#0x0001	! protected mode (PE) bit		! 保护模式比特位(PE)
	lmsw	ax		! This is it!					! 就这样加载机器状态字！
	jmpi	0,8		! jmp offset 0 of segment 8 (cs)! 跳转至cs段偏移0处

! 我们已经将system模块移动到0x00000开始的地方，所以上句中的偏移地址是0。而段值8已经是保护
! 模式下的段选择符了，用于选择描述符表和描述符表项以及所要求的特权级。段选择符长度是16位
! (2字节)；位0-1表示请求的特权级0--3，Linux操作系统只用到两级：0级（内核级）和3级（用户级）；
! 位2用于选择全局描述符表（0）还是局部描述符表（1）；位3-15是描述符表项的索引，指出选择
! 第几项描述符。所以段选择符8（0b0000,0000,0000,1000）表示请求特权级0、使用全局描述符表GDT中
! 第2个段描述符项，该项指出代码的基地址是0（参见209行），因此这里的跳转指令就会去执行system
! 中的代码。

! This routine checks that the keyboard command queue is empty
! No timeout is used - if this hangs there is something wrong with
! the machine, and we probably couldn't proceed anyway.
! 下面这个子程序检查键盘命令队列是否为空。这里使用了超时方法 -
! 如果这里死机，则说明PC机有问题，我们就没有办法再处理下去了。
!
!只有当输入缓冲器为空（键盘控制器状态寄存器位1=0）才可以对其进行写命令。
empty_8042:
	.word	0x00eb,0x00eb			! 这是两个跳转指令的机器码（跳转到下一句），相当于延时操作
	in	al,#0x64	! 8042 status port			! 读AT键盘控制器状态寄存器。
	test	al,#2		! is input buffer full? ! 测试位1，输入缓冲器满？
	jnz	empty_8042	! yes - loop
	ret

! 全局描述符表开始处。描述符表由多个8字节长的描述符项组成。这里给出了3个描述符项。
! 第1项无用（206行），但须存在（WHY？？）。第2项是系统代码段描述符（208-211行），
! 第三项是系统数据段描述符（213-216行）
gdt:
	.word	0,0,0,0		! dummy			! 第1个描述符，不用

! 在GDT表中这里的偏移量是0x08。它是内核代码段选择符的值。
	.word	0x07FF		! 8Mb - limit=2047 (2048*4096=8Mb)
	.word	0x0000		! base address=0
	.word	0x9A00		! code read/exec		! 代码段为只读、可执行
	.word	0x00C0		! granularity=4096, 386 ! 颗粒度为4096，32位模式

! 在GDT表中这里的偏移量是0x10。它是内核数据段选择符的值。
	.word	0x07FF		! 8Mb - limit=2047 (2048*4096=8Mb)
	.word	0x0000		! base address=0
	.word	0x9200		! data read/write
	.word	0x00C0		! granularity=4096, 386

! 下面是加载中断描述符表寄存器idtr的指令lidt要求的6字节操作数。前2字节是IDT表的限长，后4字节
! 是idt表的线性地址空间中的32位基地址。CPU要求在进入保护模式之前需设置IDT表，因此这里先设置
! 一个长度为0的空表。
idt_48:
	.word	0			! idt limit=0
	.word	0,0			! idt base=0L

! 这里是加载全局描述符表寄存器gdtr的指令lgdt要求的6字节操作数。前2字节是gdt表的限长，后4字节
! 是gdt表的线性基地址。这里全局表长度设置为2KB(0x7ff即可)，因此每8字节组成一个段描述符项，
! 所以表中共可有256项。4字节的线性基地址位0x0009<<16 + 0x0200 + gdt，即0x90200 + gdt。
! （符号gdt是全局表在本程序段中的偏移地址，见205行。）
gdt_48:
	.word	0x800		! gdt limit=2048, 256 GDT entries
	.word	512+gdt,0x9	! gdt base = 0X9xxxx
	
.text
endtext:
.data
enddata:
.bss
endbss:
