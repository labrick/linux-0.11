#ifndef _TIMES_H
#define _TIMES_H

#include <sys/types.h>

struct tms {
	time_t tms_utime;   // 进程用户运行时间
	time_t tms_stime;   // 内核(系统)时间
	time_t tms_cutime;  // 子进程用户运行时间
	time_t tms_cstime;  // 子进程系统运行时间
};

extern time_t times(struct tms * tp);

#endif
