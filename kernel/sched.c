/*
 *  linux/kernel/sched.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 * 'sched.c' is the main kernel file. It contains scheduling primitives
 * (sleep_on, wakeup, schedule etc) as well as a number of simple system
 * call functions (type getpid(), which just extracts a field from
 * current-task
 */
/*
 * 'sched.c'是主要的内核文件。其中包括有关调度的基本函数(sleep_on、wakeup、schedule等)
 *  以及一些简单的系统调用函数(比如getpid()，仅从当前任务中获取一个字段)。
 */
// 下面是调度程序头文件。定义了任务结构task_struct、第1个初始任务的数据。还有一些以宏
// 的形式定义的有关描述符参数设置和获取的嵌入式汇编函数程序。
#include <linux/sched.h>
#include <linux/kernel.h>	// 内核头文件。含有一些内核常用的原型定义
#include <linux/sys.h>		// 系统调用头文件。含有72个系统调用C函数处理程序，以'sys_'开头
#include <linux/fdreg.h>	// 软驱头文件。含有软盘控制器参数的一些定义
#include <asm/system.h>		// 系统头文件。定义了设置或修改描述符/中断门等的嵌入式汇编宏
#include <asm/io.h>			// io头文件。定义硬件端口输入/输出宏汇编语句
#include <asm/segment.h>	// 段操作头文件。定义了有关段寄存器操作的嵌入式汇编函数

#include <signal.h>			// 信号头文件。定义信号符号常量，sigaction结构，操作函数原型。

// 该宏取信号nr在信号位图中对应位的二进制数值。信号编号1-32。比如信号5的位图数值等于1<<(5-1)
// = 16 = 00010000b
#define _S(nr) (1<<((nr)-1))
// 除了SIGKILL和SIGSTOP信号以外其他信号都是可阻塞的(...1011,1111,1110,1111,1111b)
#define _BLOCKABLE (~(_S(SIGKILL) | _S(SIGSTOP)))

// 内核调试函数。显示任务号nr的进程号、进程状态和内核堆栈空闲字节数(大约)
void show_task(int nr,struct task_struct * p)
{
	int i,j = 4096-sizeof(struct task_struct);

	printk("%d: pid=%d, state=%d, ",nr,p->pid,p->state);
	i=0;
	while (i<j && !((char *)(p+1))[i])	// 检测指定任务数据结构以后等于0的字节数
		i++;		// 这里我们可以看到i最大值就是j，也就是堆栈总大小减去一个结构体的大小
					// 这里存在一个问题：堆栈之后的内容也一直为0呢？？？
	printk("%d (of %d) chars free in kernel stack\n\r",i,j);
}

// 现实所有任务的任务号、进程号、进程状态和内核堆栈空间字节数(大约)
// NR_TASKS是系统能容纳的最大进程(任务)数量(64个)，定义在include/kernel/sched.h第4行
void show_stat(void)
{
	int i;

	for (i=0;i<NR_TASKS;i++)
		if (task[i])				// task在哪里定义的？？？
			show_task(i,task[i]);	// 罗列出所有进程的进程号、pid、state
}

// PC机8253定时芯片的输入时钟频率约为1.193180MHz。Linux内核希望定时器发出中断的频率是
// 100Hz，也即每100ms发出一次时钟中断。因此这里的LATCH是设置8253芯片的初值，参见407行
#define LATCH (1193180/HZ)	// HZ是个啥？？？

extern void mem_use(void);	// [??]没有任何地方定义和引用该函数。

extern int timer_interrupt(void);	// 时钟中断处理程序(kernel/system_call.s 176)
extern int system_call(void);		// 系统调用中断处理程序(kernel/system_call.s 80)

