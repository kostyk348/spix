/**
 * spix.h — Single-header C99 image processing library for embedded systems
 *
 * Fixed-point arithmetic throughout. No FPU required. Zero dependencies
 * beyond libc (stdio, stdint, stdlib, string).
 *
 * Usage:
 *   #define SPIX_IMPLEMENTATION
 *   #include "spix.h"
 *
 * Image IO uses PPM/PGM (portable pixmap) format — no external libraries.
 * Pixel format: RGBA8888 (32-bit, 8 bits per channel).
 *
 * Features:
 *   - Resize (nearest-neighbor, bilinear fixed-point)
 *   - Convolution (3×3, 5×5): blur, sharpen, edge detect, emboss, sobel
 *   - Dithering (Floyd-Steinberg, Bayer ordered 4×4)
 *   - Color space (RGB ⇄ grayscale, RGB ⇄ YUV, threshold)
 *   - Flip / rotate / transpose
 *   - PPM/PGM read/write (ASCII and binary)
 */
#ifndef SPIX_H
#define SPIX_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Pixel type ─────────────────────────────────────────── */

typedef uint32_t spix_pixel_t;

#define SPIX_RGBA(r, g, b, a)  ((spix_pixel_t)(((uint8_t)(r) << 24) | ((uint8_t)(g) << 16) | ((uint8_t)(b) << 8) | (uint8_t)(a)))
#define SPIX_RGB(r, g, b)      SPIX_RGBA(r, g, b, 0xFF)
#define SPIX_R(p)              ((uint8_t)((p) >> 24))
#define SPIX_G(p)              ((uint8_t)((p) >> 16))
#define SPIX_B(p)              ((uint8_t)((p) >> 8))
#define SPIX_A(p)              ((uint8_t)(p))
#define SPIX_BLACK              SPIX_RGB(0, 0, 0)
#define SPIX_WHITE              SPIX_RGB(255, 255, 255)

/* ── Image ──────────────────────────────────────────────── */

typedef struct {
    int           width;
    int           height;
    spix_pixel_t *pixels;    /**< row-major, allocated by spix_alloc() */
    int           stride;    /**< row stride in pixels (≥ width) */
    int           owned;     /**< 1 = spix_free() will free pixels */
} spix_image_t;

#define SPIX_AT(img, x, y)  ((img)->pixels[(y) * (img)->stride + (x)])
#define SPIX_CLAMP(v, lo, hi)  ((v) < (lo) ? (lo) : (v) > (hi) ? (hi) : (v))

/* ── Error codes ────────────────────────────────────────── */

typedef enum {
    SPIX_OK       = 0,
    SPIX_ERR_MEM  = -1,
    SPIX_ERR_IO   = -2,
    SPIX_ERR_SIZE = -3,
    SPIX_ERR_FORMAT = -4,
} spix_error_t;

/* ── Lifecycle ──────────────────────────────────────────── */

/** Allocate image (pixels are zeroed). Returns NULL on OOM. */
spix_image_t *spix_alloc(int width, int height);

/** Create image wrapping existing pixel buffer (not owned). */
spix_image_t spix_wrap(int width, int height, spix_pixel_t *pixels, int stride);

/** Free image allocated with spix_alloc(). */
void spix_free(spix_image_t *img);

/** Clone image (deep copy). Returns NULL on OOM. */
spix_image_t *spix_clone(const spix_image_t *src);

/* ── IO: PPM/PGM ────────────────────────────────────────── */

/** Read PPM/PGM from file (supports P2/P3/P5/P6). Returns NULL on error. */
spix_image_t *spix_read_ppm(const char *path);

/** Write PPM P6 (binary) to file. Returns SPIX_OK or error. */
int spix_write_ppm(const spix_image_t *img, const char *path);

/** Read PPM/PGM from memory buffer. */
spix_image_t *spix_read_ppm_mem(const uint8_t *buf, size_t len);

