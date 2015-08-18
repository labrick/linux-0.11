!
! SYS_SIZE is the number of clicks (16 bytes) to be loaded.
! 0x3000 is 0x30000 bytes = 196kB, more than enough for current
! versions of linux
! SYS_SIZE是要加载的系统模块长度，单位是节，16字节为1节。0x3000共为0x30000字节=196KB
!（若以1024字节为1KB计，则应该是192KB），对于当前的版本空间已足够了。
! 这里感叹号'!'或者分号';'表示程序注释语句的开始。
!
! 下面等号'='或符号'EQU'用于定义标识符或标号所代表的值，可称为符号常量。
! 这个常亮指明编译链接后system模块的大小。
! 这个等式“SYSSIZE = 0x3000”原来是由linux/Makefile中第92行上的语句动态自动产生。但从Linux 0.11版本开始就直接在这里给出了一个最大默认值。
! 原来的自动产生语句还没有被删除，参见程序5-1中第92行的说明。当该值为0x8000时，表示内核最大为512KB。
SYSSIZE = 0x3000
!
!	bootsect.s		(C) 1991 Linus Torvalds
!
! bootsect.s is loaded at 0x7c00 by the bios-startup routines, and moves
! iself out of the way to address 0x90000, and jumps there.
!
! It then loads 'setup' directly after itself (0x90200), and the system
! at 0x10000, using BIOS interrupts. 
!
! NOTE! currently system is at most 8*65536 bytes long. This should be no
! problem, even in the future. I want to keep it simple. This 512 kB
! kernel size should be enough, especially as this doesn't contain the
! buffer cache as in minix
!
! The loader has been made as simple as possible, and continuos
! read errors will result in a unbreakable loop. Reboot by hand. It
! loads pretty fast by getting whole sectors at a time whenever possible.
! 
! 以下是前面这些文字的翻译：
! 	bootsect.s		(C) 1991 Linus Torvalds 版权所有
! bootsect.s被bios-启动子程序加载至0x7c00(31KB)处，并将自己移到了地址
! 0x90000(576KB)处，并跳转至那里。
!
! 它然后使用BIOS终端将'setup'直接加载到自己的后面(0x90200)(576.5KB)
! 并将system加载到地址0x10000处。
!
! 注意：目前的内核系统最大长度限制为(8\*65536)(512KB)字节，即使是在将来这也应该没有问题。
! 我想让它保持简单明了。这样512KB的最大内核长度应该足够了，尤其是这里没有像minix
! 中一样包含缓冲区高速缓冲。
!
! 加载程序已经做得够简单了，所以持续的读出错将导致死循环，只能手工重启。
! 只要可能，通过一次读取所有的扇区，加载过程可以做得很快。
!
! 伪指令（伪操作符）.globl或.global用于定义随后的标识符是外部的或者全局的，并且即使不适用
! 也强制引入。.text、.data、和.bss用于分别定义当前代码段、数据段和未初始化数据段。
! 多个目标模块时，链接程序(ld86)会根据他们的类别把各个目标模块中的相应段分别组合(合并)
! 在一起。这里把三个段都定义在同一个重叠地址范围中，因此本程序实际上不分段。
! 另外，后面带冒号的字符串是标号，例如下面的'begtext:'。一条汇编语句通常由标号(可选)、
! 指令助记符(指令名)和操作数三个字段组成。标号位于一条指令的第一个字段。它代表其所在位置
! 的地址，通常指明一个跳转指令的目标位置。
.globl begtext, begdata, begbss, endtext, enddata, endbss
.text						# 文本段
begtext:
.data						# 数据段
begdata:
.bss						# 未初始化数据段（Block Started by Symbol）
begbss:
.text						# 文本段

SETUPLEN = 4				! nr of setup-sectors	setup程序的扇区数(setup-sectors)值
BOOTSEG  = 0x07c0			! original address of boot-sector	bootsect的原始地址(是段地址，下同)
INITSEG  = 0x9000			! we move boot here - out of the way	将bootsect移到这里--避开
SETUPSEG = 0x9020			! setup starts here		setup程序从这里开始
SYSSEG   = 0x1000			! system loaded at 0x10000 (65536).		system模块加载0x10000(64KB)处
ENDSEG   = SYSSEG + SYSSIZE		! where to stop loading		停止加载的段地址