// 每个任务(进程)在内核态运行时都有自己的内核态堆栈。这里定义了任务的内核态堆栈结构
union task_union {				// 定义任务联合(任务结构成员和stack字符数组成员)
	struct task_struct task;	// 因为一个任务的数据结构与其内核态堆栈在同一内存页
	char stack[PAGE_SIZE];		// 中，所以从堆栈段寄存器ss可以获得其数据段选择符
};								// 如何理解？？？

static union task_union init_task = {INIT_TASK,};	// 定义初始任务的数据(sched.h 113)

// 从开机开始算起的滴答数时间值全局变量(10ms/滴答)。系统时钟中断每发生一次即一个滴答。
// 前面的限定符volatile，英语解释是易改变的、不稳定的意思。这个限定词的含义是向编译器
// 指明变量的内容可能会由于被其他程序修改而变化。通常在程序中申明一个变量时，编译器会
// 尽量把它存放在通用寄存器中，例如ebx、以提高访问效率。当CPU把其值放到ebx中后一般就
// 不会再关心该变量对应内存位置中的内容。若此时其他程序(例如内核程序或一个中断过程)
// 修改了内存中该变量的值，ebx中的值并不会随之更新。为了解决这种情况就创建了volatile
// 限定符，让代码在引用该变量时一定要从指定内存位置中取得其值。这里即是要求gcc不要对
// jiffies进行优化处理，也不要挪动位置，并且需要从内存中取其值。因为时钟中断处理过程
// 等程序会修改它的值。
long volatile jiffies=0;
long startup_time=0;		// 开机时间。从1970:0:0:0开始计时的秒数
struct task_struct *current = &(init_task.task);	// 当前任务指针(初始化指向任务0)
struct task_struct *last_task_used_math = NULL;		// 使用过协处理器任务的指针

struct task_struct * task[NR_TASKS] = {&(init_task.task), };	// 定义任务指针数组

// 定义用户堆栈，共1K项，容量4K字节。在内核初始化操作过程中被用作内核栈，初始化完成
// 之后将被用作任务0的用户态堆栈。在运行任务0之前它是内核栈，以后用作任务0和1的用户
// 态栈。下面结构用于设置堆栈ss:esp(数据段选择符，指针)，见head.s，23行。
// ss被设置为内核数据段选择符(0x10)，指针esp指在user_stack数组最后一项后面。这是因为
// Intel CPU执行堆栈操作时是先递减堆栈指针sp值，然后在sp指针处保存入栈内容。
long user_stack [ PAGE_SIZE>>2 ] ;	// PAGE_SIZE = 4096 = 4K

struct {
	long * a;
	short b;
	} stack_start = { & user_stack [PAGE_SIZE>>2] , 0x10 };
/*
 *  'math_state_restore()' saves the current math information in the
 * old math state array, and gets the new ones from the current task
 */
/*
 * 将当前协处理器内容保存到老协处理器状态数组中，并将当前任务的协处理器
 * 内容加载进协处理器。
 */
// 当任务被调度交换过以后，该函数用以保存原任务的协处理状态(上下文)并恢复新调度进来的
// 当前任务的协处理器执行状态。
void math_state_restore()
{
// 如果任务没变则返回(上一个任务就是当前任务)。这里"上一个任务"是指刚被交换出去的任务
	if (last_task_used_math == current)
		return;
	__asm__("fwait");
	if (last_task_used_math) {	// 到此处说明任务发生了变更，保存当前任务的tss
		__asm__("fnsave %0"::"m" (last_task_used_math->tss.i387));
	}
// 现在，last_task_used_math指向当前任务，以备当前任务被交换出去时使用。此时如果当前
// 任务用过协处理器，则恢复其状态。否则的话说明是第一次使用，于是就向协处理器发初始化
// 命令，并设置使用了协处理器状态。
	last_task_used_math=current;		// 这个赋值不会覆盖tss结构体？？？
	if (current->used_math) {
		__asm__("frstor %0"::"m" (current->tss.i387));
	} else {
		__asm__("fninit"::);		// 向协处理器发初始化命令
		current->used_math=1;		// 设置已使用过协处理器标志
	}
}