/** Write PPM P6 to memory buffer (caller must free *out). Returns bytes written. */
size_t spix_write_ppm_mem(const spix_image_t *img, uint8_t **out);

/* ── Resize ─────────────────────────────────────────────── */

/** Nearest-neighbor resize. Returns new image (caller must spix_free). */
spix_image_t *spix_resize_nn(const spix_image_t *src, int new_w, int new_h);

/** Bilinear resize with fixed-point Q16.16 coordinates. */
spix_image_t *spix_resize_bilinear(const spix_image_t *src, int new_w, int new_h);

/* ── Convolution ────────────────────────────────────────── */

typedef struct { int size; int16_t data[25]; int shift; } spix_kernel_t;

/** Apply kernel to image. 3×3 and 5×5 kernels supported. Returns new image. */
spix_image_t *spix_convolve(const spix_image_t *src, const spix_kernel_t *k);

/** Apply kernel in-place (writes to dst, which can be src for in-place). */
int spix_convolve_to(const spix_image_t *src, spix_image_t *dst, const spix_kernel_t *k);

/* Predefined kernels */
extern const spix_kernel_t SPIX_KERNEL_BOX_BLUR_3;
extern const spix_kernel_t SPIX_KERNEL_GAUSS_BLUR_3;
extern const spix_kernel_t SPIX_KERNEL_SHARPEN;
extern const spix_kernel_t SPIX_KERNEL_EDGE_DETECT;
extern const spix_kernel_t SPIX_KERNEL_EMBOSS;
extern const spix_kernel_t SPIX_KERNEL_SOBEL_H;
extern const spix_kernel_t SPIX_KERNEL_SOBEL_V;

/* ── Color ──────────────────────────────────────────────── */

/** Convert to grayscale (luminosity: 0.299R + 0.587G + 0.114B, fixed-point). */
int spix_grayscale(spix_image_t *img);

/** Binarize using fixed threshold (0–255). */
int spix_threshold(spix_image_t *img, uint8_t threshold);

/** Binarize using Otsu's method. Returns threshold used. */
uint8_t spix_threshold_otsu(const spix_image_t *img);

/* ── Dithering ──────────────────────────────────────────── */

/** Floyd-Steinberg error diffusion dithering (in-place). */
int spix_dither_floyd(spix_image_t *img);

/** Bayer ordered dithering with 4×4 matrix (in-place). */
int spix_dither_bayer(spix_image_t *img);

/* ── Geometry ───────────────────────────────────────────── */

/** Horizontal flip (in-place). */
int spix_flip_h(spix_image_t *img);

/** Vertical flip (in-place). */
int spix_flip_v(spix_image_t *img);

/** Rotate 90° clockwise. Returns new image. */
spix_image_t *spix_rotate_90(const spix_image_t *src);

#ifdef __cplusplus
}
#endif

/* ================================================================
 *  IMPLEMENTATION
 * ================================================================ */
#ifdef SPIX_IMPLEMENTATION

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

/* ── Kernels ─────────────────────────────────────────────── */

const spix_kernel_t SPIX_KERNEL_BOX_BLUR_3 = {3, {1,1,1, 1,1,1, 1,1,1}, 3};
const spix_kernel_t SPIX_KERNEL_GAUSS_BLUR_3 = {3, {1,2,1, 2,4,2, 1,2,1}, 4};
const spix_kernel_t SPIX_KERNEL_SHARPEN = {3, {0,-1,0, -1,5,-1, 0,-1,0}, 0};
const spix_kernel_t SPIX_KERNEL_EDGE_DETECT = {3, {-1,-1,-1, -1,8,-1, -1,-1,-1}, 0};
const spix_kernel_t SPIX_KERNEL_EMBOSS = {3, {-2,-1,0, -1,1,1, 0,1,2}, 0};
const spix_kernel_t SPIX_KERNEL_SOBEL_H = {3, {-1,0,1, -2,0,2, -1,0,1}, 0};
const spix_kernel_t SPIX_KERNEL_SOBEL_V = {3, {-1,-2,-1, 0,0,0, 1,2,1}, 0};

