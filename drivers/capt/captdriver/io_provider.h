/*
 * io_provider.h — Android-specific I/O abstraction for captdriver.
 *
 * Original captdriver (GPL-3) does USB I/O through CUPS pipes (stdin/stdout +
 * sidechannel). On Android we run as a JNI library that has no CUPS — instead
 * we receive a pair of JNI callback function pointers that ultimately invoke
 * UsbDeviceConnection.bulkTransfer() on the Java/Kotlin side.
 *
 * Copyright (C) 2026 cebers
 * Licensed under the GNU General Public License Version 3 (matches captdriver).
 */
#pragma once

#include "std.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Callback signatures (filled in by the JNI entry point). */
typedef int (*io_write_fn)(void *ctx, const uint8_t *data, size_t len);
/* Reads up to len bytes, blocks until any bytes arrive or timeout (ms). */
typedef int (*io_read_fn)(void *ctx, uint8_t *data, size_t len, int timeout_ms);

struct capt_io_provider {
    void *ctx;
    io_write_fn write;
    io_read_fn  read;
    /* IEEE-1284 device ID string (heap-allocated, freed by provider). May be NULL. */
    char *device_id;
};

/* Set globally before any capt_* call. Lives for the duration of one job. */
void capt_io_set(struct capt_io_provider *provider);
struct capt_io_provider *capt_io_get(void);

#ifdef __cplusplus
}
#endif
