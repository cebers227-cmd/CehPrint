/*
 * capt-command.c — Android port of captdriver's command-channel.
 *
 * Original (CUPS-backed):
 *   Copyright (C) 2013 Alexey Galakhov <agalakhov@gmail.com>
 * Android port (CUPS replaced with io_provider callbacks):
 *   Copyright (C) 2026 cebers
 *
 * Licensed under the GNU General Public License Version 3 — see LICENSE.GPL3.
 */

#include "capt-command.h"

#include "std.h"
#include "word.h"
#include "io_provider.h"

#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <android/log.h>

#define LOG_TAG "captdriver"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

/* Error escape: set by main(), longjmp'd on fatal printer errors. */
extern jmp_buf capt_fatal_jmp;
#define FATAL(...) do { LOGE(__VA_ARGS__); longjmp(capt_fatal_jmp, 1); } while (0)

static struct capt_io_provider *g_io = NULL;

void capt_io_set(struct capt_io_provider *provider) { g_io = provider; }
struct capt_io_provider *capt_io_get(void) { return g_io; }

static uint8_t capt_iobuf[0x10000];
static size_t  capt_iosize;

static void capt_debug_buf(int prio, size_t size)
{
    if (size > capt_iosize) size = capt_iosize;
    char line[80]; size_t off = 0;
    for (size_t i = 0; i < size; ++i) {
        off += snprintf(line + off, sizeof(line) - off, "%02X ", capt_iobuf[i]);
        if ((i + 1) % 16 == 0 || i == size - 1) {
            __android_log_print(prio, LOG_TAG, "CAPT: %s", line);
            off = 0;
        }
    }
    if (size < capt_iosize) {
        __android_log_print(prio, LOG_TAG, "CAPT: ... (%u more)", (unsigned)(capt_iosize - size));
    }
}

static void capt_send_buf(void)
{
    const uint8_t *iopos = capt_iobuf;
    size_t iosize = capt_iosize;

    LOGD("send %u B", (unsigned) iosize);
    if (iosize <= 128) capt_debug_buf(ANDROID_LOG_DEBUG, iosize);

    if (!g_io || !g_io->write) FATAL("io_provider missing or no write fn");

    while (iosize) {
        size_t sendsize = iosize > 4096 ? 4096 : iosize;
        int written = g_io->write(g_io->ctx, iopos, sendsize);
        if (written < 0 || (size_t) written != sendsize) {
            FATAL("capt write failed: wanted=%u got=%d", (unsigned) sendsize, written);
        }
        iopos  += sendsize;
        iosize -= sendsize;
    }
}

static void capt_recv_buf(size_t offset, size_t expected)
{
    if (offset + expected > sizeof(capt_iobuf)) {
        FATAL("input buffer overflow off=%u exp=%u", (unsigned) offset, (unsigned) expected);
    }
    LOGD("waiting for %u B", (unsigned) expected);
    if (!g_io || !g_io->read) FATAL("io_provider missing or no read fn");
    int got = g_io->read(g_io->ctx, capt_iobuf + offset, expected, 15000);
    if (got < 0) FATAL("no reply from printer");
    capt_iosize = offset + (size_t) got;
}

const char *capt_identify(void)
{
    if (!g_io || !g_io->device_id) {
        FATAL("io_provider has no device_id (IEEE-1284 string required)");
    }
    LOGI("printer ID string %s", g_io->device_id);
    return g_io->device_id;
}

static void capt_copy_cmd(uint16_t cmd, const void *buf, size_t size)
{
    if (capt_iosize + 4 + size > sizeof(capt_iobuf)) {
        FATAL("output buffer overflow");
    }
    if (buf) memcpy(capt_iobuf + capt_iosize + 4, buf, size);
    else size = 0;
    capt_iobuf[capt_iosize + 0] = LO(cmd);
    capt_iobuf[capt_iosize + 1] = HI(cmd);
    capt_iobuf[capt_iosize + 2] = LO(size + 4);
    capt_iobuf[capt_iosize + 3] = HI(size + 4);
    capt_iosize += size + 4;
}

void capt_send(uint16_t cmd, const void *buf, size_t size)
{
    capt_iosize = 0;
    capt_copy_cmd(cmd, buf, size);
    capt_send_buf();
}

void capt_sendrecv(uint16_t cmd, const void *buf, size_t size, void *reply, size_t *reply_size)
{
    capt_send(cmd, buf, size);
    capt_recv_buf(0, 6);
    if (capt_iosize != 6 || WORD(capt_iobuf[0], capt_iobuf[1]) != cmd) {
        capt_debug_buf(ANDROID_LOG_ERROR, 6);
        FATAL("bad reply: expected %02X %02X, got first byte %02X", LO(cmd), HI(cmd), capt_iobuf[0]);
    }
    while (1) {
        if (WORD(capt_iobuf[2], capt_iobuf[3]) == capt_iosize) break;
        if (BCD(capt_iobuf[2], capt_iobuf[3]) == capt_iosize) break;
        if (WORD(capt_iobuf[2], capt_iobuf[3]) > capt_iosize && capt_iosize % 64 == 6) {
            capt_recv_buf(capt_iosize, WORD(capt_iobuf[2], capt_iobuf[3]) - capt_iosize);
            continue;
        }
        capt_debug_buf(ANDROID_LOG_ERROR, capt_iosize);
        FATAL("bad reply size %02X %02X vs %02X %02X",
              capt_iobuf[2], capt_iobuf[3], LO(capt_iosize), HI(capt_iosize));
    }
    if (capt_iosize <= 128) capt_debug_buf(ANDROID_LOG_DEBUG, capt_iosize);
    if (reply) {
        size_t copysize = reply_size ? *reply_size : capt_iosize;
        if (copysize > capt_iosize) copysize = capt_iosize;
        memcpy(reply, capt_iobuf + 4, copysize);
    }
    if (reply_size) *reply_size = capt_iosize;
}

void capt_multi_begin(uint16_t cmd)
{
    capt_iobuf[0] = LO(cmd);
    capt_iobuf[1] = HI(cmd);
    capt_iosize = 4;
}

void capt_multi_add(uint16_t cmd, const void *buf, size_t size)
{
    capt_copy_cmd(cmd, buf, size);
}

void capt_multi_send(void)
{
    capt_iobuf[2] = LO(capt_iosize);
    capt_iobuf[3] = HI(capt_iosize);
    capt_send_buf();
}

void capt_cleanup(void)
{
    capt_iosize = 0;
    /* g_io is owned by main(). */
}