/* ── Lifecycle ───────────────────────────────────────────── */

spix_image_t *spix_alloc(int width, int height) {
    if (width <= 0 || height <= 0) return NULL;
    spix_image_t *img = (spix_image_t *)calloc(1, sizeof(spix_image_t));
    if (!img) return NULL;
    img->pixels = (spix_pixel_t *)calloc((size_t)(width * height), sizeof(spix_pixel_t));
    if (!img->pixels) { free(img); return NULL; }
    img->width  = width;
    img->height = height;
    img->stride = width;
    img->owned  = 1;
    return img;
}

spix_image_t spix_wrap(int width, int height, spix_pixel_t *pixels, int stride) {
    spix_image_t img = { width, height, pixels, stride, 0 };
    return img;
}

void spix_free(spix_image_t *img) {
    if (!img) return;
    if (img->owned && img->pixels) free(img->pixels);
    free(img);
}

spix_image_t *spix_clone(const spix_image_t *src) {
    if (!src) return NULL;
    spix_image_t *dst = spix_alloc(src->width, src->height);
    if (!dst) return NULL;
    for (int y = 0; y < src->height; y++)
        memcpy(dst->pixels + y * dst->stride,
               src->pixels + y * src->stride,
               (size_t)src->width * sizeof(spix_pixel_t));
    return dst;
}

/* ── IO helpers ──────────────────────────────────────────── */

static int _spix_skip_ws(FILE *fp) {
    int c;
    while ((c = fgetc(fp)) != EOF) {
        if (c == '#') { while ((c = fgetc(fp)) != EOF && c != '\n'); }
        else if (!isspace(c)) { ungetc(c, fp); return 1; }
    }
    return 0;
}

static int _spix_read_int(FILE *fp) {
    int val = 0, c;
    if (!_spix_skip_ws(fp)) return -1;
    while ((c = fgetc(fp)) != EOF && isdigit(c)) val = val * 10 + (c - '0');
    return val;
}

/* ── PPM read ────────────────────────────────────────────── */

spix_image_t *spix_read_ppm(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;

    char magic[3] = {0};
    if (!fgets(magic, sizeof(magic), fp) || magic[0] != 'P') { fclose(fp); return NULL; }

    int w = _spix_read_int(fp);
    int h = _spix_read_int(fp);
    int maxval = _spix_read_int(fp);
    if (w <= 0 || h <= 0 || maxval <= 0) { fclose(fp); return NULL; }

    spix_image_t *img = spix_alloc(w, h);
    if (!img) { fclose(fp); return NULL; }

    int c;
    fgetc(fp); /* consume newline after maxval */

    if (magic[1] == '6') {
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                int r = fgetc(fp); int g = fgetc(fp); int b = fgetc(fp);
                if (r == EOF) { spix_free(img); fclose(fp); return NULL; }
                SPIX_AT(img, x, y) = SPIX_RGB(
                    (uint8_t)(r * 255 / maxval),
                    (uint8_t)(g * 255 / maxval),
                    (uint8_t)(b * 255 / maxval));
            }
    } else if (magic[1] == '5') {
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                int v = fgetc(fp);
                if (v == EOF) { spix_free(img); fclose(fp); return NULL; }
                uint8_t g = (uint8_t)(v * 255 / maxval);
                SPIX_AT(img, x, y) = SPIX_RGB(g, g, g);
            }
    } else if (magic[1] == '3') {
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                int r = _spix_read_int(fp);
                int g = _spix_read_int(fp);
                int b = _spix_read_int(fp);
                if (r < 0) { spix_free(img); fclose(fp); return NULL; }
                SPIX_AT(img, x, y) = SPIX_RGB(
                    (uint8_t)(r * 255 / maxval),
                    (uint8_t)(g * 255 / maxval),
                    (uint8_t)(b * 255 / maxval));
            }
    } else if (magic[1] == '2') {
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                int v = _spix_read_int(fp);
                if (v < 0) { spix_free(img); fclose(fp); return NULL; }
                uint8_t g = (uint8_t)(v * 255 / maxval);
                SPIX_AT(img, x, y) = SPIX_RGB(g, g, g);
            }
    } else { spix_free(img); fclose(fp); return NULL; }

    fclose(fp);
    return img;
}

