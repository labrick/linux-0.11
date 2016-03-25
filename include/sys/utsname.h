#ifndef _SYS_UTSNAME_H
#define _SYS_UTSNAME_H

#include <sys/types.h>

struct utsname {
	char sysname[9];    // 当前运行系统的名称
	char nodename[9];   // 网络节点名称(主机名)
	char release[9];    // 当前操作系统发行级别
	char version[9];    // 操作系统版本号
	char machine[9];    // 系统运行的硬件类型名称
};

extern int uname(struct utsname * utsbuf);

#endif
