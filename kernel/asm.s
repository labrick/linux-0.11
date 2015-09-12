/*
 *  linux/kernel/asm.s
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 * asm.s contains the low-level code for most hardware faults.
 * page_exception is handled by the mm, so that isn't here. This
 * file also handles (hopefully) fpu-exceptions due to TS-bit, as
 * the fpu must be properly saved/resored. This hasn't been tested.
 */
/*
 * asm.s程序中包括大部分的硬件故障(或出错)处理的底层次代码。页异常由内存管理程序mm处理，
 * 所以不在这里。此程序还处理(希望是这样)由于TS-位而造成的fpu异常，因此fpu必须正确地
 * 进行保护/恢复处理，这些还没有测试过。
 */

# 本代码文件主要涉及对intel保留中断int0--int16的处理(int17-int31留作今后使用)
# 以下是一些全局函数名的声明，其原型在traps.c中说明。
.globl _divide_error,_debug,_nmi,_int3,_overflow,_bounds,_invalid_op
.globl _double_fault,_coprocessor_segment_overrun
.globl _invalid_TSS,_segment_not_present,_stack_segment
.globl _general_protection,_coprocessor_error,_irq13,_reserved

# 下面这段程序处理无出错号的情况。参见图8-4(a)
# int0 - 处理被零除出错的情况。类型：错误(Fault)；错误号：无。
# 在执行DIV或IDIV指令时，若除数是0，CPU就会产生这个异常。当EAX(或AX、AL)容纳不了一个
# 合法除操作的结果时也会产生这个异常。20行上标号'_do_divide_error'实际上是Ｃ语言函数
# do_divide_error()编译后所生成模块中对应的名称。函数'do_divide_error'在traps.c中实现(97)
_divide_error:
	pushl $_do_divide_error		# 首先把将要调用的函数地址入栈
no_error_code:					# 这里是无错误号处理的入口处，见下面第55行等
	xchgl %eax,(%esp)			# _do_divide_error的地址 --> eax,eax被交换入栈
	pushl %ebx
	pushl %ecx
	pushl %edx
	pushl %edi
	pushl %esi
	pushl %ebp
	push %ds					# ！！16位的段寄存器入栈后也要占用4个字节
	push %es
	push %fs
	pushl $0		# "error code"	# 将数值0作为出错码入栈
	lea 44(%esp),%edx			# 取堆栈中原调用返回地址处堆栈指针位置，并压入堆栈(44代表从开始到最后压栈的字节数)
	pushl %edx
	movl $0x10,%edx				# 初始化段寄存器ds、es和fs，加载内核数据段选择符(不明白)
	mov %dx,%ds
	mov %dx,%es
	mov %dx,%fs
# 下行上的'*'号表示调用操作数指定地址处的函数，称为间接调用。这句的含义是调用引起本次
# 异常的C处理函数，例如do_divide_error()等。第40行是将堆栈指针加8相当于两次pop操作，
# 弹出(丢弃)最后入堆栈的两个C函数参数(32行和34行入栈的值)，让堆栈指针重新指向寄存器
# fs入栈处。
	call *%eax					# 间接调用，例如调用C函数do_divide_error()等
	addl $8,%esp				# 让堆栈指针重新指向寄存器fs入栈处
	pop %fs
	pop %es
	pop %ds
	popl %ebp
	popl %esi
	popl %edi
	popl %edx
	popl %ecx
	popl %ebx
	popl %eax					# 弹出原来eax中的内容
	iret

# int1 -- debug调试中断入口点。处理过程同上。类型：错误/陷阱(Fault/Trap)；错误号：无。
# 当EFLAGS中TF标志置位时而引发的中断。当发现硬件断点(数据：陷阱，代码：错误)；或者
# 开启了指令跟踪陷阱或任务交换陷阱，或者调试寄存器访问无效(错误)，CPU就会产生该异常。
_debug:
	pushl $_do_int3		# _do_debug C函数指针入栈。以下同
	jmp no_error_code

# int2 -- 非屏蔽中断调用入口点。类型：陷阱；错误号：无。
# 这是仅有的被赋予固定中断向量的硬件中断。每当接收到一个NMI信号，CPU内部就会产生中断
# 向量2，并执行标准中断应答周期，因此很节省时间。NMI通常保留为极为重要的硬件事件使用。
# 当CPU收到一个NMI信号并且开始执行其中中断处理过程时，随后所有的硬件中断都将被忽略。
_nmi:
	pushl $_do_nmi
	jmp no_error_code

# int3 -- 断点指令引起中断的入口点。类型：陷阱；无错误号。
# 由int 3指令引发的中断，与硬件中断无关。该指令通常由调试器插入被调试程序的代码中。
# 处理过程同_debug
_int3:
	pushl $_do_int3
	jmp no_error_code
# int4 -- 溢出出错处理中断入口点。类型：陷阱；无错误号。
# EFLAGS中OF标志置位时CPU执行INTO指令就会引发该中断。通常用于编译器跟踪算术计算溢出。
_overflow:
	pushl $_do_overflow
	jmp no_error_code

# int5 -- 边界检查出错中断入口点。类型：错误；无错误号。
# 当操作数在有效范围以外时引发的中断。当BOUND指令测试失败就会产生该中断。BOUND指令有
# 3个操作数，如果第1个不在另外两个之间，就产生异常5。
_bounds:
	pushl $_do_bounds
	jmp no_error_code