! ROOT_DEV:	0x000 - same type of floppy as boot.	根文件系统设备使用与引导时同样的软驱设备
!		0x301 - first partition on first drive etc	根文件系统设备在第一个硬盘的第一个分区上，等等
ROOT_DEV = 0x306
! 设备号0x306指定根文件系统设备是第2个硬盘的第1个分区。当年Linus是在第2个硬盘上安装了Linux 0.11
! 系统，所以这里的ROOT_DEV被设置为0x306。在编译这个内核时你可以根据自己根文件系统所在位置修改
! 这个设备号。这个设备号是Linux系统老式的硬盘设备号命名方式，硬盘设备号具体值的含义如下：
! 设备号=主设备号\*256 + 次设备号（也即dev_no = (major<<8) + minor）
! （主设备号：1-内存，2-磁盘，3-硬盘，4-ttyx，6-并行口，7-非命名管道）
! 0x300 - /dev/hd0 - 带博鳌整个第1个硬盘
! 0x301 - /dev/hd1 - 第1个盘的第1个分区
! ...
! 0x304 - /dev/hd4 - 第1个盘的第4个分区
! 0x305 - /dev/hd5 - 代表整个第2个硬盘
! 0x306 - /dev/hd6 - 第2个盘的第1个分区
! ...
! 0x309 - /dev/hd9 - 第2个盘的第4个分区
! 从Linux内核0.95版后就已经使用与现在内核相同的命名方式了
 
! 伪指令entry迫使链接程序在生成的执行程序(a.out)中包含指定的标识符或标号。
! 47--56行作用是将自身(bootsect)从目前段位置0x7c0(31KB)移动到0x9000(576KB)处，共256字(512字节)，
! 然后跳转到移动后代码的go标号处，也即本程序的下一语句处。
entry start					! 告知链接程序，程序从start标号开始执行
start:
	mov	ax,#BOOTSEG			! 将ds段寄存器置为0x7c0
	mov	ds,ax
	mov	ax,#INITSEG			! 将es段寄存器置为0x9000
	mov	es,ax
	mov	cx,#256				! 设置移动计数值=256字
	sub	si,si				! 源地址 	ds:si = 0x7c0:0x000
	sub	di,di				! 目的地址	es:di = 0x9000:0x000
	rep						! 重复执行并递减cx的值，直到cx = 0为止
	movw					! 即movs指令,影响为0标志位。这里是内存[si]处移动cx个字到[di]处
							! 类似的还有movl(4字节移动),movb(1字节移动)
	jmpi	go,INITSEG		! 段间跳转(Jump Intersegment)。
							! 这里INITSEG指出跳转的段地址，标号go是段内偏移地址
							! 这里的go的数值是多少呢？？？？cs也会自动=INITSEG

! 从下面开始，CPU在已移动到0x90000位置处的代码中执行
! 这段代码设置几个段寄存器，包括栈寄存器ss和sp。栈指针sp只要指向大于512字节偏移
! （即地址0x90200）处都可以。因为从0x90200地址开始处还要放置setup程序，而此时setup程序大约为
! 4个扇区，因此sp要指向大于(0x90200 + 0x200 \* 4 + 堆栈大小)处。
! 实际上BIOS把引导扇区加载到0x7c00处并把执行权交给引导程序时，ss=0x00，因此必须设置堆栈。
go:	mov	ax,cs				! 将ds、es和ss都置成移动后代码所在的段处(0x9000)
	mov	ds,ax				! 由于程序中有栈操作(push,pop,call),因此必须设置堆栈
	mov	es,ax
! put stack at 0x9ff00.		! 将堆栈指针sp指向0x9ff00(即：0x9000:0xff00)处
							! 这里堆栈大小？？
	mov	ss,ax
	mov	sp,#0xFF00		! arbitrary value >>512

! load the setup-sectors directly after the bootblock.
! Note that 'es' is already set up.
! 在bootsect程序块后紧跟着加载setup模块的代码数据
! 注意es已经设置好了。（在移动代码时es已经指向了目的段地址处0x9000）