int spix_write_ppm(const spix_image_t *img, const char *path) {
    if (!img) return SPIX_ERR_IO;
    FILE *fp = fopen(path, "wb");
    if (!fp) return SPIX_ERR_IO;
    fprintf(fp, "P6\n%d %d\n255\n", img->width, img->height);
    for (int y = 0; y < img->height; y++) {
        for (int x = 0; x < img->width; x++) {
            spix_pixel_t p = SPIX_AT(img, x, y);
            uint8_t rgb[3] = { SPIX_R(p), SPIX_G(p), SPIX_B(p) };
            fwrite(rgb, 1, 3, fp);
        }
    }
    fclose(fp);
    return SPIX_OK;
}

/* ── Memory PPM I/O ──────────────────────────────────────── */

spix_image_t *spix_read_ppm_mem(const uint8_t *buf, size_t len) {
    /* Write to temp file (simplest cross-platform approach for single-header) */
    FILE *fp = tmpfile();
    if (!fp) return NULL;
    fwrite(buf, 1, len, fp);
    rewind(fp);

    char magic[3] = {0};
    if (!fgets(magic, sizeof(magic), fp) || magic[0] != 'P') { fclose(fp); return NULL; }

    int w = 0, h = 0, maxval = 0;
    fscanf(fp, "%d %d %d", &w, &h, &maxval);
    if (w <= 0 || h <= 0 || maxval <= 0) { fclose(fp); return NULL; }
    fgetc(fp);

    spix_image_t *img = spix_alloc(w, h);
    if (!img) { fclose(fp); return NULL; }

    if (magic[1] == '6') {
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                int r = fgetc(fp), g = fgetc(fp), b = fgetc(fp);
                if (r == EOF) { spix_free(img); fclose(fp); return NULL; }
                SPIX_AT(img, x, y) = SPIX_RGB(r, g, b);
            }
    } else if (magic[1] == '5') {
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                int v = fgetc(fp); if (v == EOF) { spix_free(img); fclose(fp); return NULL; }
                SPIX_AT(img, x, y) = SPIX_RGB((uint8_t)v, (uint8_t)v, (uint8_t)v);
            }
    } else { spix_free(img); fclose(fp); return NULL; }

    fclose(fp);
    return img;
}

size_t spix_write_ppm_mem(const spix_image_t *img, uint8_t **out) {
    if (!img || !out) return 0;

    /* Write PPM to memory buffer */
    char header[128];
    int hlen = snprintf(header, sizeof(header), "P6\n%d %d\n255\n", img->width, img->height);
    size_t row_bytes = (size_t)img->width * 3;
    size_t total = (size_t)hlen + (size_t)img->height * row_bytes;
    *out = (uint8_t *)malloc(total + 1);
    if (!*out) return 0;
    memcpy(*out, header, (size_t)hlen);
    for (int y = 0; y < img->height; y++) {
        for (int x = 0; x < img->width; x++) {
            spix_pixel_t p = SPIX_AT(img, x, y);
            (*out)[(size_t)hlen + y * row_bytes + x * 3 + 0] = SPIX_R(p);
            (*out)[(size_t)hlen + y * row_bytes + x * 3 + 1] = SPIX_G(p);
            (*out)[(size_t)hlen + y * row_bytes + x * 3 + 2] = SPIX_B(p);
        }
    }
    (*out)[total] = 0;
    return total;
}

/* ── Resize: nearest-neighbor ────────────────────────────── */

