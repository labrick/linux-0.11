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
	mov	sectors,cx
	mov	ax,#INITSEG
	mov	es,ax

! Print some inane message

	mov	ah,#0x03		! read cursor pos
	xor	bh,bh
	int	0x10
	
	mov	cx,#24
	mov	bx,#0x0007		! page 0, attribute 7 (normal)
	mov	bp,#msg1
	mov	ax,#0x1301		! write string, move cursor
	int	0x10

! ok, we've written the message, now
! we want to load the system (at 0x10000)

	mov	ax,#SYSSEG
	mov	es,ax		! segment of 0x010000
	call	read_it
	call	kill_motor

! After that we check which root-device to use. If the device is
! defined (!= 0), nothing is done and the given device is used.
! Otherwise, either /dev/PS0 (2,28) or /dev/at0 (2,8), depending
! on the number of sectors that the BIOS reports currently.

	seg cs
	mov	ax,root_dev
	cmp	ax,#0
	jne	root_defined
	seg cs
	mov	bx,sectors
	mov	ax,#0x0208		! /dev/ps0 - 1.2Mb
	cmp	bx,#15
	je	root_defined
	mov	ax,#0x021c		! /dev/PS0 - 1.44Mb
	cmp	bx,#18
	je	root_defined
undef_root:
	jmp undef_root
root_defined:
	seg cs
	mov	root_dev,ax

! after that (everyting loaded), we jump to
! the setup-routine loaded directly after
! the bootblock:

	jmpi	0,SETUPSEG

! This routine loads the system at address 0x10000, making sure
! no 64kB boundaries are crossed. We try to load it as fast as
! possible, loading whole tracks whenever we can.
!
! in:	es - starting address segment (normally 0x1000)
!
sread:	.word 1+SETUPLEN	! sectors read of current track
head:	.word 0			! current head
track:	.word 0			! current track

read_it:
	mov ax,es
	test ax,#0x0fff
die:	jne die			! es must be at 64kB boundary
	xor bx,bx		! bx is starting address within segment
rp_read:
	mov ax,es
	cmp ax,#ENDSEG		! have we loaded all yet?
	jb ok1_read
	ret
ok1_read:
	seg cs
	mov ax,sectors
	sub ax,sread
	mov cx,ax
	shl cx,#9
	add cx,bx
	jnc ok2_read
	je ok2_read
	xor ax,ax
	sub ax,bx
	shr ax,#9
ok2_read:
	call read_track
	mov cx,ax
	add ax,sread
	seg cs
	cmp ax,sectors
	jne ok3_read
	mov ax,#1
	sub ax,head
	jne ok4_read
	inc track
ok4_read:
	mov head,ax
	xor ax,ax
ok3_read:
	mov sread,ax
	shl cx,#9
	add bx,cx
	jnc rp_read
	mov ax,es
	add ax,#0x1000
	mov es,ax
	xor bx,bx
	jmp rp_read

read_track:
	push ax
	push bx
	push cx
	push dx
	mov dx,track
	mov cx,sread
	inc cx
	mov ch,dl
	mov dx,head
	mov dh,dl
	mov dl,#0
	and dx,#0x0100
	mov ah,#2
	int 0x13
	jc bad_rt
	pop dx
	pop cx
	pop bx
	pop ax
	ret
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
kill_motor:
	push dx
	mov dx,#0x3f2
	mov al,#0
	outb
	pop dx
	ret

sectors:
	.word 0

msg1:
	.byte 13,10
	.ascii "Loading system ..."
	.byte 13,10,13,10

.org 508
root_dev:
	.word ROOT_DEV
boot_flag:
	.word 0xAA55

.text
endtext:
.data
enddata:
.bss
endbss:
