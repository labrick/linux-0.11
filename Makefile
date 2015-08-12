#
# if you want the ram-disk device, define this to be the
# size in blocks.
#
RAMDISK = #-DRAMDISK=512

AS86	=as86 -0 -a		# 8086汇编编译器和链接器
LD86	=ld86 -0		# -0 生成8086目标程序； -a 生成与gas和gld部分兼容的代码。

AS	=gas				#GNU汇编编译器和链接器
LD	=gld

# 下面是GNU链接器gld运行时用到的选项。含义是：-s 输出文件中省略所有的符号信息；-x 删除所有局部符号； -M 表示需要在标准输出设备（显示器）上打印链接映像（link map），是指由链接程序产生的一种内存地址映像，其中列出了程序段装入内存中的位置信息。具体来讲有如下信息：
# 1. 目标文件及符号信息映射到内存中的位置；
# 2. 公共符号如何设置；
# 3. 链接中包含的所有文件成员及引用的符号。
LDFLAGS	=-s -x -M

#gcc 是GNU C程序编译器。对于UNIX类的脚本（script）程序而言，在引用定义的标示符时，需在前面加上$符号并用括号括住标示符
CC	=gcc $(RAMDISK)

# 下面指定gcc使用的选项。前一行最后的'\'符号表示下一行是续行。
# 选项含义为：-Wall 打印所有警告信息；-0 对代码进行优化。'-f标志'指定与机器无关的编译标志。其中-fstrength-reduce用于优化循环语句；-fcombine-regs用于指明编译器在组合编译阶段把复制一个寄存器到另一个寄存器的指令组合在一起。-fomit-frame-pointer指明对于无需帧指针（Frame pointer）的函数不要把帧指针保留在寄存器中。这样在函数中可以避免对帧指针的操作和维护。-mstring-insns是Linus在学习gcc编译器时为gcc添加的选项，用于gcc-1.40在复制结构等操作时使用386 CPU的字符串指令，可以去掉。
CFLAGS	=-Wall -O -fstrength-reduce -fomit-frame-pointer \
-fcombine-regs -mstring-insns

# 下面cpp是gcc的前（预）处理器程序。前处理器用于进行程序中的宏替换处理、条件编译处理以及包含进指定文件的内容，即把使用'#include'指定的文件包含进来。源程序文件中所有以符号'#'开始的行均需要由前处理器进行处理。程序中所有的'#define'定义的宏都会使用其定义部分替换掉。程序中所有'#if'、'#ifdef'、'#ifndef'和'#endif'等条件判别行用于确定是否包含其指定范围中的语句。
# '-nostdinc -linclude'含义是不要搜索标准头文件目录中的文件，即不用系统/usr/include/目录下的头文件，而是使用'-I'选项指定的目录或者是在当前目录里搜索头文件。
CPP	=cpp -nostdinc -Iinclude

#
# ROOT_DEV specifies the default root-device when making the image.
# This can be either FLOPPY, /dev/xxxx or empty, in which case the
# default of /dev/hd6 is used by 'build'.
#
# ROOT_DEV指定在创建内核映像（image）文件时所使用的默认根文件系统所在的设备，
# 这里可以是软盘（FLOPPY）、/dev/xxxx或者干脆空着，空着时build程序（在tools/目录中）
# 就使用默认值/dev/hd6。
#
#这里/dev/hd6对应第2个硬盘的第1个分区。这是Linus开发Linux内核时自己的机器上根文件系统所在的分区位置。
ROOT_DEV=/dev/hd6

# 下面是kernel目录、mm目录、和fs目录所产生的目标代码文件。为了方便引用在这里将他们用ARCHIVES（归档文件）标示符表示。
ARCHIVES=kernel/kernel.o mm/mm.o fs/fs.o

# 块和字符设备库文件。
# '.a'表示该文件是个归档文件，也即包含有很多可执行二进制代码子程序集合的库文件，通常是用GNU的ar程序生成。ar是GNU的二进制文件处理程序，用于创建、修改以及从归档文件中抽取文件。
DRIVERS =kernel/blk_drv/blk_drv.a kernel/chr_drv/chr_drv.a
MATH	=kernel/math/math.a
LIBS	=lib/lib.a

