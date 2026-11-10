/*
 * Copyright (C) 2026 cebers — GPL-3.0-or-later
 */
#include <cups/cups.h>
#include <cups/ppd.h>
#include <cups/raster.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int read_full(int fd, unsigned char *buf, size_t len)
{
  size_t got = 0;
  while (got < len) {
    ssize_t n = read(fd, buf + got, len - got);
    if (n <= 0) return 0;
    got += (size_t)n;
  }
  return 1;
}

static int input_channels(const cups_page_header2_t *h)
{
  switch (h->cupsColorSpace) {
  case CUPS_CSPACE_W:
  case CUPS_CSPACE_K:
  case CUPS_CSPACE_SW:
    return 1;
  default:
    return 3;
  }
}

static void channel_values(const cups_page_header2_t *h, int ch, const unsigned char *in, int x, unsigned char v[4])
{
  int r, g, b;
  v[0] = v[1] = v[2] = v[3] = 0;
  if (ch == 1) {
    r = g = b = in[x];
  } else {
    r = in[x * 3]; g = in[x * 3 + 1]; b = in[x * 3 + 2];
  }
  switch (h->cupsColorSpace) {
  case CUPS_CSPACE_W:
  case CUPS_CSPACE_SW:
    v[0] = (unsigned char)((r * 30 + g * 59 + b * 11) / 100);
    break;
  case CUPS_CSPACE_K:
    v[0] = (unsigned char)(255 - (r * 30 + g * 59 + b * 11) / 100);
    break;
  case CUPS_CSPACE_RGB:
  case CUPS_CSPACE_SRGB:
  case CUPS_CSPACE_ADOBERGB:
    v[0] = (unsigned char)r; v[1] = (unsigned char)g; v[2] = (unsigned char)b;
    break;
  case CUPS_CSPACE_CMY:
    v[0] = (unsigned char)(255 - r); v[1] = (unsigned char)(255 - g); v[2] = (unsigned char)(255 - b);
    break;
  case CUPS_CSPACE_CMYK: {
    int c = 255 - r, m = 255 - g, y = 255 - b;
    int k = c < m ? (c < y ? c : y) : (m < y ? m : y);
    v[0] = (unsigned char)(c - k); v[1] = (unsigned char)(m - k);
    v[2] = (unsigned char)(y - k); v[3] = (unsigned char)k;
    break;
  }
  default:
    v[0] = (unsigned char)((r * 30 + g * 59 + b * 11) / 100);
  }
}

static void dither_plane(const unsigned char *val, int w, int *err_cur, int *err_next, unsigned char *out)
{
  memset(out, 0, (size_t)((w + 7) / 8));
  memset(err_next, 0, sizeof(int) * (size_t)(w + 2));
  for (int x = 0; x < w; x++) {
    int v = val[x] + err_cur[x + 1] / 16;
    int on = v >= 128;
    int e = v - (on ? 255 : 0);
    err_cur[x + 2] += e * 7;
    err_next[x] += e * 3;
    err_next[x + 1] += e * 5;
    err_next[x + 2] += e;
    if (on) out[x >> 3] |= (unsigned char)(0x80 >> (x & 7));
  }
}

int main(int argc, char *argv[])
{
  if (argc < 3) {
    fprintf(stderr, "usage: cehraster info|pages N [options]\n");
    return 1;
  }
  const char *ppdname = getenv("PPD");
  ppd_file_t *ppd = ppdname ? ppdOpenFile(ppdname) : NULL;
  if (!ppd) {
    fprintf(stderr, "ERROR: cannot open PPD\n");
    return 2;
  }
  cups_option_t *options = NULL;
  int num_options = cupsParseOptions(argc > 3 ? argv[3] : "", 0, &options);
  ppdMarkDefaults(ppd);
  cupsMarkOptions(ppd, num_options, options);

  cups_page_header2_t h;
  if (cupsRasterInterpretPPD(&h, ppd, num_options, options, NULL)) {
    fprintf(stderr, "ERROR: cupsRasterInterpretPPD: %s\n", cupsRasterErrorString());
    return 3;
  }

  if (!strcmp(argv[1], "info")) {
    printf("%u %u %u %u %u %u %u %u %u %u %u %u %u %u\n", h.cupsWidth, h.cupsHeight, h.HWResolution[0],
           h.HWResolution[1], h.cupsColorSpace, h.cupsBitsPerColor, h.cupsBitsPerPixel,
           (unsigned)input_channels(&h), h.PageSize[0], h.PageSize[1], h.ImagingBoundingBox[0],
           h.ImagingBoundingBox[1], h.ImagingBoundingBox[2], h.ImagingBoundingBox[3]);
    return 0;
  }

  int pages = atoi(argv[2]);
  int ch = input_channels(&h);
  int nc = (int)h.cupsNumColors;
  int w = (int)h.cupsWidth;
  int banded = h.cupsColorOrder == CUPS_ORDER_BANDED;
  int bits = (int)h.cupsBitsPerColor;
  if (h.cupsColorOrder == CUPS_ORDER_PLANAR || nc > 4 || (bits != 1 && bits != 8) ||
      (bits == 1 && nc > 1 && !banded)) {
    fprintf(stderr, "ERROR: unsupported raster layout (order %u, colors %d, bits %d)\n", h.cupsColorOrder, nc, bits);
    return 4;
  }
  size_t in_len = (size_t)w * (size_t)ch;
  unsigned char *in = malloc(in_len);
  unsigned char *line = malloc(h.cupsBytesPerLine);
  unsigned char *val[4];
  int *e1[4], *e2[4];
  for (int c = 0; c < 4; c++) {
    val[c] = malloc((size_t)w);
    e1[c] = calloc((size_t)w + 2, sizeof(int));
    e2[c] = calloc((size_t)w + 2, sizeof(int));
  }
  size_t plane_bytes = bits == 1 ? (size_t)((w + 7) / 8) : (size_t)w;
  cups_raster_t *ras = cupsRasterOpen(1, CUPS_RASTER_WRITE);
  for (int p = 0; p < pages; p++) {
    if (!cupsRasterWriteHeader2(ras, &h)) return 5;
    for (int c = 0; c < 4; c++) memset(e1[c], 0, sizeof(int) * ((size_t)w + 2));
    for (unsigned y = 0; y < h.cupsHeight; y++) {
      if (!read_full(0, in, in_len)) {
        fprintf(stderr, "ERROR: short input on page %d line %u\n", p + 1, y);
        return 6;
      }
      for (int x = 0; x < w; x++) {
        unsigned char v[4];
        channel_values(&h, ch, in, x, v);
        for (int c = 0; c < 4; c++) val[c][x] = v[c];
      }
      memset(line, 0, h.cupsBytesPerLine);
      for (int c = 0; c < nc; c++) {
        if (bits == 1) {
          dither_plane(val[c], w, e1[c], e2[c], line + (banded ? (size_t)c * plane_bytes : 0));
          int *t = e1[c]; e1[c] = e2[c]; e2[c] = t;
        } else {
          for (int x = 0; x < w; x++) {
            if (banded) line[(size_t)c * plane_bytes + x] = val[c][x];
            else line[(size_t)x * nc + c] = val[c][x];
          }
        }
      }
      cupsRasterWritePixels(ras, line, h.cupsBytesPerLine);
    }
  }
  cupsRasterClose(ras);
  ppdClose(ppd);
  return 0;
}