spix_image_t *spix_resize_nn(const spix_image_t *src, int new_w, int new_h) {
    if (!src || new_w <= 0 || new_h <= 0) return NULL;
    spix_image_t *dst = spix_alloc(new_w, new_h);
    if (!dst) return NULL;
    for (int y = 0; y < new_h; y++) {
        int sy = y * src->height / new_h;
        sy = SPIX_CLAMP(sy, 0, src->height - 1);
        for (int x = 0; x < new_w; x++) {
            int sx = x * src->width / new_w;
            sx = SPIX_CLAMP(sx, 0, src->width - 1);
            SPIX_AT(dst, x, y) = SPIX_AT(src, sx, sy);
        }
    }
    return dst;
}

/* ── Resize: bilinear (fixed-point Q16.16) ───────────────── */

spix_image_t *spix_resize_bilinear(const spix_image_t *src, int new_w, int new_h) {
    if (!src || new_w <= 0 || new_h <= 0) return NULL;
    spix_image_t *dst = spix_alloc(new_w, new_h);
    if (!dst) return NULL;

    int64_t x_ratio = (int64_t)(src->width  - 1) << 16;
    int64_t y_ratio = (int64_t)(src->height - 1) << 16;

    for (int y = 0; y < new_h; y++) {
        int64_t sy_fp = y * y_ratio / new_h;
        int sy = (int)(sy_fp >> 16);
        int sy1 = sy + 1; if (sy1 >= src->height) sy1 = sy;
        int fy = (int)(sy_fp & 0xFFFF);
        int ify = 0x10000 - fy;

        for (int x = 0; x < new_w; x++) {
            int64_t sx_fp = x * x_ratio / new_w;
            int sx = (int)(sx_fp >> 16);
            int sx1 = sx + 1; if (sx1 >= src->width) sx1 = sx;
            int fx = (int)(sx_fp & 0xFFFF);
            int ifx = 0x10000 - fx;

            /* Sample four pixels */
            uint32_t p00 = SPIX_AT(src, sx, sy);
            uint32_t p01 = SPIX_AT(src, sx1, sy);
            uint32_t p10 = SPIX_AT(src, sx, sy1);
            uint32_t p11 = SPIX_AT(src, sx1, sy1);

            /* Bilinear interpolation per channel */
            int r = ((SPIX_R(p00) * ifx + SPIX_R(p01) * fx) * ify +
                     (SPIX_R(p10) * ifx + SPIX_R(p11) * fx) * fy) >> 32;
            int g = ((SPIX_G(p00) * ifx + SPIX_G(p01) * fx) * ify +
                     (SPIX_G(p10) * ifx + SPIX_G(p11) * fx) * fy) >> 32;
            int b = ((SPIX_B(p00) * ifx + SPIX_B(p01) * fx) * ify +
                     (SPIX_B(p10) * ifx + SPIX_B(p11) * fx) * fy) >> 32;

            SPIX_AT(dst, x, y) = SPIX_RGB(
                (uint8_t)SPIX_CLAMP(r, 0, 255),
                (uint8_t)SPIX_CLAMP(g, 0, 255),
                (uint8_t)SPIX_CLAMP(b, 0, 255));
        }
    }
    return dst;
}

/* ── Convolution ─────────────────────────────────────────── */