! 68--77行的用途是利用BIOS终端INT 0x13将setup模块从磁盘第2个扇区开始读到0x90200开始处。
! 共读4个扇区。如果读出错，则复位驱动器，并重试，没有退路。
! INT 0x13的使用方法如下：
! 读扇区：
! ah = 0x02 - 读磁盘扇区到内存；	al = 需要读出的扇区数量；
! ch = 磁道（柱面）号的低8位；		cl = 开始扇区（位0-5），磁道号高2位（位6-7）；
! dh = 磁头号；						dl = 驱动器号（如果是硬盘则位7要置位，位7是怎么规定的？？）；
! es:bx --> 指向数据缓冲区；如果出错则CF标志置位，ah中是出错码。
load_setup:
	mov	dx,#0x0000		! drive 0, head 0
	mov	cx,#0x0002		! sector 2, track 0
	mov	bx,#0x0200		! address = 512, in INITSEG
	mov	ax,#0x0200+SETUPLEN	! service 2, nr of sectors
						! BIOS已经把第一个扇区（也就是本程序）读到BOOTSEG，这里是接着读后四个扇区
	int	0x13			! read it
	jnc	ok_load_setup		! ok - continue
	mov	dx,#0x0000		! 对驱动器0进行读操作？？？
	mov	ax,#0x0000		! reset the diskette	执行子功能号为0的功能（是什么？？）
	int	0x13
	j	load_setup		! 即jmp指令

ok_load_setup:

! Get disk drive parameters, specifically nr of sectors/track
! 取磁盘驱动器的参数，特别是每道的扇区数量。
! 取磁盘驱动器参数INT 0x13调用格式和返回信息如下：
! ah = 0x08		dl = 驱动器号（如果是硬盘则要置位7为1）
! 返回信息：
! 如果出错则CF置位，并且ah = 状态码。
! ah = 0，al = 0，		bl = 驱动器类型（AT/PS2）
! ch = 最大磁道号的低8位，cl = 每磁道最大扇区数（位0-5），最大磁道号高2位（位6-7）
! dh = 最大磁头数，		dl = 驱动器数量
! es:di --> 软驱磁盘参数表
	mov	dl,#0x00
	mov	ax,#0x0800		! AH=8 is get drive parameters
	int	0x13
	mov	ch,#0x00
! 下面指令表示下一条语句的操作数在cs段寄存器所指的段中。它只影响其下一条语句。
! 实际上，由于本程序代码和数据都被设置处于同一个段中，即段寄存器cs和ds、es的值相同，
! 因此本程序中此处可以不适用该指令。
	seg cs
! 下句保存每磁道扇区数。对于软盘来说(dl=0)，其最大磁道号不会超过256，ch已经足够表示它，
! 因此cl的位6-7肯定为0。又86行已置ch=0，因此此时cx中是每磁道扇区数。
	mov	sectors,cx		! sectors定义在最后.word 0
	mov	ax,#INITSEG
	mov	es,ax			! 因为上面取磁盘参数中断改掉了es的值，这里重新改过来

! Print some inane message
! 显示信息：“‘Loading system ...'回车换行”，共显示包括回车和换行控制字符在内的24个字符
! BIOS中断0x10功能号ah = 0x03，读光标位置。
! 输出：bh = 页号
! 返回：ch = 扫描开始线；	cl = 扫描结束线；
! dh = 行号（0x00顶端）;	dl = 列号（0x00最左边）。
!
! BIOS中断0x10功能号ah = 0x13，显示字符串。
! 输入：al = 放置光标的方式及规定属性。0x01-表示使用bl中的属性值，光标停在字符串结尾处。
! es:bp 此寄存器对指向要显示的字符串起始位置处。cx = 显示的字符串字符数。
! bh = 显示页面号；	bl = 字符属性；	dl = 列号。
	mov	ah,#0x03		! read cursor pos
	xor	bh,bh			! 首先读光标位置。返回光标位置值在dx中。
	int	0x10			! dh=行（0--24）； dl=列（0--79）。供显示字符串用。
	
	mov	cx,#24			!共显示24个字符。
	mov	bx,#0x0007		! page 0, attribute 7 (normal)
	mov	bp,#msg1		! es:bp 寄存器对指向要显示的字符串
	mov	ax,#0x1301		! write string, move cursor
	int	0x10			! 写字符串并移动光标到串结尾处。

