/*
 * capt_main.c — standalone captdriver job runner for CehPrint.
 *
 * Runs as its own process (libcaptjob.so is an executable). The app talks to it
 * over stdin/stdout, so the GPL code stays a separate program.
 *
 *   captjob probe <ieee1284-id>                       exit 0 if the model is known
 *   captjob job <ieee1284-id> <pages-file> <npages> <w> <h> <bytes-per-line> <dpi> <paper-w-pt> <paper-h-pt>
 *
 * Printer I/O is requested from the app with frames on stdout (little-endian):
 *   'W' u32 len, data          -> app replies i32 bytes written
 *   'R' u32 len, i32 timeoutMs -> app replies i32 n, then n bytes
 *
 * Copyright (C) 2026 cebers — GPL-3.0-or-later (matches captdriver).
 */
#include <android/log.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "captdriver/std.h"
#include "captdriver/printer.h"
#include "captdriver/paper.h"
#include "captdriver/capt-command.h"
#include "captdriver/cups_compat.h"
#include "captdriver/io_provider.h"

#define LOG_TAG "captjob"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

jmp_buf capt_fatal_jmp;

static int read_all(int fd, void *buf, size_t len)
{
    uint8_t *p = buf;
    while (len) {
        ssize_t n = read(fd, p, len);
        if (n <= 0) return -1;
        p += n; len -= (size_t) n;
    }
    return 0;
}

static int write_all(int fd, const void *buf, size_t len)
{
    const uint8_t *p = buf;
    while (len) {
        ssize_t n = write(fd, p, len);
        if (n <= 0) return -1;
        p += n; len -= (size_t) n;
    }
    return 0;
}

static int pipe_write(void *ctx, const uint8_t *data, size_t len)
{
    (void) ctx;
    uint8_t h[5] = { 'W' };
    uint32_t l = (uint32_t) len;
    memcpy(h + 1, &l, 4);
    int32_t res;
    if (write_all(1, h, 5) || write_all(1, data, len) || read_all(0, &res, 4)) return -1;
    return res;
}

static int pipe_read(void *ctx, uint8_t *data, size_t len, int timeout_ms)
{
    (void) ctx;
    uint8_t h[9] = { 'R' };
    uint32_t l = (uint32_t) len;
    int32_t t = timeout_ms, n;
    memcpy(h + 1, &l, 4);
    memcpy(h + 5, &t, 4);
    if (write_all(1, h, 9) || read_all(0, &n, 4)) return -1;
    if (n > 0 && ((size_t) n > len || read_all(0, data, (size_t) n))) return -1;
    return n;
}

/* ---- per-band linked list (mirrors rastertocapt.c) ---- */
struct band_list_s {
    struct band_list_s *next;
    size_t size;
    uint8_t data[];
};

static void free_band_list(struct band_list_s *head)
{
    while (head) {
        struct band_list_s *next = head->next;
        free(head);
        head = next;
    }
}

/*
 * Compress one page out of a flat raster buffer in memory.
 *
 * Adapted from rastertocapt.c::compress_page_data() — instead of pulling
 * scanlines through cupsRasterReadPixels(), we memcpy() them directly from
 * [pixels] which is a pre-rasterised one-page bitmap exactly
 * header->cupsHeight * header->cupsBytesPerLine bytes long.
 *
 * Returns a linked list of compressed bands ready to be passed to send_band.
 */
static struct band_list_s *compress_page_from_mem(
    struct printer_state_s *state,
    const struct page_dims_s *dims,
    const uint8_t *pixels,
    const struct cups_page_header2_s *header)
{
    const unsigned compsize = 2 * dims->line_size * dims->band_size;
    struct band_list_s *head = NULL, *tail = NULL;

    uint8_t *linebuf = calloc(1, header->cupsBytesPerLine);
    uint8_t *bandbuf = calloc(dims->line_size, dims->band_size);
    uint8_t *compbuf = calloc(1, compsize);
    if (!linebuf || !bandbuf || !compbuf) {
        free(linebuf); free(bandbuf); free(compbuf);
        LOGE("OOM in compress_page_from_mem");
        longjmp(capt_fatal_jmp, 1);
    }

    unsigned src_bpl = header->cupsBytesPerLine;
    unsigned dst_bpl = dims->line_size;
    unsigned copysize = src_bpl < dst_bpl ? src_bpl : dst_bpl;

    for (unsigned iband = 0; iband * dims->band_size < dims->num_lines; ++iband) {
        unsigned start = iband * dims->band_size;
        unsigned nlines = dims->band_size;
        if (start + nlines > dims->num_lines) nlines = dims->num_lines - start;

        memset(bandbuf, 0, dst_bpl * dims->band_size);
        for (unsigned iline = 0; iline < nlines; ++iline) {
            if (start + iline < header->cupsHeight) {
                memcpy(linebuf, pixels + (start + iline) * src_bpl, src_bpl);
            } else {
                memset(linebuf, 0, src_bpl);
            }
            memcpy(bandbuf + iline * dst_bpl, linebuf, copysize);
        }

        size_t size = state->ops->compress_band(
            state, compbuf, compsize, bandbuf, dst_bpl, nlines);

        struct band_list_s *band = calloc(1, sizeof(*band) + size);
        if (!band) {
            free(linebuf); free(bandbuf); free(compbuf);
            free_band_list(head);
            longjmp(capt_fatal_jmp, 1);
        }
        band->size = size;
        if (size) memcpy(band->data, compbuf, size);
        if (!head) head = band;
        if (tail) tail->next = band;
        tail = band;
    }

    free(linebuf); free(bandbuf); free(compbuf);
    return head;
}