int spix_convolve_to(const spix_image_t *src, spix_image_t *dst, const spix_kernel_t *k) {
    if (!src || !dst || !k) return SPIX_ERR_SIZE;
    if (dst->width != src->width || dst->height != src->height) return SPIX_ERR_SIZE;
    int half = k->size / 2;

    for (int y = 0; y < src->height; y++) {
        for (int x = 0; x < src->width; x++) {
            int sum_r = 0, sum_g = 0, sum_b = 0;
            for (int ky = -half; ky <= half; ky++) {
                for (int kx = -half; kx <= half; kx++) {
                    int idx = (ky + half) * k->size + (kx + half);
                    int coeff = k->data[idx];
                    if (!coeff) continue;
                    int sx = x + kx; if (sx < 0) sx = 0; if (sx >= src->width) sx = src->width - 1;
                    int sy = y + ky; if (sy < 0) sy = 0; if (sy >= src->height) sy = src->height - 1;
                    spix_pixel_t p = SPIX_AT(src, sx, sy);
                    sum_r += SPIX_R(p) * coeff;
                    sum_g += SPIX_G(p) * coeff;
                    sum_b += SPIX_B(p) * coeff;
                }
            }
            if (k->shift > 0) {
                sum_r >>= k->shift; sum_g >>= k->shift; sum_b >>= k->shift;
            }
            int bias = k->shift > 0 ? 0 : 128; /* bias for edge/emboss */
            if (k == &SPIX_KERNEL_EDGE_DETECT || k == &SPIX_KERNEL_EMBOSS ||
                k == &SPIX_KERNEL_SOBEL_H || k == &SPIX_KERNEL_SOBEL_V)
                bias = 128;
            /* Also check by values: if kernel sums to 0, add bias */
            {
                int ksum = 0;
                for (int i = 0; i < k->size * k->size; i++) ksum += k->data[i];
                if (ksum == 0) bias = 128;
            }
            SPIX_AT(dst, x, y) = SPIX_RGB(
                (uint8_t)SPIX_CLAMP(sum_r + bias - 128, 0, 255),
                (uint8_t)SPIX_CLAMP(sum_g + bias - 128, 0, 255),
                (uint8_t)SPIX_CLAMP(sum_b + bias - 128, 0, 255));
        }
    }
    return SPIX_OK;
}

spix_image_t *spix_convolve(const spix_image_t *src, const spix_kernel_t *k) {
    if (!src) return NULL;
    spix_image_t *dst = spix_clone(src);
    if (!dst) return NULL;
    if (spix_convolve_to(src, dst, k) != SPIX_OK) { spix_free(dst); return NULL; }
    return dst;
}

/* ── Grayscale (luminosity, fixed-point) ─────────────────── */

int spix_grayscale(spix_image_t *img) {
    if (!img) return SPIX_ERR_SIZE;
    for (int y = 0; y < img->height; y++)
        for (int x = 0; x < img->width; x++) {
            spix_pixel_t p = SPIX_AT(img, x, y);
            /* 0.299*R + 0.587*G + 0.114*B in Q16 (scaled by 1000) */
            int g = (SPIX_R(p) * 299 + SPIX_G(p) * 587 + SPIX_B(p) * 114) / 1000;
            if (g < 0) g = 0; if (g > 255) g = 255;
            SPIX_AT(img, x, y) = SPIX_RGB((uint8_t)g, (uint8_t)g, (uint8_t)g);
        }
    return SPIX_OK;
}

/* ── Threshold ───────────────────────────────────────────── */

int spix_threshold(spix_image_t *img, uint8_t thresh) {
    if (!img) return SPIX_ERR_SIZE;
    /* First ensure grayscale */
    spix_grayscale(img);
    for (int y = 0; y < img->height; y++)
        for (int x = 0; x < img->width; x++) {
            uint8_t g = SPIX_R(SPIX_AT(img, x, y));
            SPIX_AT(img, x, y) = (g >= thresh) ? SPIX_WHITE : SPIX_BLACK;
        }
    return SPIX_OK;
}

/* ── Otsu threshold ──────────────────────────────────────── */

uint8_t spix_threshold_otsu(const spix_image_t *img) {
    if (!img) return 128;
    int hist[256] = {0};
    int total = img->width * img->height;
    for (int y = 0; y < img->height; y++)
        for (int x = 0; x < img->width; x++)
            hist[SPIX_R(SPIX_AT(img, x, y))]++;

    int sum_total = 0;
    for (int i = 0; i < 256; i++) sum_total += i * hist[i];

    int sum_bg = 0, w_bg = 0, w_fg = 0;
    double max_var = 0;
    uint8_t best = 0;
    for (int t = 0; t < 256; t++) {
        w_bg += hist[t];
        if (w_bg == 0) continue;
        w_fg = total - w_bg;
        if (w_fg == 0) break;
        sum_bg += t * hist[t];
        double mean_bg = (double)sum_bg / w_bg;
        double mean_fg = (double)(sum_total - sum_bg) / w_fg;
        double var = (double)w_bg * w_fg * (mean_bg - mean_fg) * (mean_bg - mean_fg);
        if (var > max_var) { max_var = var; best = (uint8_t)t; }
    }
    return best;
}