! ok, we've written the message, now
! we want to load the system (at 0x10000)
! 我们开始将system模块加载到0x10000（64KB）开始出
	mov	ax,#SYSSEG
	mov	es,ax		! segment of 0x010000	es = 存放system的段地址
	call	read_it	! 读磁盘上system模块，es为输入参数
	call	kill_motor	! 关闭驱动器马达，这样就可以知道驱动器的状态了

! After that we check which root-device to use. If the device is
! defined (!= 0), nothing is done and the given device is used.
! Otherwise, either /dev/PS0 (2,28) or /dev/at0 (2,8), depending
! on the number of sectors that the BIOS reports currently.
! 此后，我们检查要使用哪个根文件系统设备（简称根设备）。如果已经指定了设备（!=0）
! 就直接使用给定的设备。否则就需要根据BIOS报告的每磁盘扇区数来确定到底是用
! /dev/PS0(2,28)还是/dev/at0(2,8)。
!! 上面一行中两个设备的含义：
!! 在Linux中软驱的主设备号是2（参见第43行的注释），次设备号 = type\*4 + nr，其中
!! nr为0-3分别对应软驱A、B、C或D；type是软驱的类型（2-->1.2MB或7-->1.44MB等）。
!! 因为7\*4 + 0 = 28，所以/dev/PS0(2,28)指的是1.44MB A驱动器，其设备号是0x021c
!! 同理/dev/at0(2,8)指的是1.2MB A驱动器，其设备号是0x0208。
! 下面root_dev定义在引导扇区508,509字节处，指根文件系统所在设备号。0x0306指第2个
! 硬盘第1个分区。这里默认为0x0306是因为当时linus开发Linux系统时是在第2个硬盘第1个
! 分区中存放根文件系统。这个指需要根据你自己根文件系统所在硬盘和分区进行修改。
! 例如，如果你的根文件系统在第1个硬盘的第1个分区上，那么该值应该为0x0301
! 即(0x01,0x03)。如果根文件系统是在第2个Bochs软盘上，那么该值应该为0x021D
! 即(0x1D,0x02)。当编译内核时，你可以在Makefile文件中另行指定你自己的值，内核映像
! 文件Image的创建程序tools/build会使用你指定的值来设置你的根文件系统所在设备号。
	seg cs
	mov	ax,root_dev		! 去508,509字节处的根设备号并判断是否已被定义
	cmp	ax,#0
	jne	root_defined
! 取上面第88行保存的每磁道扇区数。如果sectors=15则说明是1.2MB的驱动器；
! 如果sectors=18，则说明是1.44MB软驱。因为是可引导的驱动器，所以肯定是A驱。
	seg cs
	mov	bx,sectors		! 上面已经保存了读回的每磁道扇区数
	mov	ax,#0x0208		! /dev/ps0 - 1.2Mb
	cmp	bx,#15			! 判断每磁道扇区数是否=15
	je	root_defined	! 如果相等，则ax中就是引导驱动器的设备号
	mov	ax,#0x021c		! /dev/PS0 - 1.44Mb
	cmp	bx,#18
	je	root_defined
undef_root:				! 如果都不一样，则死循环（死机）。
	jmp undef_root
root_defined:
	seg cs
	mov	root_dev,ax		! 将检查过的设备号保存在root_dev中

! after that (everyting loaded), we jump to
! the setup-routine loaded directly after
! the bootblock:
! 到此，所有程序都加载完毕，我们就跳转到被加载在bootsect后面的setup程序去。
! 段间跳转指令（Jump Intersegment）。跳转到0x9020:0000（setup.s程序开始处）去执行。
	jmpi	0,SETUPSEG	!!!! 到此本程序就结束了。!!!!