static void send_bands(struct printer_state_s *state, const struct band_list_s *head)
{
    state->isend = 0;
    state->iband = 0;
    for (const struct band_list_s *b = head; b; b = b->next, ++state->iband) {
        state->ops->send_band(state, b->data, b->size);
    }
}


static int cmd_probe(char *id)
{
    struct capt_io_provider provider = { .ctx = NULL, .write = NULL, .read = NULL, .device_id = id };
    capt_io_set(&provider);
    int rc = 1;
    if (setjmp(capt_fatal_jmp) == 0 && printer_detect()) rc = 0;
    capt_io_set(NULL);
    return rc;
}

static int cmd_job(char *id, const char *pages_path, int npages, int w, int h, int bpl,
                   int dpi, int paper_w, int paper_h)
{
    FILE *f = fopen(pages_path, "rb");
    if (!f) { LOGE("cannot open %s", pages_path); return 1; }
    size_t page_len = (size_t) h * (size_t) bpl;
    uint8_t *pixels = malloc(page_len);
    if (!pixels) { fclose(f); return 1; }

    struct capt_io_provider provider = { .ctx = NULL, .write = pipe_write, .read = pipe_read, .device_id = id };
    capt_io_set(&provider);

    int ok = 0;
    struct printer_state_s *state = NULL;
    const struct printer_ops_s *ops = NULL;

    if (setjmp(capt_fatal_jmp) != 0) {
        LOGE("fatal error, aborting job");
        goto cleanup;
    }
    ops = printer_detect();
    if (!ops) goto cleanup;
    state = ops->alloc_state ? ops->alloc_state() : calloc(1, sizeof(*state));
    state->ops = ops;
    state->ipage = 0;
    if (ops->job_prologue) ops->job_prologue(state);

    for (int ip = 0; ip < npages; ++ip) {
        if (fread(pixels, 1, page_len, f) != page_len) { LOGE("short pages file at page %d", ip); goto cleanup; }
        struct cups_page_header2_s header = {0};
        header.cupsWidth = (unsigned) w;
        header.cupsHeight = (unsigned) h;
        header.cupsBytesPerLine = (unsigned) bpl;
        header.cupsBitsPerPixel = 1;
        header.cupsBitsPerColor = 1;
        header.cupsColorSpace = CUPS_CSPACE_K;
        header.cupsRowCount = 64;
        header.PageSize[0] = (unsigned) paper_w;
        header.PageSize[1] = (unsigned) paper_h;
        header.HWResolution[0] = (unsigned) dpi;
        header.HWResolution[1] = (unsigned) dpi;
        snprintf(header.MediaType, sizeof(header.MediaType), "Plain");

        struct page_dims_s dims = {0};
        page_set_dims(&dims, &header);
        ops->page_setup(state, &dims, header.cupsWidth, header.cupsHeight);
        state->ipage += 1;

        struct band_list_s *bands = compress_page_from_mem(state, &dims, pixels, &header);
        bool page_ok = true;
        if (ops->page_prologue && !ops->page_prologue(state, &dims)) {
            LOGW("page_prologue refused page %d", ip + 1);
            page_ok = false;
        }
        if (page_ok) send_bands(state, bands);
        if (page_ok && ops->page_epilogue && !ops->page_epilogue(state, &dims)) {
            LOGW("page_epilogue refused page %d", ip + 1);
        }
        free_band_list(bands);
        if (!page_ok) goto cleanup;
    }
    if (ops->job_epilogue) ops->job_epilogue(state);
    ok = 1;
    LOGI("job done, %d pages", npages);

cleanup:
    if (state) {
        if (ops && ops->free_state) ops->free_state(state);
        else free(state);
    }
    capt_cleanup();
    capt_io_set(NULL);
    free(pixels);
    fclose(f);
    return ok ? 0 : 1;
}

int main(int argc, char **argv)
{
    if (argc == 3 && !strcmp(argv[1], "probe")) return cmd_probe(argv[2]);
    if (argc == 11 && !strcmp(argv[1], "job")) {
        return cmd_job(argv[2], argv[3], atoi(argv[4]), atoi(argv[5]), atoi(argv[6]),
                       atoi(argv[7]), atoi(argv[8]), atoi(argv[9]), atoi(argv[10]));
    }
    fprintf(stderr, "usage: captjob probe <id> | job <id> <pages> <n> <w> <h> <bpl> <dpi> <pw> <ph>\n");
    return 2;
}
