#pragma once
#include <stdarg.h>
#include <stdio.h>
enum { ANDROID_LOG_DEBUG = 3, ANDROID_LOG_INFO, ANDROID_LOG_WARN, ANDROID_LOG_ERROR };
static inline int __android_log_print(int prio, const char *tag, const char *fmt, ...)
{
    (void) prio;
    va_list ap; va_start(ap, fmt);
    fprintf(stderr, "[%s] ", tag); vfprintf(stderr, fmt, ap); fputc('\n', stderr);
    va_end(ap);
    return 0;
}