! This routine loads the system at address 0x10000, making sure
! no 64kB boundaries are crossed. We try to load it as fast as
! possible, loading whole tracks whenever we can.
!
! in:	es - starting address segment (normally 0x1000)
!
! 该子程序将系统模块加载到内存地址0x10000处，并确定没有跨越64KB的内存边界。
! 我们试图尽快地进行加载，只要可能，就每次加载整条磁道的数据。
! 输入:es - 开始内存地址段值（通常0x1000）
!
! 下面伪操作符.word定义一个2字节目标。相当于C语言程序中定义的变量和所占内存空间大小。
! '1+SETUPLEN'表示开始时已经读进1个引导扇区和setup程序所占的扇区数SETUPLEN。
sread:	.word 1+SETUPLEN	! sectors read of current track
head:	.word 0			! current head		! 标志当前所读的磁头
track:	.word 0			! current track		! 标志当前所读的磁道

read_it:
! 首先测试输入的段值。从盘上读入的数据必须存放在位于内存地址64KB的边界开始处，否则进入
! 死循环。清bx寄存器，用于表示当前段内存放数据的开始位置。
! 153行上的指定test以比特位逻辑与两个操作数。若两个操作数对应的比特位都为1，则结果值的
! 对应比特位为1，否则为0。该操作结果只影响标志（零标志ZF等）。例如，若AX=0x1000，那么
! test指令的执行结果是(0x1000 & 0x0fff) = 0x0000，于是ZF标志置位。此时即下一条指令
! jne条件不成立。
	mov ax,es		! 这里的es = 0x10000，我们怎么确定cs的值呢？？？？(seg是伪指令)
	test ax,#0x0fff
die:jne die			! es must be at 64kB boundary
	xor bx,bx		! bx is starting address within segment	! bx为段内偏移
rp_read:
! 接着判断是否已经读入全部数据。比较当前所读段是否就是系统数据末端所处的段（#ENDSEG），
! 如果不是就跳转至下面ok1_read标号处继续读数据。否则退出子程序返回。
	mov ax,es
	cmp ax,#ENDSEG		! have we loaded all yet?	! 是否已经加载了全部数据？
	jb ok1_read
	ret
ok1_read:
! 计算和验证当前磁道需要读取的扇区数，放在ax寄存器中。
! 根据当前磁道还未读取的扇区数以及段内数据字节开始偏移位置，计算如果全部读取这些未读扇区，
! 所读总字节数是否会超过64KB段长度的限制。若会超过，则根据此次最多能读入的字节数
! （64KB - 段内偏移位置），反算出此次需要读取的扇区数。
	seg cs
	mov ax,sectors		! 取每磁道扇区数
	sub ax,sread		! 减去当前磁道已读扇区数
	mov cx,ax			! cx = ax = 当前未读扇区数
	shl cx,#9			! cx = cx \* 512字节 + 段内当前偏移值(bx)
	add cx,bx			!    = 此次读操作后，段内共读入的字节数
	jnc ok2_read		! 若没有超过64KB字节，则跳转至ok2_read处执行
	je ok2_read
! 若加上此次将读磁道上所有未读扇区时会超过64KB，则计算此时最多能读入的字节数：
! (64KB - 段内读偏移位置)，再转换成需读取的扇区数。其中0减某数就是取该数64KB的补值。
	xor ax,ax
	sub ax,bx
	shr ax,#9
ok2_read:
! 读当前磁道上指定开始扇区(cl)和需读扇区数(al)的数据到es:bx开始处。然后统计当前磁道上
! 已经读取的扇区数并与磁道最大扇区数sectors作比较。如果小于sectors说明当前磁道上还有
! 扇区未读。于是跳转到ok3_read处继续操作。
	call read_track		! 读当前磁道上指定开始扇区和需读扇区数的数据
	mov cx,ax			! cx = 该次操作已读取的扇区数
	add ax,sread		! 加上当前磁道上已经读取的扇区数
	seg cs
	cmp ax,sectors		! 如果当前磁道上还有扇区未读，则跳转到ok3_read处
	jne ok3_read
! 若该磁道的当前磁头面所有扇区已经读取，则读该磁道的下一个磁头面(1号磁头) 上的数据。
! 如果已经完成，则去读下一个磁道。
	mov ax,#1
	sub ax,head			! 判断当前磁头号
	jne ok4_read		! 如果是0磁头，则再去读1磁头面上的扇区数据
	inc track			! 否则去读下一个磁道