/*
 *  'schedule()' is the scheduler function. This is GOOD CODE! There
 * probably won't be any reason to change this, as it should work well
 * in all circumstances (ie gives IO-bound processes good response etc).
 * The one thing you might take a look at is the signal-handler code here.
 *
 *   NOTE!!  Task 0 is the 'idle' task, which gets called when no other
 * tasks can run. It can not be killed, and it cannot sleep. The 'state'
 * information in task[0] is never used.
 */
/*
 *  'schedule()'是调度函数。这是个很好的代码！没有任何理由对它进行修改，因为
 *  它可以在所有的环境下工作(比如能够对IO-边界处理很好的响应等)。只有一件事
 *  值得留意，那就是这里的信号处理代码。
 *
 *  注意！！任务0是个闲置'idle'任务)，只有当没有其他任务可以运行时才调用它。
 *  它不能被杀死，也不能睡眠。任务0中的状态信息'state'是从来不用的。
 */
void schedule(void)
{
	int i,next,c;
	struct task_struct ** p;		// 任务结构指针的指针

/* check alarm, wake up any interruptible tasks that have got a signal */
/* 检测alarm(进程的报警定时值)，唤醒任何已得到信号的可中断任务 */

// 从任务数组中最后一个任务开始循环检测alarm。在循环时跳过空指针项。
	for(p = &LAST_TASK ; p > &FIRST_TASK ; --p)
		if (*p) {
// 如果设置过任务的定时值alarm，并且已经过期(alarm<jiffies)，则在信号位图中置SIGALRM
// 信号，即向任务发送SIGALARM信号。然后清alarm。该信号的默认操作是终止进程。jiffies
// 是系统从开机开始算起的滴答数(10ms/滴答)。定义在sched.h第139行。
			if ((*p)->alarm && (*p)->alarm < jiffies) {
					(*p)->signal |= (1<<(SIGALRM-1));
					(*p)->alarm = 0;
				}
// 如果信号位图中除被阻塞的信号外还有其他信号，并且任务处于可中断状态，则置任务为就绪
// 状态。其中'~(_BLOCKABLE & (*p)->blocked)'用于忽略被阻塞的信号，但SIGKILL和SIGSTOP
// 不能被阻塞。
			if (((*p)->signal & ~(_BLOCKABLE & (*p)->blocked)) &&
			(*p)->state==TASK_INTERRUPTIBLE)
				(*p)->state=TASK_RUNNING;		// 置为就绪(可执行)状态
		}

/* this is the scheduler proper: */
/* 这里是调度程序的主要部分 */

	while (1) {
		c = -1;
		next = 0;
		i = NR_TASKS;
		p = &task[NR_TASKS];
// 这段代码也是从任务数组的最后一个任务开始循环处理，并跳过不含任务的数组槽。比较
// 每个就绪状态任务的counter(任务运行时间的递减滴答计数)值，哪一个大，运行时间还不
// 长，next就指向哪个的任务号。
// 也就是搜索一下哪个进程的counter最大，然后执行它，如果都为0，则执行任务0
		while (--i) {
			if (!*--p)
				continue;
			if ((*p)->state == TASK_RUNNING && (*p)->counter > c)
				c = (*p)->counter, next = i;
		}
// 如果比较得出有counter值不等于0的结果，或者系统中没有一个可运行的任务存在(此时c
// 仍然为-1，next=0)，则退出124行开始的循环，执行141行上的任务切换操作。否则就根据
// 每个任务的优先权值，更新每一个任务的counter值，然后回到125行重新比较。counter值
// 的计算方式为counter=counter/2 + priority。注意，这里计算过程不考虑进程的状态。
		if (c) break;
		for(p = &LAST_TASK ; p > &FIRST_TASK ; --p)
			if (*p)
				(*p)->counter = ((*p)->counter >> 1) +
						(*p)->priority;
	}
// 用下面宏(定义在sched.h中)把当前任务指针current指向任务号为next的任务，并切换
// 到该任务中运行。在126行next被初始化为0。因此若系统中没有任何其他任务可运行时，
// 则next始终为0。因此调度程序会在系统空闲时去执行任务0。此时任务0仅执行pause()
// 系统调用，并又会调用本函数。
	switch_to(next);		// 切换到任务号为next的任务，并运行之
}