# int6 -- 无效操作指令出错中断入口点。类型：错误；无错误号。
# CPU执行机构检测到一个无效的操作码而引起的中断。
_invalid_op:
	pushl $_do_invalid_op
	jmp no_error_code

# int9 -- 协处理器段超过出错中断入口点。类型：放弃；无错误号。
# 该异常基本上等同于协处理器出错保护。因为在浮点指令操作数太大时，我们就有这个机会来
# 加载或保存超出数据段的浮点值。
_coprocessor_segment_overrun:
	pushl $_do_coprocessor_segment_overrun
	jmp no_error_code

# int15 -- 其他intel保留中断的入口点。
_reserved:
	pushl $_do_reserved
	jmp no_error_code

# int45 -- (=0x20 + 13)Linux设置的数学协处理器硬件中断。
# 当协处理器执行完一个操作时就会发出IRQ13中断信号，以通知CPU操作完成。80387在执行
# 计算时，CPU会等待其操作完成。下面88行上0xF0是协处理端口，用于请忙锁存器。通过写
# 该端口，本中断将消除CPU的BUSY延续信号，并重新激活80387的处理器扩展请求引脚PEREQ
# 该操作主要是为了确保在继续执行80387的任何指令之前，CPU响应本中断。
_irq13:
	pushl %eax
	xorb %al,%al			# 80387在执行计算时，CPU会等待其操作的完成。(????)
	outb %al,$0xF0
	movb $0x20,%al
	outb %al,$0x20			# 向8259主中断控制芯片发送EOI(中断结束)信号
	jmp 1f					# 这两个跳转指令起延时作用
1:	jmp 1f
1:	outb %al,$0xA0			# 再向8259从中断控制芯片发送EOI(中断结束)信号
	popl %eax
	jmp _coprocessor_error	# coprocessor_error原在本程序中，现已放到system_call.s中

# 以下中断在调用时CPU会在中断返回之后将错误号压入堆栈，因此返回时也需要将出错号弹出。

# int8 --双出错故障。类型：放弃；有错误码。
# 通常当CPU在调用前一个异常的处理程序而又检测到一个新的异常时，这两个异常会被串行地进行
# 处理，但也会碰到很少的情况，CPU不能进行这样的串行处理操作，此时就会引发该中断。
_double_fault:
	pushl $_do_double_fault	# C函数地址入栈(错误号什么压栈了？)
error_code:
	xchgl %eax,4(%esp)		# error code <-> %eax	eax原来的值被保存在堆栈上
	xchgl %ebx,(%esp)		# &function <-> %ebx	ebx原来的值被波存在堆栈上
	pushl %ecx
	pushl %edx
	pushl %edi
	pushl %esi
	pushl %ebp
	push %ds
	push %es
	push %fs
	pushl %eax			# error code	# 出错号入栈
	lea 44(%esp),%eax		# offset	# 程序返回地址处堆栈指针位置值入栈
	pushl %eax
	movl $0x10,%eax		# 置内核数据段选择符
	mov %ax,%ds
	mov %ax,%es
	mov %ax,%fs
	call *%ebx			# 间接调用，调用相应的C函数，其参数已入栈
	addl $8,%esp		# 丢弃入栈的2个用作C函数的参数
	pop %fs
	pop %es
	pop %ds
	popl %ebp
	popl %esi
	popl %edi
	popl %edx
	popl %ecx
	popl %ebx
	popl %eax
	iret

# int10 -- 无效的任务状态段(TSS)。类型：错误；有错误码。
# CPU企图切换到一个进程，而该进程的TSS无效。根据TSS中哪一部分引起了异常，当由于TSS长度
# 超过104字节时，这个异常在当前任务中产生，因而切换被终止。其他问题则会导致在切换后的
# 新任务中产生本异常。
_invalid_TSS:
	pushl $_do_invalid_TSS
	jmp error_code

# int11 -- 段不存在。类型：错误；有出错码。
# 被引用的段不在内存中。段描述符中标志着段不在内存中。
_segment_not_present:
	pushl $_do_segment_not_present
	jmp error_code

# int12 -- 堆栈段错误。类型：错误；有出错码。
# 指令操作试图超过堆栈范围，或者堆栈段不在内存中。这时异常11和13的特例。有些操作系统可以
# 这个异常来确定什么时候应该为程序分配更多的栈空间。
_stack_segment:
	pushl $_do_stack_segment
	jmp error_code

# int13 -- 一般保护性错误。类型：错误；有出错码。
# 表明是不属于任何其他类的错误。若一个异常产生时没有对应的处理向量(0--16)，通常就会
# 归到此类。
_general_protection:
	pushl $_do_general_protection
	jmp error_code
# int7 -- 设备不存在(_device_not_available)在kernel/system_call.s 148
# int14 -- 页错误(_page_fault)在mm/page.s 14
# int16 -- 协处理器错误(_coprocessor_error)在krnel/system_call.s 131
# 时钟中断int 0x20(_timer_interrupt)在kernel/system_call.s 176
# 系统调用int 0x80(_system_call)在kernel/system_call.s 80
