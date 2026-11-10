#pragma once
#include <string.h>
#include <stdlib.h>
#include <android/api-level.h>
#if __ANDROID_API__ < 24
#include <fcntl.h>
#include <unistd.h>
#ifndef F_ULOCK
#define F_ULOCK 0
#define F_LOCK 1
#define F_TLOCK 2
#define F_TEST 3
#endif
static inline int lockf(int fd, int cmd, off_t len)
{
  struct flock fl = { .l_whence = SEEK_CUR, .l_start = 0, .l_len = len };
  switch (cmd) {
  case F_ULOCK: fl.l_type = F_UNLCK; return fcntl(fd, F_SETLK, &fl);
  case F_LOCK: fl.l_type = F_WRLCK; return fcntl(fd, F_SETLKW, &fl);
  case F_TLOCK: fl.l_type = F_WRLCK; return fcntl(fd, F_SETLK, &fl);
  default: fl.l_type = F_RDLCK; return fcntl(fd, F_GETLK, &fl) == -1 ? -1 : (fl.l_type == F_UNLCK ? 0 : -1);
  }
}
#endif

#if __ANDROID_API__ < 26
#include <langinfo.h>
static inline char *ceh_nl_langinfo(nl_item item) { (void)item; return (char *)"UTF-8"; }
#define nl_langinfo ceh_nl_langinfo
#endif

#include <pthread.h>
static inline int ceh_pthread_cancel(pthread_t t) { (void)t; return 0; }
#define pthread_cancel ceh_pthread_cancel

#include <stddef.h>
static inline int ceh_getdomainname(char *name, size_t len) { if (len) name[0] = 0; return 0; }
#define getdomainname ceh_getdomainname