//// pause()系统调用。转换当前任务的状态为可中断的等待状态，并重新调度。
// 该系统调用将导致进程进入睡眠状态，直到收到一个信号。该信号用于终止进程或者使进程
// 调用一个信号捕捉函数。只有当捕捉了一个信号，并且信号捕捉处理函数返回，pause()才
// 会返回。此时pause()返回值应该是-1，并且errno被置为EINTR。这里还没有完全实现
// (直到0.95版)
int sys_pause(void)
{
	current->state = TASK_INTERRUPTIBLE;
	schedule();
	return 0;
}

// 把当前任务置为不可中断的等待状态，并让睡眠队列头指针指向当前任务。
// 只有明确地唤醒时才会返回。该函数提供了进程与中断处理程序之间的同步机制。
// 函数参数p是等待任务队列头指针。指针是含有一个变量地址的变量。这里参数p是用了指针
// 的指针形式'**p'，这是因为C函数参数只能传值，没有直接的方式让被调用函数改变调用
// 该函数程序中变量的值。但是指针'*p'指向的目标(这里是任务结构)会改变，因此为了能修
// 改调用该函数程序中原来就是指针变量的值，就需要传递指针'*p'的指针，即'**p'。参见
// 图8-6中p指针的使用情况。
void sleep_on(struct task_struct **p)
{
	struct task_struct *tmp;

// 若指针无效，则退出。(指针所指的对象可以是NULL，但指针本身不应该为0)。另外，如果
// 当前任务是任务0，则死机。因为任务0的运行不依赖自己的状态，所以内核代码把任务0置
// 为睡眠状态毫无意义。
	if (!p)
		return;
	if (current == &(init_task.task))
		panic("task[0] trying to sleep");
// 让tmp指向已经在等待队列上的任务(如果有的话)，例如inode->i_wait。并且将睡眠队列
// 头的等待指针指向当前任务。这样就把当前任务插入到了*p的等待队列中。然后将当前任
// 务置为不可中断的等待状态(???)，并执行重新调度。
	tmp = *p;
	*p = current;		// current插入等待队列*p(??current有指向tmp??)
	current->state = TASK_UNINTERRUPTIBLE;
	schedule();
// 只有当这个等待任务被唤醒时，调度程序才又返回到这里，表示本进程已被明确地唤醒(就
// 绪态)。既然大家都在等待同样的资源，那么在资源可用时，就有必要唤醒所有等待该资源
// 的进程。该函数嵌套调用，也会嵌套唤醒所有等待该资源的进程。这里嵌套调用是指当一个
// 进程调用了sleep_on()后就会在该函数中被切换掉，控制权被转移到其他进程中。此时若有
// 进程也需要使用同一资源，那么也会使用同一个等待队列头指针作为参数调用sleep_on()函
// 数，并且也会"陷入"该函数而不会返回。只有当内核某处代码以队列头指针作为参数wake_up
// 了该队列，那么当系统切换去执行头指针所指的进程A时，该进程才会继续执行163行，把队
// 列后一个进程B置为就绪状态(唤醒)。而当轮到B进程执行时，它也才可能继续执行163行。
// 若它后面还有等待的进程C，那么它也会把C唤醒等。这里在163行前还应该添加一条语句：
// *p = tmp; 见183行上的注释。
	if (tmp)			// 若在其前还存在等待的任务，则也将其置为就绪状态(唤醒)
		tmp->state=0;
}

// 将当前任务置为可中断的等待状态，并放入*p指定的等待队列中
// sleep_on就是这个不执行了，调用schedule函数重新换个进程执行，等我可以了再回来
void interruptible_sleep_on(struct task_struct **p)
{
	struct task_struct *tmp;

// 若指针无效，则退出。(指针所指的对象可以是NULL，但指针本身不会为0)。如果当前任务
// 是任务0，则死机(impossible!)
	if (!p)
		return;
	if (current == &(init_task.task))
		panic("task[0] trying to sleep");
// 让tmp指向已经在等待队列上的任务(如果有的话)，例如inode->i_wait。并且将睡眠队列
// 头的等待指针指向当前任务。这样就把当前任务插入到了*p的等待队列中。然后将当前任
// 务置为可中断的等待状态(???)，并执行重新调度。
	tmp=*p;
	*p=current;
repeat:	current->state = TASK_INTERRUPTIBLE;
	schedule();
// 只有当这个队列任务被唤醒时，程序才又会返回到这里，表示进程已被明确地唤醒并执行。
// 如果等待队列中还有等待任务，并且队列头指针所指向的任务不是当前任务时，则将该等待
// 任务置为可运行的就绪状态，并重新执行调度程序。当指针*p所指向的不是当前任务时，表
// 示在当前任务被放入队列后，又有新的任务被插入等待队列前部。因此我们先唤醒它们，而
// 让自己仍然等待。等待这些后续进入队列的任务被唤醒执行时来唤醒本任务。于是去执行重
// 新调度。(机制还不太明白？？？？)
	if (*p && *p != current) {
		(**p).state=0;
		goto repeat;
	}
// 下面一句代码有误，应该是*p = tmp，让队列头指针指向其他等待任务，否则在当前任务之
// 前插入等待队列的任务均被抹掉了。当然，同时也需要删除192行上的语句。
	*p=NULL;
	if (tmp)
		tmp->state=0;
}

// 唤醒*p指向的任务。*p是任务等待队列头指针。由于新等待任务是插入在等待队列头指针
// 处的，因此唤醒的是最后进入等待的任务。
void wake_up(struct task_struct **p)
{
	if (p && *p) {
		(**p).state=0;		// 置为就绪(可运行)状态TASK_RUNNING
		*p=NULL;			// 简单的删除可以解决问题？？？？
	}
}

/*
 * OK, here are some floppy things that shouldn't be in the kernel
 * proper. They are here because the floppy needs a timer, and this
 * was the easiest way of doing it.
 */
/*
 * 好了，从这里开始是一些有关软盘的子程序，本不应该放在内核的主要部分中的。
 * 将它们放在这里是因为软驱需要定时处理，而放在这里是最方便的。
 */
// 下面第201 - 262行代码用于处理软驱定时。在阅读这段代码之前请先看一下块设备一章
// 中有关软盘驱动程序(floppy.c)后面的说明，或者到阅读软盘块设备驱动程序时在来看
// 这段代码。其中时间单位：1个滴答 = 1/100秒。
// 下面数组存放等待软驱马达启动到正常转速的进程指针。数组索引0-3分别对应软驱A-D
static struct task_struct * wait_motor[4] = {NULL,NULL,NULL,NULL};
// 下面数组分别存放各软驱马达启动所需要的滴答数。程序中默认启动时间为50个滴答(0.5秒)
static int  mon_timer[4]={0,0,0,0};
// 下面数组分别存放软驱在马达停转之前需维持的时间。程序中设定为10000个滴答(100秒)
static int moff_timer[4]={0,0,0,0};
// 对应软驱控制器中当前数字输出寄存器。该寄存器每位的定义如下：
// 位7-4：分别控制驱动器D-A马达的启动。1 - 启动；0 - 关闭；
// 位3：1 - 允许DMA和中断请求；0 - 禁止DMA和中断请求。
// 位2：1 - 启动软盘控制器；0 - 复位软盘控制器。
// 位1-0：00 - 11，用于选择控制的软驱A-D
unsigned char current_DOR = 0x0C;		// 这里设置初值为：允许DMA和中断请求、启动FDC

// 指定软驱启动到正常运转状态所需等待时间
// 参数nr -- 软驱号(0--3)，返回值为滴答数
// 局部变量selected是选中软驱标志(blk_drv/floppy.c 122)。mask是所选软驱对应的
// 数字输出寄存器中启动马达比特位。mask高4位是各软驱启动马达标志。
int ticks_to_floppy_on(unsigned int nr)
{
	extern unsigned char selected;
	unsigned char mask = 0x10 << nr;

// 系统最多有4个软驱。首先预先设置好指定软驱nr停转之前需要经过的时间(100秒)。然后
// 取当前DOR寄存器值到临时变量mask中，并把指定软驱的马达启动标志置位。
	if (nr>3)
		panic("floppy_on: nr>3");
	moff_timer[nr]=10000;		/* 100 s = very big :-) */	// 停转维持时间
	cli();			// 关中断	/* use floppy_off to turn it off */
	mask |= current_DOR;
	if (!selected) {
		mask &= 0xFC;
		mask |= nr;
	}
// 如果数字输出寄存器的当前值与要求的值不同，则向FDC数字输出端口输出新值(mask)，并且
// 如果要求启动的马达还没有启动，则置相应软驱的马达启动定时器值(HZ/2 = 0.5秒或50个
// 滴答)。若已经启动，则再设置启动定时为2个滴答，能满足下面do_floppy_timer()中先递减
// 后判断的要求。执行本次定时代码的要求即可。此后更新当前数字输出寄存器current_DOR。
	if (mask != current_DOR) {
		outb(mask,FD_DOR);
		if ((mask ^ current_DOR) & 0xf0)
			mon_timer[nr] = HZ/2;
		else if (mon_timer[nr] < 2)
			mon_timer[nr] = 2;
		current_DOR = mask;
	}
	sti();					// 开中断
	return mon_timer[nr];	// 最后返回启动马达所需的时间
}

// 等待指定软驱马达启动所需的一段时间，然后返回。
// 设置指定软驱的马达启动到正常转速所需的延时，然后睡眠等待。在定时中断过程中会一直
// 递减判断这里设定的延时值。当延时到期，就会唤醒这里的等待进程。
void floppy_on(unsigned int nr)
{
	cli();
// 如果马达启动定时还没到，就一直把当前进程设置为不可中断睡眠状态并放入等待马达运行
// 的队列中。
	while (ticks_to_floppy_on(nr))
		sleep_on(nr+wait_motor);
	sti();						// 开中断
}

// 置关闭相应软驱马达停转定时器(3秒)
// 若不是用该函数明确关闭指定的软驱马达，则在马达启动100秒之后也会被关闭。
void floppy_off(unsigned int nr)
{
	moff_timer[nr]=3*HZ;
}

// 软盘定时处理子程序。更新马达启动定时值和马达关闭停转计时值。该子程序会在时钟定时
// 中断过程中被调用，因此系统每经过一个滴答(10ms)就会被调用一次，随时更新马达开启或
// 停转定时器的值。如果某一个马达停转定时到，则将数字输出寄存器马达启动位复位。
void do_floppy_timer(void)
{
	int i;
	unsigned char mask = 0x10;

	for (i=0 ; i<4 ; i++,mask <<= 1) {
		if (!(mask & current_DOR))		// 如果不是DOR指定的马达则跳过
			continue;
		if (mon_timer[i]) {				// 如果马达启动定时到则唤醒进程
			if (!--mon_timer[i])
				wake_up(i+wait_motor);
		} else if (!moff_timer[i]) {	// 如果马达停转定时到则
			current_DOR &= ~mask;		// 复位相应马达启动位，并且
			outb(current_DOR,FD_DOR);	// 更新数字输出寄存器
		} else
			moff_timer[i]--;			// 否则马达停转计时递减
	}
}