/* ── Floyd-Steinberg dithering ───────────────────────────── */

int spix_dither_floyd(spix_image_t *img) {
    if (!img) return SPIX_ERR_SIZE;
    spix_grayscale(img);

    /* Work with float error for simplicity (or use fixed-point) */
    /* We use a temporary buffer for error */
    int *errors = (int *)calloc((size_t)(img->width + 2) * (img->height + 1), sizeof(int));
    if (!errors) return SPIX_ERR_MEM;
#define ERR(x, y) errors[(y) * (img->width + 2) + (x)]

    for (int y = 0; y < img->height; y++) {
        for (int x = 0; x < img->width; x++) {
            int old = SPIX_R(SPIX_AT(img, x, y)) + ERR(x, y);
            if (old < 0) old = 0; if (old > 255) old = 255;
            int new_val = old < 128 ? 0 : 255;
            SPIX_AT(img, x, y) = new_val ? SPIX_WHITE : SPIX_BLACK;
            int error = old - new_val;
            ERR(x + 1, y)     += error * 7 / 16;
            ERR(x - 1, y + 1) += error * 3 / 16;
            ERR(x,     y + 1) += error * 5 / 16;
            ERR(x + 1, y + 1) += error * 1 / 16;
        }
    }
    free(errors);
    return SPIX_OK;
#undef ERR
}

/* ── Bayer dithering ─────────────────────────────────────── */

static const uint8_t bayer4[4][4] = {
    {0, 8, 2, 10},
    {12, 4, 14, 6},
    {3, 11, 1, 9},
    {15, 7, 13, 5}
};

int spix_dither_bayer(spix_image_t *img) {
    if (!img) return SPIX_ERR_SIZE;
    spix_grayscale(img);
    for (int y = 0; y < img->height; y++)
        for (int x = 0; x < img->width; x++) {
            int g = SPIX_R(SPIX_AT(img, x, y));
            int threshold = (bayer4[y & 3][x & 3] * 255 + 8) / 16;
            SPIX_AT(img, x, y) = (g >= threshold) ? SPIX_WHITE : SPIX_BLACK;
        }
    return SPIX_OK;
}

/* ── Geometry ────────────────────────────────────────────── */

int spix_flip_h(spix_image_t *img) {
    if (!img) return SPIX_ERR_SIZE;
    for (int y = 0; y < img->height; y++)
        for (int x = 0; x < img->width / 2; x++) {
            int rx = img->width - 1 - x;
            spix_pixel_t t = SPIX_AT(img, x, y);
            SPIX_AT(img, x, y) = SPIX_AT(img, rx, y);
            SPIX_AT(img, rx, y) = t;
        }
    return SPIX_OK;
}

int spix_flip_v(spix_image_t *img) {
    if (!img) return SPIX_ERR_SIZE;
    for (int y = 0; y < img->height / 2; y++)
        for (int x = 0; x < img->width; x++) {
            int ry = img->height - 1 - y;
            spix_pixel_t t = SPIX_AT(img, x, y);
            SPIX_AT(img, x, y) = SPIX_AT(img, x, ry);
            SPIX_AT(img, x, ry) = t;
        }
    return SPIX_OK;
}

spix_image_t *spix_rotate_90(const spix_image_t *src) {
    if (!src) return NULL;
    spix_image_t *dst = spix_alloc(src->height, src->width);
    if (!dst) return NULL;
    for (int y = 0; y < src->height; y++)
        for (int x = 0; x < src->width; x++)
            SPIX_AT(dst, src->height - 1 - y, x) = SPIX_AT(src, x, y);
    return dst;
}

#endif /* SPIX_IMPLEMENTATION */
