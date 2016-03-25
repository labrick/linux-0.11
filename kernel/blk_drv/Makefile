#
# Makefile for the FREAX-kernel block device drivers.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# FREAX内核块设备驱动程序的Makefile文件
# 注意！依赖关系是由'make dep'自动进行的，它也会去除原来的依赖信息。不要把你自己的
# 依赖关系信息放在这里，除非是特别文件的(也即不是一个.c文件的信息)
# (Linux最初的名字叫FREAX，后来被ftp.funet.fi的管理员改成linux这个名字)

AR	=gar 		# GNU的二进制文件处理程序，用于创建、修改以及从归档文件中抽取文件
AS	=gas		# GNU的汇编程序
LD	=gld		# GNU的连接程序
LDFLAGS	=-s -x	# 连接程序所用的参数，-s输出文件中省略所有符号信息。-x删除所有局部符号
CC	=gcc		# GNU C语言编译器
# 下一行是C编译程序选项。-Wall显示所有的警告信息；-o优化选项，优化代码长度和执行时间
# -fstrength-reduce优化循环执行代码，排除重复变量；-fomit-frame-pointer省略保存不必要
# 的框架指针；-fcombine-regs合并寄存器，减少寄存器类的使用；-finline-functions将所有简
# 单短小的函数代码嵌入调用程序中；-mstring-insns linus自己添加的优化选项，以后不再使用
CFLAGS	=-Wall -O -fstrength-reduce -fomit-frame-pointer -fcombine-regs \
	-finline-functions -mstring-insns -nostdinc -I../../include
# C前处理选项。-E只运行C前处理，对所有指定的C程序进行预处理并将处理结果输出到标准输出
# 设备或指定的输出文件中；-nostdinc -I../../include同前。
CPP	=gcc -E -nostdinc -I../../include

# 下面的规则指示make利用下面的命令将所有的.c文件编译生成.s汇编程序。该规则的命令指使
# gcc采用CFLAGS所指定的选项对C代码编译后不进行汇编就停止(-S)，从而产生与输入的各个
# C文件对应的汇编代码文件。默认情况下所产生的汇编程序文件名是原C文件名去掉.c而加上
# .s后缀。-o表示其后是输出文件的名称。其中$*.s(或$@)是自动目标变量，$<代表第一个先决
# 条件，这里即是复合条件*.c的文件。
# 下面这3个不同规则分别用于不同的操作要求。若目标是.s文件，而源文件是.c文件则会使用
# 第一个规则；若目录是.o，而源文件是.s，则使用第2个规则；若目标是.o文件而源文件是.c
# 文件，则可直接使用第3个规则。
.c.s:
	$(CC) $(CFLAGS) \
	-S -o $*.s $<
# 下面规则表示将所有.s汇编程序文件编译成.o目标文件。22行是实现该操作的具体命令
.s.o:
	$(AS) -c -o $*.o $<
.c.o:					# 类似上面，*.c文件-->*.o目标文件，不进行连接
	$(CC) $(CFLAGS) \
	-c -o $*.o $<

OBJS  = ll_rw_blk.o floppy.o hd.o ramdisk.o		# 定义目标文件变量OBJS

# 在有了先决条件OBJS后使用下面的命令连接成目标blk_drv.a库文件
# 命令行中的'rcs'是操作码和修饰标志(前面可加上'-')，放置次序任意。其中'r'是操作码，
# 说明需要执行的操作。'r'表示要把命令行末列出的目标文件插入(替换replacement)归档文件
# blk_drv.a中。'cs'是两个修饰标志，用于修饰具体操作行为。'c'表示当归档文件blk_drv.a
# 不存在时就创建这个文件。's'表示写进或更新文档文件中的目标文件索引。对一个归档文件
# 单独使用命令"ar s"等同于对一个归档文件执行命令ranlib
blk_drv.a: $(OBJS)
	$(AR) rcs blk_drv.a $(OBJS)
	sync

# 下面的规则用于清理工作。当执行'make clean'时，就会执行34--35行上的命令，去除所有编译
# 连接生成的文件。'rm'是文件删除命令，选项-f含义是忽略不存在的文件，并且不现实删除信息
clean:
	rm -f core *.o *.a tmp_make
	for i in *.c;do rm -f `basename $$i .c`.s;done

# 下面得目标或规则用于检查各文件之间的依赖关系。方法如下：
# 使用字符串编辑程序sed对Makefile文件(即是本文件)进行处理，输出为删除Makefile文件中
# '### Dependencies'行后面的所有行(下面从44开始的行)，并生成tmp_make临时文件(38行的
# 作用)。然后对kernel/blk_drv/目录下的每个C文件执行gcc预处理操作。
# -M标志告诉预处理器输出描述每个目标文件相关性的规则，并且这些规则符合make语法。
# 对于每一个源文件，预处理程序输出一个make规则，其结果形式是相应源程序文件的目标文件
# 名加上其依赖关系--该源文件中包含的所有头文件列表。把预处理结果都添加到临时文件
# tmp_make中，然后将该临时文件复制成新的Makefile文件。
dep:
	sed '/\#\#\# Dependencies/q' < Makefile > tmp_make
	(for i in *.c;do echo -n `echo $$i | sed 's,\.c,\.s,'`" "; \
		$(CPP) -M $$i;done) >> tmp_make
	cp tmp_make Makefile

### Dependencies:
floppy.s floppy.o : floppy.c ../../include/linux/sched.h ../../include/linux/head.h \
  ../../include/linux/fs.h ../../include/sys/types.h ../../include/linux/mm.h \
  ../../include/signal.h ../../include/linux/kernel.h \
  ../../include/linux/fdreg.h ../../include/asm/system.h \
  ../../include/asm/io.h ../../include/asm/segment.h blk.h 
hd.s hd.o : hd.c ../../include/linux/config.h ../../include/linux/sched.h \
  ../../include/linux/head.h ../../include/linux/fs.h \
  ../../include/sys/types.h ../../include/linux/mm.h ../../include/signal.h \
  ../../include/linux/kernel.h ../../include/linux/hdreg.h \
  ../../include/asm/system.h ../../include/asm/io.h \
  ../../include/asm/segment.h blk.h 
ll_rw_blk.s ll_rw_blk.o : ll_rw_blk.c ../../include/errno.h ../../include/linux/sched.h \
  ../../include/linux/head.h ../../include/linux/fs.h \
  ../../include/sys/types.h ../../include/linux/mm.h ../../include/signal.h \
  ../../include/linux/kernel.h ../../include/asm/system.h blk.h 