ok4_read:
	mov head,ax			! 保存当前磁头号
	xor ax,ax			! 清当前磁道已读扇区数
ok3_read:
! 如果当前磁道上还有未读扇区，则首先保存当前磁道已读扇区数，然后调整存放数据处的开始位置。
! 若小于64KB边界值，则跳转到rp_read(56行)处，继续读数据。
	mov sread,ax		! 保存当前磁道已读扇区数
	shl cx,#9			! 上次已读扇区数\*512字节
	add bx,cx			! 调整当前段内数据开始位置
	jnc rp_read
! 否则说明已经读取64KB数据。此时调整当前段，为读下一段数据做准备
	mov ax,es
	add ax,#0x1000		! 将段基址调整为指向下一个64KB内存开始处
	mov es,ax
	xor bx,bx			! 清段内数据开始偏移值
	jmp rp_read			! 跳转至rp_read(156行)处，继续读数据

! read_track子程序。
! 读当前磁道上指定开始扇区和需读扇区数的数据到es:bx开始处。参见第67行下对BIOS磁盘读中断
! int 0x13, ah=2的说明。
! al - 需读扇区数；	es:bx - 缓冲区开始位置
read_track:
	push ax
	push bx
	push cx
	push dx
	mov dx,track		! 取当前磁道号
	mov cx,sread		! 取当前磁道上已读扇区数
	inc cx				! cl = 开始读扇区
	mov ch,dl			! ch = 当前磁道号
	mov dx,head			! 取当前磁头号
	mov dh,dl			! dh = 磁头号
	mov dl,#0			！dl = 驱动器号（为0表示当前A驱动器）
	and dx,#0x0100		! 磁头号不大于1
	mov ah,#2			! ah = 2，读磁盘扇区功能号
	int 0x13
	jc bad_rt			! 若出错，则跳转至bad_rt
	pop dx
	pop cx
	pop bx
	pop ax
	ret
! 读磁盘操作出错。则执行驱动器复位操作（磁盘中断功能号0），再跳转到read_track处重试
bad_rt:	mov ax,#0
	mov dx,#0
	int 0x13
	pop dx
	pop cx
	pop bx
	pop ax
	jmp read_track

/*
 * This procedure turns off the floppy drive motor, so
 * that we enter the kernel in a known state, and
 * don't have to worry about it later.
 */
/* 这个子程序用于关闭软驱的马达，这样我们进入内核后就能
 * 知道它所处的状态，以后也就无须担心它了。
 */
! 下面第235行上的值0x3f2是软盘控制器的一个端口，被成为数字输出寄存器(DOR)端口。它是
! 一个8位的寄存器，其位7--位4分别用于控制4个软驱(D--A)的开启与关闭。位3--位2用于
! 允许/禁止DMA和中断请求以及启动/复位软盘控制器FDC。位1--位0用于选择选择操作的软驱。
! 第236行上在al中设置并输出的0值，就是用于选择A驱动器，关闭FDC，禁止DMA和中断请求，
! 关闭马达。有关软驱控制卡编程的详细信息请参见kernel/blk_drv/floppy.c程序后面的说明。
kill_motor:
	push dx
	mov dx,#0x3f2			! 软驱控制卡的数字输出寄存器(DOR)端口，只写
	mov al,#0				! A驱动器，关闭FDC，禁止DMA和中断请求，关闭马达。
	outb					! 将al中的内容输出到dx指定的端口去
	pop dx
	ret

sectors:
	.word 0					! 存放当前启动软盘每磁道的扇区数

msg1:						! 调用BIOS中断显示的信息
	.byte 13,10				! 回车、换行的ASCII码
	.ascii "Loading system ..."
	.byte 13,10,13,10		! 共24个ASCII码字符

! 表示下面语句从地址508(0x1fc)开始，所以root_dev在启动扇区的第508开始的2个字节中。
.org 508
root_dev:
	.word ROOT_DEV			! 这里存放根文件系统所在设备号(init/main.c中会用)

! 下面是启动盘具有有效引导扇区的标志。仅供BIOS中的程序加载引导扇区时识别使用。
! 它必须位于引导扇区的最后两个字节中
boot_flag:
	.word 0xAA55

.text
endtext:
.data
enddata:
.bss
endbss:
