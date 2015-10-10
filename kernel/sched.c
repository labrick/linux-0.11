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
// 则next时钟为0。因此调度程序会在系统空闲时去执行任务0。此时任务0仅执行pause()
// 系统调用，并又会调用本函数。
	switch_to(next);		// 切换到任务号为next的任务，并运行之
}

int sys_pause(void)
{
	current->state = TASK_INTERRUPTIBLE;
	schedule();
	return 0;
}

void sleep_on(struct task_struct **p)
{
	struct task_struct *tmp;

	if (!p)
		return;
	if (current == &(init_task.task))
		panic("task[0] trying to sleep");
	tmp = *p;
	*p = current;
	current->state = TASK_UNINTERRUPTIBLE;
	schedule();
	if (tmp)
		tmp->state=0;
}

void interruptible_sleep_on(struct task_struct **p)
{
	struct task_struct *tmp;

	if (!p)
		return;
	if (current == &(init_task.task))
		panic("task[0] trying to sleep");
	tmp=*p;
	*p=current;
repeat:	current->state = TASK_INTERRUPTIBLE;
	schedule();
	if (*p && *p != current) {
		(**p).state=0;
		goto repeat;
	}
	*p=NULL;
	if (tmp)
		tmp->state=0;
}

void wake_up(struct task_struct **p)
{
	if (p && *p) {
		(**p).state=0;
		*p=NULL;
	}
}

/*
 * OK, here are some floppy things that shouldn't be in the kernel
 * proper. They are here because the floppy needs a timer, and this
 * was the easiest way of doing it.
 */
static struct task_struct * wait_motor[4] = {NULL,NULL,NULL,NULL};
static int  mon_timer[4]={0,0,0,0};
static int moff_timer[4]={0,0,0,0};
unsigned char current_DOR = 0x0C;

int ticks_to_floppy_on(unsigned int nr)
{
	extern unsigned char selected;
	unsigned char mask = 0x10 << nr;

	if (nr>3)
		panic("floppy_on: nr>3");
	moff_timer[nr]=10000;		/* 100 s = very big :-) */
	cli();				/* use floppy_off to turn it off */
	mask |= current_DOR;
	if (!selected) {
		mask &= 0xFC;
		mask |= nr;
	}
	if (mask != current_DOR) {
		outb(mask,FD_DOR);
		if ((mask ^ current_DOR) & 0xf0)
			mon_timer[nr] = HZ/2;
		else if (mon_timer[nr] < 2)
			mon_timer[nr] = 2;
		current_DOR = mask;
	}
	sti();
	return mon_timer[nr];
}

void floppy_on(unsigned int nr)
{
	cli();
	while (ticks_to_floppy_on(nr))
		sleep_on(nr+wait_motor);
	sti();
}

void floppy_off(unsigned int nr)
{
	moff_timer[nr]=3*HZ;
}

void do_floppy_timer(void)
{
	int i;
	unsigned char mask = 0x10;

	for (i=0 ; i<4 ; i++,mask <<= 1) {
		if (!(mask & current_DOR))
			continue;
		if (mon_timer[i]) {
			if (!--mon_timer[i])
				wake_up(i+wait_motor);
		} else if (!moff_timer[i]) {
			current_DOR &= ~mask;
			outb(current_DOR,FD_DOR);
		} else
			moff_timer[i]--;
	}
}

#define TIME_REQUESTS 64

static struct timer_list {
	long jiffies;
	void (*fn)();
	struct timer_list * next;
} timer_list[TIME_REQUESTS], * next_timer = NULL;

void add_timer(long jiffies, void (*fn)(void))
{
	struct timer_list * p;

	if (!fn)
		return;
	cli();
	if (jiffies <= 0)
		(fn)();
	else {
		for (p = timer_list ; p < timer_list + TIME_REQUESTS ; p++)
			if (!p->fn)
				break;
		if (p >= timer_list + TIME_REQUESTS)
			panic("No more time requests free");
		p->fn = fn;
		p->jiffies = jiffies;
		p->next = next_timer;
		next_timer = p;
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

void do_timer(long cpl)
{
	extern int beepcount;
	extern void sysbeepstop(void);

	if (beepcount)
		if (!--beepcount)
			sysbeepstop();

	if (cpl)
		current->utime++;
	else
		current->stime++;

	if (next_timer) {
		next_timer->jiffies--;
		while (next_timer && next_timer->jiffies <= 0) {
			void (*fn)(void);
			
			fn = next_timer->fn;
			next_timer->fn = NULL;
			next_timer = next_timer->next;
			(fn)();
		}
	}
	if (current_DOR & 0xf0)
		do_floppy_timer();
	if ((--current->counter)>0) return;
	current->counter=0;
	if (!cpl) return;
	schedule();
}

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
