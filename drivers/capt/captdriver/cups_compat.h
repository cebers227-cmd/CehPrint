/*
 * cups_compat.h — minimal CUPS-raster header compatibility shim for Android.
 *
 * Original captdriver pulls fields like cupsBytesPerLine / cupsBitsPerPixel
 * out of <cups/raster.h>. On Android we have no CUPS, so we declare just the
 * fields paper.c / rastertocapt.c actually read, and JNI fills them up from
 * Android Bitmap raster + Kotlin-side metadata.
 *
 * Copyright (C) 2026 cebers — GPL-3 (matches captdriver).
 */
#pragma once

#include "std.h"

/* Mirrors layout-relevant fields from cups_page_header2_t. */
struct cups_page_header2_s {
    unsigned  cupsWidth;          /* width in pixels                       */
    unsigned  cupsHeight;         /* height in pixels (lines)              */
    unsigned  cupsBytesPerLine;   /* bytes per scanline (= width * bpp / 8)*/
    unsigned  cupsBitsPerPixel;
    unsigned  cupsBitsPerColor;
    unsigned  cupsColorSpace;     /* CUPS_CSPACE_K / CUPS_CSPACE_W         */
    unsigned  cupsRowCount;       /* band height                           */
    unsigned  cupsMediaType;
    char      MediaType[64];
    unsigned  cupsCompression;
    unsigned  PageSize[2];        /* points (1/72")                        */
    unsigned  HWResolution[2];    /* DPI (x, y)                            */
    unsigned  Margins[2];
    int       cupsInteger[16];    /* [0]=toner_save [1]=ink_k [2]=manual_duplex */
};

/* CUPS color-space enum values we care about. */
#define CUPS_CSPACE_W 3   /* 1-channel grayscale, white = 1 (CUPS naming)  */
#define CUPS_CSPACE_K 4   /* 1-channel black,    black = 1                 */