# 下面是make老式的隐式后缀规则。该行指示make利用下面的命令将所有的'.c'文件编译生成'.s'汇编程序。':'表示下面是该规则的命令。整句表示让gcc采用前面CFLAGS所指定的选项以及仅使用include/目录中的头文件，在适当地编译后不进行汇编就停止（-S），从而产生与输入的各个C文件对应的汇编语言形式的代码文件。默认情况下所产生的汇编程序文件是原C文件名去掉'.c'后再加上'.s'后缀。'-o'表示其后是输出文件的形式。其中'$*.s'（或'$@'）是自动目标变量，'$<'代表第一个先决条件，这里即是符合条件'*.c'的文件。
# 下面这3个不同规则分别用于不同的操作要求。若目标是.s文件，而源文件是.c文件则会使用第一个规则；若目标是.o，而源文件是.s，则使用第2个规则；若目标是.o文件而原文件是c文件，则可直接使用第3个规则。
.c.s:
	$(CC) $(CFLAGS) \
	-nostdinc -Iinclude -S -o $*.s $<

# 表示将所有.s汇编程序文件编译成.o目标文件。整句表示使用gas编译器将汇编程序编译成.o目标文件。-c表示只编译或汇编，但不进行链接操作。
.s.o:
	$(AS) -c -o $*.o $<

# 类似上面，*.c文件 --> *.o目标文件。整句表示使用gcc将C语言文件编译成目标文件但不链接。
.c.o:
	$(CC) $(CFLAGS) \
	-nostdinc -Iinclude -c -o $*.o $<


# 下面'all'表示创建Makefile所知的最顶层的目标。这里即是Image文件。这里生成的Image文件即是引导启动盘映像文件bootimage。若将其写入软盘就可以使用该软盘引导Linux系统了。在Linux下将Image写入软盘的命令参见46行。DOS系统下可以使用软件rawrite.exe。
all:	Image

# 说明目标（Image文件）是由冒号后面的4个元素生成，分别是boot/目录中的bootsect和setup文件、tools/目录中的system和build文件。42--43行这是执行的命令。42行表示使用tools目录下的build工具程序（下面会说明如何生成）将bootsect、setup和system文件以$(ROOT_DEV)为根文件系统设备组装成内核映像文件Image。第43行的sync同步命令是迫使缓冲数据立即写盘并更新超级块。
Image: boot/bootsect boot/setup tools/system tools/build
	tools/build boot/bootsect boot/setup tools/system $(ROOT_DEV) > Image
	sync

# 表示disk这个目标要由Image产生。dd为UNIX标准命令：复制一个文件，根据选项进行转换和格式化。bs=表示一次读/写的字节数。if=表示输入的文件，of=表示输出到的文件。这里/dev/PS0是指第一个软盘驱动器（设备文件）。在Linux系统下使用/dev/fd0。
disk: Image
	dd bs=8192 if=Image of=/dev/PS0

tools/build: tools/build.c		# 由tools目录下的build.c程序生成执行程序build。
	$(CC) $(CFLAGS) \
	-o tools/build tools/build.c	# 编译生成执行程序build的命令。

boot/head.o: boot/head.s			# 利用上面给出的.s.o规则生成head.o目标文件。

# 表示tools目录下的system文件要由冒号右边所列的元素生成。56--61行是生成system的命令。最后的>System.map表示gld需要将链接映像重定向存放在System.map文件。关于System.map文件的用途参见注释后的说明。
tools/system:	boot/head.o init/main.o \
		$(ARCHIVES) $(DRIVERS) $(MATH) $(LIBS)
	$(LD) $(LDFLAGS) boot/head.o init/main.o \
	$(ARCHIVES) \
	$(DRIVERS) \
	$(MATH) \
	$(LIBS) \
	-o tools/system > System.map

# 数学协处理函数文件math.a由64行上的命令实现：进入kernel/math/目录：运行make工具程序。
kernel/math/math.a:
	(cd kernel/math; make)

kernel/blk_drv/blk_drv.a:		# 生成块设备库文件blk_drv.a，其中含有可重定位目标文件。
	(cd kernel/blk_drv; make)

kernel/chr_drv/chr_drv.a:		# 生成字符设备函数文件chr_drv.a。
	(cd kernel/chr_drv; make)

kernel/kernel.o:				# 内核目标模块kernel.o
	(cd kernel; make)

mm/mm.o:						# 内存管理模块mm.o
	(cd mm; make)

fs/fs.o:						# 文件系统目标模块fs.o
	(cd fs; make)

lib/lib.a:						# 库函数lib.a
	(cd lib; make)

boot/setup: boot/setup.s						# 这里开始的三行是使用8086汇编和连接器
	$(AS86) -o boot/setup.o boot/setup.s		# 对setup.s文件进行编译生成setup文件。
	$(LD86) -s -o boot/setup boot/setup.o		# -s选项表示要去除目标文件中的符号信息。

boot/bootsect:	boot/bootsect.s					# 同上。生成bootsect.o磁盘引导块。
	$(AS86) -o boot/bootsect.o boot/bootsect.s
	$(LD86) -s -o boot/bootsect boot/bootsect.o