// 下面是关于定时器的代码。最多可有64个定时器
#define TIME_REQUESTS 64

// 定时器链表结构和定时器数组。该定时器链表专用于供软驱关闭马达和启动马达定时操作。这种
// 类型定时器类似现代Linux系统中的动态定时器(Dynamic Timer)，仅供内核使用。
static struct timer_list {
	long jiffies;						// 定时滴答数
	void (*fn)();						// 定时处理程序
	struct timer_list * next;			// 链接指向下一个定时器
} timer_list[TIME_REQUESTS], * next_timer = NULL;	// next_timer是定时器队列头指针

// 添加定时器。输入参数为指定的定时值(滴答数)和相应的处理程序指针。
// 软盘驱动程序(floppy.c)利用该函数执行启动或关闭马达的延时操作。
// 参数jiffies - 以10毫秒计的滴答数；*fn() - 定时时间到时执行的函数。
void add_timer(long jiffies, void (*fn)(void))
{
	struct timer_list * p;

// 如果定时处理程序指针为空，则退出
	if (!fn)
		return;
	cli();
// 如果定时值<=0，则立即调用其他处理程序。并且该定时器不加入链表中
	if (jiffies <= 0)
		(fn)();
	else {
// 否则从定时器数组中，找一个空闲项
		for (p = timer_list ; p < timer_list + TIME_REQUESTS ; p++)
			if (!p->fn)
				break;
// 如果已经用完了定时器数组，则系统崩溃:-)。否则向定时器数据结构填入相应信息，并链入
// 链表头
		if (p >= timer_list + TIME_REQUESTS)
			panic("No more time requests free");
		p->fn = fn;
		p->jiffies = jiffies;
		p->next = next_timer;
		next_timer = p;
// 链表项按定时值从小到大排序。在排序时减去排在前面需要的滴答数，这样在处理定时器时
// 只要查看链表头的第一项的定时是否到期即可。[[?? 这段程序好像没有考虑周全。如果新
// 插入的定时器值小于原来头一个定时器值时则根本不会进入循环中，但此时还是应该将紧随
// 其后面的一个定时器值减去新的第1个的定时值。即如果第1个定时值<=第2个，则第2个定时
// 值扣除第1个的值即可，否则进入下面循环中进行处理。*********]]
		while (p->next && p->next->jiffies < p->jiffies) {
			p->jiffies -= p->next->jiffies;
			fn = p->fn;
			p->fn = p->next->fn;
			p->next->fn = fn;
			jiffies = p->jiffies;
			p->jiffies = p->next->jiffies;
			p->next->jiffies = jiffies;
			p = p->next;
		}
	}
	sti();
}

