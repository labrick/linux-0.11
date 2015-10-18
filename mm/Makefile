CC	=gcc		# GNU C语言编译器
CFLAGS	=-O -Wall -fstrength-reduce -fcombine-regs -fomit-frame-pointer \
	-finline-functions -nostdinc -I../include
# C编译程序选项。-Wall显示所有的警告信息；-0优化选项，优化代码长度和执行时间；
# -fstrength-reduce优化循环执行代码，排除重复变量；-fomit-frame-pointer省略保存不必要
#  的框架指针；-fcombine-regs合并寄存器，减少寄存器类的使用；-finline-function将所有
#  简单短小的函数代码嵌入调用程序中；-nostdinc -I../include不使用默认路径中的包含文件，
#  而使用这里指定目录中的(../include)。
AS	=gas		# GNU的汇编程序
AR	=gar		# GNU的二进制文件处理程序，用于创建、修改以及从文档文件中抽取文件
LD	=gld		# GNU的连接程序
CPP	=gcc -E -nostdinc -I../include
# C前处理选项。-E只运行C前处理，对所有指定的C程序进行预处理并将处理结果输出到标准输出
# 设备或指定的输出文件中；-nostdinc -I../include同前。

# 下面的规则指示make利用下面的命令将所有的.c文件编译生成.s汇编程序。该规则的命令指使
# gcc采用CFLAGS所指定的选项对C代码编译后不进行汇编就停止(-S)，从而产生与输入的各个C
# 文件对应的汇编代码文件。默认情况下所产生的汇编程序文件名是原C文件名去掉.c而加上.s
# 后缀。-o表示其后是输出文件的名称。其中$*.s(或$@)是自动目标变量，$<代表第一个先决
# 条件，这里即是符合条件*.c的文件。
# 下面这3个不同规则分别用于不同的操作要求。若目标是.s文件，而源文件是.c文件则会使用
# 第一个规则；若目标是.o，而源文件是.s，则使用第2个规则；若目标是.o文件而源文件是.c
# 文件，则可直接使用第3个规则。
.c.o:
	$(CC) $(CFLAGS) \
	-c -o $*.o $<
# 下面规则表示将所有.s汇编程序文件编译成.o目标文件。22行是实现该操作的具体命令
.s.o:
	$(AS) -o $*.o $<
.c.s:					# 类似上面，*.c文件 -> *.s汇编程序文件。不进行连接。
	$(CC) $(CFLAGS) \
	-S -o $*.s $<

OBJS	= memory.o page.o	# 定义目标文件变量OBJS

all: mm.o

# 在有了先决条件OBJS后使用下面的命令连接成目标mm.o
# 选项'-r'用于指示生成可重定位的输出，即产生可以作为链接器ld输入的目标文件
mm.o: $(OBJS)
	$(LD) -r -o mm.o $(OBJS)

# 下面的规则用于清理工作。当执行'make clean'时，就会执行26-27行上的命令，去除所有编译
# 连接生成的文件。'rm'是文件删除命令，选项-f含义是忽略不存在的文件，并且不显示删除信息
clean:
	rm -f core *.o *.a tmp_make
	for i in *.c;do rm -f `basename $$i .c`.s;done

# 下面的目标或规则用于检查各文件之间的依赖关系。方法如下：
# 使用字符串编辑程序sed对Makefile文件(这里即自己)进行处理，输出为删除Makefile文件中
# '### Dependencies'行后面的所有行(下面从35开始的行)，并生成tmp_make临时文件(30行的
# 作用)。然后对mm/目录下的每一个C文件执行gcc预处理操作。
# -M标志告诉预处理程序输出描述每个目标我呢间相关性的规则，并且这些规则符合make语法。
# 对于每一个源文件，预处理程序输出一个make规则，其结果形式是相应源程序文件的目标文
# 件名加上其依赖关系--该源文件中包含的所有头文件列表。把预处理结果都添加到临时文件
# tmp_make中，然后将该临时文件复制成新的Makefile文件。
dep:
	sed '/\#\#\# Dependencies/q' < Makefile > tmp_make
	(for i in *.c;do $(CPP) -M $$i;done) >> tmp_make
	cp tmp_make Makefile

### Dependencies:
memory.o : memory.c ../include/signal.h ../include/sys/types.h \
  ../include/asm/system.h ../include/linux/sched.h ../include/linux/head.h \
  ../include/linux/fs.h ../include/linux/mm.h ../include/linux/kernel.h 