# 下面92--95行的作用是在bootsect.s文本程序开始处添加一行有关system模块文件长度信息，在把system模块加载到内存期间用于指明系统模块的程度。添加该行信息的方法是首先生成只含有“SYSSIZE = system文件实际长度”一行信息的tmp.s文件，然后将bootsect.s文件添加在其后。取得system长度的方法是：首先利用命令ls对编译生成的system模块文件进行长列表显示，用grep命令取得列表行上文件字节数字段信息，并定向保存在tmp.s临时文件中。cut命令用于剪切字符串，tr用于去除行尾的回车符。其中：（实际长度+15）/16用于获得用‘节’表示的长度信息，1节=16字节。
# 注意：这是Linux 0.11之前的内核版本（0.01--0.10）获取system模块长度并添加到bootsect.s程序中使用的方法。从0.11版内核开始已不使用这个方法，而是直接在bootsect.s程序开始处给出了system模块的一个最大默认长度值。因此这个规则现在已经不起作用。
tmp.s:	boot/bootsect.s tools/system
	(echo -n "SYSSIZE = (";ls -l tools/system | grep system \
		| cut -c25-31 | tr '\012' ' '; echo "+ 15 ) / 16") > tmp.s
	cat boot/bootsect.s >> tmp.s

# 当执行'make clean'时，就会执行98--103行上的命令，去除所有编译链接生成的文件。'rm'是文件删除命令，选项-f含义是忽略不存在的文件，并且不显示删除信息。
clean:
	rm -f Image System.map tmp_make core boot/bootsect boot/setup
	rm -f init/*.o tools/system tools/build boot/*.o
	(cd mm;make clean)
	(cd fs;make clean)
	(cd kernel;make clean)
	(cd lib;make clean)

# 该规则将首先执行上面的clean规则，然后对linux/目录进行压缩，生成'backup.Z'压缩文件。'cd ..'表示退到linux/的上一级（父）目录：'tar cf -linux'表示对linux/目录执行tar归档程序。'-cf'表示需要创建新的归档文件'| compress -'表示将tar程序的执行通过管道操作（'|'）传递给压缩程序compress，并将压缩程序的输出存在backup.Z文件。
backup: clean
	(cd .. ; tar cf - linux | compress - > backup.Z)
	sync				# 迫使缓冲块数据立即写盘并更新磁盘超级块。

# 该目标或规则用于产生各文件之间的依赖关系。创建这些依赖关系是为了让make命令用它们来确定是否需要重建一个目标对象。比如当某个头文件被改动过后，make就能通过生成的依赖关系，重新编译与该头文件有关的所有*.c文件。具体方法如下：
# 使用字符串编辑程序sed对Makefile文件（这里即是本文件）进行处理，输出为删除了Makefile文件中'### Dependencies'行后面的所有行，即删除了下面从118开始到文件末的所有行，并生成一个临时文件tmp_make（也即110行的作用）。然后对指定目录下（init/）的每一个C文件（其实只有一个文件main.c）执行gcc预处理操作。标志'-M'告诉预处理程序cpp输出描述每个目标文件相关性的规则，并且这些规则符合make语法。对于每一个源文件，预处理程序会输出一个规则，其结果形式就是相应源程序文件的目标文件名加上其依赖关系，即该源文件中包含的所有头文件列表。然后把预处理结果都添加到临时文件tmp_make中，最后将该临时文件复制成新的Makefile文件。111行上的'$$i'实际上是'$($i)'。这里'$i'是这句前面的shell变量'i'的值。
dep:
	sed '/\#\#\# Dependencies/q' < Makefile > tmp_make		# q是退出的意思，这个sed意思是符合单引号中内容则退出，否则就复制
	(for i in init/*.c;do echo -n "init/";$(CPP) -M $$i;done) >> tmp_make
	cp tmp_make Makefile
	(cd fs; make dep)
	(cd kernel; make dep)
	(cd mm; make dep)

### Dependencies:		# 就是说init/main.c文件包含了下面的这些头文件，如果这些头文件发生了变化，则重新编译init/main.c文件，而且下面是由make自动生成的，cpp预处理操作可以做到这一点。
init/main.o : init/main.c include/unistd.h include/sys/stat.h \
  include/sys/types.h include/sys/times.h include/sys/utsname.h \
  include/utime.h include/time.h include/linux/tty.h include/termios.h \
  include/linux/sched.h include/linux/head.h include/linux/fs.h \
  include/linux/mm.h include/signal.h include/asm/system.h include/asm/io.h \
  include/stddef.h include/stdarg.h include/fcntl.h 