//// 时钟中断C函数处理程序，在system_call.s中的_timer_interrupt(176)被调用。参数
// cpl是当前特权级0或3，是时钟中断发生时正被执行的代码选择符中的特权级。cpl=0时
// 表示中断发生时正在执行内核代码；cpl=3时表示中断发生时正在执行用户代码。对于一
// 个进程由于执行时间片用完时，则进行任务切换。并执行一个计时更新工作。
void do_timer(long cpl)
{
	extern int beepcount;			// 扬声器发声时间滴答数(chr_drv/console.c 697)
	extern void sysbeepstop(void);	// 关闭扬声器(kernel/chr_drv/console.c 691)

// 如果发声计数次数到，则关闭发声。(向0x61口发送命令，复位位0和1。位0控制8253计数
// 器2的工作，位1控制扬声器)
	if (beepcount)
		if (!--beepcount)
			sysbeepstop();

// 如果当前特权级(cpl)为0(最高，表示是内核程序在工作)，则将内核代码运行时间stime
// 递增；[Linus把内核程序统称为超级用户(supervisor)的程序，见system_call.s 193上
// 的英文注释]。如果cpl>0，则表示是一般用户程序在工作，增加utime。
	if (cpl)
		current->utime++;
	else
		current->stime++;

// 如果有定时器存在，则将链表第1个定时器的值减1。如果已等于0，则调用相应的处理程序，
// 并将该处理程序指针置为空。然后去掉该项定时器。next_timer是定时器链表的头指针。
	if (next_timer) {
		next_timer->jiffies--;
		while (next_timer && next_timer->jiffies <= 0) {
			void (*fn)(void);			// 这里插入了一个函数指针定义!!!:-(???
			
			fn = next_timer->fn;
			next_timer->fn = NULL;
			next_timer = next_timer->next;
			(fn)();						// 调用处理函数
		}
	}
// 如果当前软盘控制器FDC的数字输出寄存器中马达启动位有置位的，则执行软盘定时程序(245)
	if (current_DOR & 0xf0)
		do_floppy_timer();
// 如果进程运行时间还没完，则退出。否则置当前任务运行计数值为0。并且若发生时钟中断时
// 正在内核代码中运行则返回，否则调用执行调度函数。
	if ((--current->counter)>0) return;
	current->counter=0;
	if (!cpl) return;			// 对于内核态程序，不依赖counter值进行调度(**)
	schedule();
}

// 系统调用功能 - 设置报警定时时间值(秒)
// 如果参数seconds大于0，则设置新定时值，并返回原定时时刻还剩余的间隔时间，否则返回0。
// 进程数据结构中报警定时值alarm的单位是系统滴答(1滴答为10ms)，它是系统开机起到设置定
// 时操作时系统滴答值jiffies和转换成滴答单位的定时值之和，即'jiffies + HZ*定时秒值'。
// 而参数给出的是以秒为单位的定时值，因此本函数的主要操作时进行两种单位的转换。
// 其中常数HZ = 100，是内核系统运行频率。定义在include/sched.h第5行上。
// 参数seconds是新的定时时间值，单位为秒。
int sys_alarm(long seconds)
{
	int old = current->alarm;

	if (old)
		old = (old - jiffies) / HZ;
	current->alarm = (seconds>0)?(jiffies+HZ*seconds):0;
	return (old);
}

int sys_getpid(void)
{
	return current->pid;
}

int sys_getppid(void)
{
	return current->father;
}

int sys_getuid(void)
{
	return current->uid;
}

int sys_geteuid(void)
{
	return current->euid;
}

int sys_getgid(void)
{
	return current->gid;
}

int sys_getegid(void)
{
	return current->egid;
}

int sys_nice(long increment)
{
	if (current->priority-increment>0)
		current->priority -= increment;
	return 0;
}

void sched_init(void)
{
	int i;
	struct desc_struct * p;

	if (sizeof(struct sigaction) != 16)
		panic("Struct sigaction MUST be 16 bytes");
	set_tss_desc(gdt+FIRST_TSS_ENTRY,&(init_task.task.tss));
	set_ldt_desc(gdt+FIRST_LDT_ENTRY,&(init_task.task.ldt));
	p = gdt+2+FIRST_TSS_ENTRY;
	for(i=1;i<NR_TASKS;i++) {
		task[i] = NULL;
		p->a=p->b=0;
		p++;
		p->a=p->b=0;
		p++;
	}
/* Clear NT, so that we won't have troubles with that later on */
	__asm__("pushfl ; andl $0xffffbfff,(%esp) ; popfl");
	ltr(0);
	lldt(0);
	outb_p(0x36,0x43);		/* binary, mode 3, LSB/MSB, ch 0 */
	outb_p(LATCH & 0xff , 0x40);	/* LSB */
	outb(LATCH >> 8 , 0x40);	/* MSB */
	set_intr_gate(0x20,&timer_interrupt);
	outb(inb_p(0x21)&~0x01,0x21);
	set_system_gate(0x80,&system_call);
}
