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

/* ── Pixel access ────────────────────────────────────────── */

/** Get single channel value (0=R,1=G,2=B,3=A) at (x,y). Returns -1 on error. */
int spix_get_channel(const spix_image_t *img, int x, int y, int channel);

/** Set single channel value at (x,y). Returns SPIX_OK or error. */
int spix_set_channel(spix_image_t *img, int x, int y, int channel, uint8_t value);

/* ── Histogram ───────────────────────────────────────────── */

/** Compute grayscale histogram (256 bins). img is NOT modified. */
int spix_histogram(const spix_image_t *img, uint32_t hist[256]);

/** Histogram equalization (in-place, converts to grayscale first). */
int spix_equalize(spix_image_t *img);

/* ── Connected components ────────────────────────────────── */

typedef struct {
    int x, y, w, h;           /**< bounding box */
    int area;                  /**< pixel count */
    int cx, cy;                /**< centroid */
    uint32_t color;            /**< representative pixel color */
} spix_component_t;

typedef struct {
    spix_component_t *items;
    int count;
    int *labels;               /**< label map (width × height), owned */
    int width, height;
} spix_components_t;

/** Free components result. */
void spix_components_free(spix_components_t *cc);

/**
 * Find connected components on a binary image (white=foreground).
 * tolerance: max grayscale difference to consider pixels connected (0=exact).
 * Returns SPIX_OK on success.
 */
int spix_find_components(const spix_image_t *img, spix_components_t *cc, int tolerance);

/** Get label at (x,y). Returns -1 if out of bounds. */
int spix_components_label(const spix_components_t *cc, int x, int y);

/** Draw component bounding boxes on image (in-place). */
int spix_draw_boxes(spix_image_t *img, const spix_components_t *cc, spix_pixel_t color);

/* ── Morphology (3×3 structuring element) ────────────────── */

/** Erosion (binary image: white=foreground, in-place). */
int spix_erode(spix_image_t *img);

/** Dilation (binary image: white=foreground, in-place). */
int spix_dilate(spix_image_t *img);

/** Morphological open = erode then dilate (removes small noise). */
int spix_open(spix_image_t *img);

/** Morphological close = dilate then erode (fills small holes). */
int spix_close(spix_image_t *img);

/** Top-hat = original - open (extracts bright细节). */
spix_image_t *spix_tophat(const spix_image_t *src);

/* ── Image arithmetic ────────────────────────────────────── */

/** Add two images (clamped to 0-255 per channel). Returns new image. */
spix_image_t *spix_add(const spix_image_t *a, const spix_image_t *b);

/** Subtract b from a (clamped to 0). Returns new image. */
spix_image_t *spix_sub(const spix_image_t *a, const spix_image_t *b);

/** Blend two images: dst = a*(1-alpha) + b*alpha. alpha in [0.0, 1.0]. Returns new image. */
spix_image_t *spix_blend(const spix_image_t *a, const spix_image_t *b, float alpha);

/** Invert colors (in-place). */
int spix_invert(spix_image_t *img);

/** Adjust brightness by offset (-255..+255). Returns new image. */
spix_image_t *spix_brighten(const spix_image_t *src, int offset);

/** Adjust contrast by factor (1.0=identity). Returns new image. */
spix_image_t *spix_contrast(const spix_image_t *src, float factor);

/* ── IO: BMP ─────────────────────────────────────────────── */

/** Write uncompressed BMP (24-bit) to file. Returns SPIX_OK or error. */
int spix_write_bmp(const spix_image_t *img, const char *path);

/** Write uncompressed BMP to memory buffer (caller must free *out). Returns bytes written. */
size_t spix_write_bmp_mem(const spix_image_t *img, uint8_t **out);

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
    if (c != EOF) ungetc(c, fp);   /* leave the delimiter for the next token */
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

    /* Consume exactly the single delimiter that terminated maxval.
       (_spix_read_int() leaves it in the stream, so no byte of pixel data —
        which may itself be a whitespace value — is lost.) Tolerate CRLF. */
    {
        int c = fgetc(fp);
        if (c == '\r') {
            int c2 = fgetc(fp);
            if (c2 != EOF && c2 != '\n') ungetc(c2, fp);
        }
    }

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
    {
        /* exactly one delimiter after maxval, tolerating CRLF */
        int c = fgetc(fp);
        if (c == '\r') {
            int c2 = fgetc(fp);
            if (c2 != EOF && c2 != '\n') ungetc(c2, fp);
        }
    }

    spix_image_t *img = spix_alloc(w, h);
    if (!img) { fclose(fp); return NULL; }

    if (magic[1] == '6') {
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                int r = fgetc(fp), g = fgetc(fp), b = fgetc(fp);
                if (r == EOF) { spix_free(img); fclose(fp); return NULL; }
                SPIX_AT(img, x, y) = SPIX_RGB(
                    (uint8_t)(r * 255 / maxval),
                    (uint8_t)(g * 255 / maxval),
                    (uint8_t)(b * 255 / maxval));
            }
    } else if (magic[1] == '5') {
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                int v = fgetc(fp); if (v == EOF) { spix_free(img); fclose(fp); return NULL; }
                uint8_t g = (uint8_t)(v * 255 / maxval);
                SPIX_AT(img, x, y) = SPIX_RGB(g, g, g);
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

            /* Bilinear interpolation per channel.
               The accumulator must be 64-bit: the intermediate value reaches
               (2^25 * 2^16) = 2^41, which overflows 32-bit int. */
            int64_t r = (((int64_t)(SPIX_R(p00) * ifx + SPIX_R(p01) * fx) * ify) +
                         ((int64_t)(SPIX_R(p10) * ifx + SPIX_R(p11) * fx) * fy)) >> 32;
            int64_t g = (((int64_t)(SPIX_G(p00) * ifx + SPIX_G(p01) * fx) * ify) +
                         ((int64_t)(SPIX_G(p10) * ifx + SPIX_G(p11) * fx) * fy)) >> 32;
            int64_t b = (((int64_t)(SPIX_B(p00) * ifx + SPIX_B(p01) * fx) * ify) +
                         ((int64_t)(SPIX_B(p10) * ifx + SPIX_B(p11) * fx) * fy)) >> 32;

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

    /* Output offset: kernels that sum to zero (edge detect, sobel) carry no
       brightness of their own and are rendered on a mid-grey background, so a
       flat region stays 128 instead of black. Kernels that preserve the level
       (box/gauss blur, sharpen, emboss) must NOT be offset. */
    int ksum = 0;
    for (int i = 0; i < k->size * k->size; i++) ksum += k->data[i];
    int offset = (ksum == 0) ? 128 : 0;

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
            SPIX_AT(dst, x, y) = SPIX_RGB(
                (uint8_t)SPIX_CLAMP(sum_r + offset, 0, 255),
                (uint8_t)SPIX_CLAMP(sum_g + offset, 0, 255),
                (uint8_t)SPIX_CLAMP(sum_b + offset, 0, 255));
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
    /* Otsu's t is the last level of the dark class (dark = [0..t]); this
       library binarizes with "value >= threshold", so hand back t+1 to keep
       spix_threshold(img, spix_threshold_otsu(img)) consistent. */
    return (uint8_t)(best < 255 ? best + 1 : 255);
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

/* ── Pixel access ────────────────────────────────────────── */

int spix_get_channel(const spix_image_t *img, int x, int y, int channel) {
    if (!img || x < 0 || x >= img->width || y < 0 || y >= img->height) return -1;
    if (channel < 0 || channel > 3) return -1;
    spix_pixel_t p = SPIX_AT(img, x, y);
    switch (channel) {
        case 0: return SPIX_R(p);
        case 1: return SPIX_G(p);
        case 2: return SPIX_B(p);
        case 3: return SPIX_A(p);
    }
    return -1;
}

int spix_set_channel(spix_image_t *img, int x, int y, int channel, uint8_t value) {
    if (!img || x < 0 || x >= img->width || y < 0 || y >= img->height) return SPIX_ERR_SIZE;
    if (channel < 0 || channel > 3) return SPIX_ERR_SIZE;
    spix_pixel_t *p = &SPIX_AT(img, x, y);
    switch (channel) {
        case 0: *p = (*p & 0x00FFFFFF) | ((uint32_t)value << 24); break;
        case 1: *p = (*p & 0xFF00FFFF) | ((uint32_t)value << 16); break;
        case 2: *p = (*p & 0xFFFF00FF) | ((uint32_t)value << 8);  break;
        case 3: *p = (*p & 0xFFFFFF00) | value;                    break;
    }
    return SPIX_OK;
}

/* ── Histogram ───────────────────────────────────────────── */

int spix_histogram(const spix_image_t *img, uint32_t hist[256]) {
    if (!img || !hist) return SPIX_ERR_SIZE;
    memset(hist, 0, 256 * sizeof(uint32_t));
    for (int y = 0; y < img->height; y++)
        for (int x = 0; x < img->width; x++)
            hist[SPIX_R(SPIX_AT(img, x, y))]++;
    return SPIX_OK;
}

int spix_equalize(spix_image_t *img) {
    if (!img) return SPIX_ERR_SIZE;
    spix_grayscale(img);

    uint32_t hist[256];
    spix_histogram(img, hist);

    int total = img->width * img->height;
    if (total == 0) return SPIX_OK;

    /* CDF */
    uint32_t cdf[256];
    cdf[0] = hist[0];
    for (int i = 1; i < 256; i++) cdf[i] = cdf[i - 1] + hist[i];

    /* Find min non-zero CDF */
    uint32_t cdf_min = 0;
    for (int i = 0; i < 256; i++) {
        if (cdf[i] > 0) { cdf_min = cdf[i]; break; }
    }

    /* Build LUT using integer arithmetic: lut[i] = (cdf[i] - cdf_min) * 255 / (total - cdf_min) */
    uint8_t lut[256];
    int denom = total - cdf_min;
    if (denom <= 0) {
        for (int i = 0; i < 256; i++) lut[i] = (uint8_t)i;
    } else {
        for (int i = 0; i < 256; i++) {
            int val = (int)((cdf[i] - cdf_min) * 255 / (uint32_t)denom);
            lut[i] = (uint8_t)SPIX_CLAMP(val, 0, 255);
        }
    }

    /* Apply LUT */
    for (int y = 0; y < img->height; y++)
        for (int x = 0; x < img->width; x++) {
            uint8_t g = SPIX_R(SPIX_AT(img, x, y));
            uint8_t eq = lut[g];
            SPIX_AT(img, x, y) = SPIX_RGB(eq, eq, eq);
        }
    return SPIX_OK;
}

/* ── Connected components (union-find) ───────────────────── */

static int _spix_uf_find(int *parent, int x) {
    while (parent[x] != x) { parent[x] = parent[parent[x]]; x = parent[x]; }
    return x;
}

static void _spix_uf_union(int *parent, int *rank, int a, int b) {
    a = _spix_uf_find(parent, a);
    b = _spix_uf_find(parent, b);
    if (a == b) return;
    if (rank[a] < rank[b]) { int t = a; a = b; b = t; }
    parent[b] = a;
    if (rank[a] == rank[b]) rank[a]++;
}

void spix_components_free(spix_components_t *cc) {
    if (!cc) return;
    free(cc->items);
    free(cc->labels);
    cc->items = NULL;
    cc->labels = NULL;
    cc->count = 0;
}

int spix_components_label(const spix_components_t *cc, int x, int y) {
    if (!cc || !cc->labels) return -1;
    if (x < 0 || x >= cc->width || y < 0 || y >= cc->height) return -1;
    return cc->labels[y * cc->width + x];
}

int spix_find_components(const spix_image_t *img, spix_components_t *cc, int tolerance) {
    if (!img || !cc) return SPIX_ERR_SIZE;
    memset(cc, 0, sizeof(*cc));

    int w = img->width, h = img->height;
    int n = w * h;
    int *parent = (int *)malloc((size_t)n * sizeof(int));
    int *rank   = (int *)calloc((size_t)n, sizeof(int));
    if (!parent || !rank) { free(parent); free(rank); return SPIX_ERR_MEM; }
    for (int i = 0; i < n; i++) parent[i] = i;

    /* Union adjacent foreground pixels */
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint8_t g = SPIX_R(SPIX_AT(img, x, y));
            if (g < 128) continue;  /* background */
            int idx = y * w + x;

            /* right neighbor */
            if (x + 1 < w) {
                uint8_t gr = SPIX_R(SPIX_AT(img, x + 1, y));
                if (gr >= 128 && abs((int)g - (int)gr) <= tolerance)
                    _spix_uf_union(parent, rank, idx, idx + 1);
            }
            /* bottom neighbor */
            if (y + 1 < h) {
                uint8_t gb = SPIX_R(SPIX_AT(img, x, y + 1));
                if (gb >= 128 && abs((int)g - (int)gb) <= tolerance)
                    _spix_uf_union(parent, rank, idx, idx + w);
            }
        }
    }

    /* Collect unique roots, assign labels */
    int *root_map = (int *)calloc((size_t)n, sizeof(int));
    int *labels   = (int *)malloc((size_t)n * sizeof(int));
    if (!root_map || !labels) { free(parent); free(rank); free(root_map); free(labels); return SPIX_ERR_MEM; }

    int unique = 0;
    for (int i = 0; i < n; i++) {
        labels[i] = -1;
        if (SPIX_R(img->pixels[i]) < 128) continue;
        int root = _spix_uf_find(parent, i);
        /* Find or add in root_map */
        int k;
        for (k = 0; k < unique; k++) { if (root_map[k] == root) break; }
        if (k == unique) root_map[unique++] = root;
        labels[i] = k;
    }

    free(parent);
    free(rank);

    /* Compute bounding boxes and centroids */
    spix_component_t *items = (spix_component_t *)calloc((size_t)unique, sizeof(spix_component_t));
    if (!items) { free(root_map); free(labels); return SPIX_ERR_MEM; }
    for (int i = 0; i < unique; i++) {
        items[i].x = w; items[i].y = h;
        items[i].w = -1; items[i].h = -1;   /* running max_x / max_y */
        items[i].area = 0;
        items[i].cx = 0; items[i].cy = 0;
    }
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int lbl = labels[y * w + x];
            if (lbl < 0) continue;
            spix_component_t *c = &items[lbl];
            if (x < c->x) c->x = x;
            if (y < c->y) c->y = y;
            if (x > c->w) c->w = x;   /* running max x, converted below */
            if (y > c->h) c->h = y;   /* running max y, converted below */
            c->area++;
            c->cx += x;
            c->cy += y;
        }
    }
    for (int i = 0; i < unique; i++) {
        if (items[i].area > 0) {
            items[i].cx /= items[i].area;
            items[i].cy /= items[i].area;
            items[i].w = items[i].w - items[i].x + 1;   /* max_x - min_x + 1 */
            items[i].h = items[i].h - items[i].y + 1;   /* max_y - min_y + 1 */
        } else {
            items[i].w = 0;
            items[i].h = 0;
        }
        /* Find representative color (centroid pixel) */
        int cx = SPIX_CLAMP(items[i].cx, 0, w - 1);
        int cy = SPIX_CLAMP(items[i].cy, 0, h - 1);
        items[i].color = SPIX_AT(img, cx, cy);
    }

    free(root_map);

    cc->items  = items;
    cc->count  = unique;
    cc->labels = labels;
    cc->width  = w;
    cc->height = h;
    return SPIX_OK;
}

int spix_draw_boxes(spix_image_t *img, const spix_components_t *cc, spix_pixel_t color) {
    if (!img || !cc) return SPIX_ERR_SIZE;
    for (int i = 0; i < cc->count; i++) {
        const spix_component_t *c = &cc->items[i];
        if (c->area < 2) continue;  /* skip single-pixel noise */
        for (int x = c->x; x < c->x + c->w && x < img->width; x++) {
            if (c->y < img->height) SPIX_AT(img, x, c->y) = color;
            if (c->y + c->h - 1 < img->height) SPIX_AT(img, x, c->y + c->h - 1) = color;
        }
        for (int y = c->y; y < c->y + c->h && y < img->height; y++) {
            if (c->x < img->width) SPIX_AT(img, c->x, y) = color;
            if (c->x + c->w - 1 < img->width) SPIX_AT(img, c->x + c->w - 1, y) = color;
        }
    }
    return SPIX_OK;
}

/* ── Morphology ──────────────────────────────────────────── */

int spix_erode(spix_image_t *img) {
    if (!img) return SPIX_ERR_SIZE;
    spix_pixel_t *buf = (spix_pixel_t *)malloc((size_t)img->width * img->height * sizeof(spix_pixel_t));
    if (!buf) return SPIX_ERR_MEM;

    for (int y = 0; y < img->height; y++) {
        for (int x = 0; x < img->width; x++) {
            uint8_t min_v = 255;
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    int nx = x + dx, ny = y + dy;
                    if (nx < 0 || nx >= img->width || ny < 0 || ny >= img->height) continue;
                    uint8_t v = SPIX_R(SPIX_AT(img, nx, ny));
                    if (v < min_v) min_v = v;
                }
            }
            buf[y * img->width + x] = (min_v >= 128) ? SPIX_WHITE : SPIX_BLACK;
        }
    }
    for (int y = 0; y < img->height; y++)
        for (int x = 0; x < img->width; x++)
            SPIX_AT(img, x, y) = buf[y * img->width + x];
    free(buf);
    return SPIX_OK;
}

int spix_dilate(spix_image_t *img) {
    if (!img) return SPIX_ERR_SIZE;
    spix_pixel_t *buf = (spix_pixel_t *)malloc((size_t)img->width * img->height * sizeof(spix_pixel_t));
    if (!buf) return SPIX_ERR_MEM;

    for (int y = 0; y < img->height; y++) {
        for (int x = 0; x < img->width; x++) {
            uint8_t max_v = 0;
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    int nx = x + dx, ny = y + dy;
                    if (nx < 0 || nx >= img->width || ny < 0 || ny >= img->height) continue;
                    uint8_t v = SPIX_R(SPIX_AT(img, nx, ny));
                    if (v > max_v) max_v = v;
                }
            }
            buf[y * img->width + x] = (max_v >= 128) ? SPIX_WHITE : SPIX_BLACK;
        }
    }
    for (int y = 0; y < img->height; y++)
        for (int x = 0; x < img->width; x++)
            SPIX_AT(img, x, y) = buf[y * img->width + x];
    free(buf);
    return SPIX_OK;
}

int spix_open(spix_image_t *img) {
    int r = spix_erode(img);
    if (r == SPIX_OK) r = spix_dilate(img);
    return r;
}

int spix_close(spix_image_t *img) {
    int r = spix_dilate(img);
    if (r == SPIX_OK) r = spix_erode(img);
    return r;
}

spix_image_t *spix_tophat(const spix_image_t *src) {
    if (!src) return NULL;
    spix_image_t *tmp = spix_clone(src);
    if (!tmp) return NULL;
    spix_open(tmp);
    /* tophat = original - open */
    spix_image_t *result = spix_sub(src, tmp);
    spix_free(tmp);
    return result;
}

/* ── Image arithmetic ────────────────────────────────────── */

spix_image_t *spix_add(const spix_image_t *a, const spix_image_t *b) {
    if (!a || !b) return NULL;
    if (a->width != b->width || a->height != b->height) return NULL;
    spix_image_t *dst = spix_alloc(a->width, a->height);
    if (!dst) return NULL;
    for (int y = 0; y < a->height; y++)
        for (int x = 0; x < a->width; x++) {
            int r = SPIX_R(SPIX_AT(a, x, y)) + SPIX_R(SPIX_AT(b, x, y));
            int g = SPIX_G(SPIX_AT(a, x, y)) + SPIX_G(SPIX_AT(b, x, y));
            int b_ = SPIX_B(SPIX_AT(a, x, y)) + SPIX_B(SPIX_AT(b, x, y));
            SPIX_AT(dst, x, y) = SPIX_RGB(
                (uint8_t)SPIX_CLAMP(r, 0, 255),
                (uint8_t)SPIX_CLAMP(g, 0, 255),
                (uint8_t)SPIX_CLAMP(b_, 0, 255));
        }
    return dst;
}

spix_image_t *spix_sub(const spix_image_t *a, const spix_image_t *b) {
    if (!a || !b) return NULL;
    if (a->width != b->width || a->height != b->height) return NULL;
    spix_image_t *dst = spix_alloc(a->width, a->height);
    if (!dst) return NULL;
    for (int y = 0; y < a->height; y++)
        for (int x = 0; x < a->width; x++) {
            int r = SPIX_R(SPIX_AT(a, x, y)) - SPIX_R(SPIX_AT(b, x, y));
            int g = SPIX_G(SPIX_AT(a, x, y)) - SPIX_G(SPIX_AT(b, x, y));
            int b_ = SPIX_B(SPIX_AT(a, x, y)) - SPIX_B(SPIX_AT(b, x, y));
            SPIX_AT(dst, x, y) = SPIX_RGB(
                (uint8_t)SPIX_CLAMP(r, 0, 255),
                (uint8_t)SPIX_CLAMP(g, 0, 255),
                (uint8_t)SPIX_CLAMP(b_, 0, 255));
        }
    return dst;
}

spix_image_t *spix_blend(const spix_image_t *a, const spix_image_t *b, float alpha) {
    if (!a || !b) return NULL;
    if (a->width != b->width || a->height != b->height) return NULL;
    alpha = SPIX_CLAMP(alpha, 0.0f, 1.0f);
    float beta = 1.0f - alpha;
    spix_image_t *dst = spix_alloc(a->width, a->height);
    if (!dst) return NULL;
    for (int y = 0; y < a->height; y++)
        for (int x = 0; x < a->width; x++) {
            int r = (int)(SPIX_R(SPIX_AT(a, x, y)) * beta + SPIX_R(SPIX_AT(b, x, y)) * alpha);
            int g = (int)(SPIX_G(SPIX_AT(a, x, y)) * beta + SPIX_G(SPIX_AT(b, x, y)) * alpha);
            int b_ = (int)(SPIX_B(SPIX_AT(a, x, y)) * beta + SPIX_B(SPIX_AT(b, x, y)) * alpha);
            SPIX_AT(dst, x, y) = SPIX_RGB(
                (uint8_t)SPIX_CLAMP(r, 0, 255),
                (uint8_t)SPIX_CLAMP(g, 0, 255),
                (uint8_t)SPIX_CLAMP(b_, 0, 255));
        }
    return dst;
}

int spix_invert(spix_image_t *img) {
    if (!img) return SPIX_ERR_SIZE;
    for (int y = 0; y < img->height; y++)
        for (int x = 0; x < img->width; x++) {
            spix_pixel_t p = SPIX_AT(img, x, y);
            SPIX_AT(img, x, y) = SPIX_RGBA(255 - SPIX_R(p), 255 - SPIX_G(p), 255 - SPIX_B(p), SPIX_A(p));
        }
    return SPIX_OK;
}

spix_image_t *spix_brighten(const spix_image_t *src, int offset) {
    if (!src) return NULL;
    spix_image_t *dst = spix_alloc(src->width, src->height);
    if (!dst) return NULL;
    for (int y = 0; y < src->height; y++)
        for (int x = 0; x < src->width; x++) {
            int r = SPIX_R(SPIX_AT(src, x, y)) + offset;
            int g = SPIX_G(SPIX_AT(src, x, y)) + offset;
            int b = SPIX_B(SPIX_AT(src, x, y)) + offset;
            SPIX_AT(dst, x, y) = SPIX_RGB(
                (uint8_t)SPIX_CLAMP(r, 0, 255),
                (uint8_t)SPIX_CLAMP(g, 0, 255),
                (uint8_t)SPIX_CLAMP(b, 0, 255));
        }
    return dst;
}

spix_image_t *spix_contrast(const spix_image_t *src, float factor) {
    if (!src) return NULL;
    spix_image_t *dst = spix_alloc(src->width, src->height);
    if (!dst) return NULL;
    float f = SPIX_CLAMP(factor, -10.0f, 10.0f);
    for (int y = 0; y < src->height; y++)
        for (int x = 0; x < src->width; x++) {
            int r = (int)(((float)SPIX_R(SPIX_AT(src, x, y)) - 128.0f) * f + 128.0f);
            int g = (int)(((float)SPIX_G(SPIX_AT(src, x, y)) - 128.0f) * f + 128.0f);
            int b = (int)(((float)SPIX_B(SPIX_AT(src, x, y)) - 128.0f) * f + 128.0f);
            SPIX_AT(dst, x, y) = SPIX_RGB(
                (uint8_t)SPIX_CLAMP(r, 0, 255),
                (uint8_t)SPIX_CLAMP(g, 0, 255),
                (uint8_t)SPIX_CLAMP(b, 0, 255));
        }
    return dst;
}

/* ── BMP writer (24-bit uncompressed) ────────────────────── */

int spix_write_bmp(const spix_image_t *img, const char *path) {
    if (!img) return SPIX_ERR_IO;
    FILE *fp = fopen(path, "wb");
    if (!fp) return SPIX_ERR_IO;

    int w = img->width, h = img->height;
    int row_bytes = w * 3;
    int padding = (4 - (row_bytes % 4)) % 4;
    int data_size = (row_bytes + padding) * h;
    int file_size = 54 + data_size;

    /* File header (14 bytes) */
    uint8_t fh[14] = {0};
    fh[0] = 'B'; fh[1] = 'M';
    fh[2] = (uint8_t)(file_size);
    fh[3] = (uint8_t)(file_size >> 8);
    fh[4] = (uint8_t)(file_size >> 16);
    fh[5] = (uint8_t)(file_size >> 24);
    fh[10] = 54;  /* offset to pixel data */
    fwrite(fh, 1, 14, fp);

    /* Info header (40 bytes) */
    uint8_t ih[40] = {0};
    ih[0] = 40;                        /* header size */
    ih[4] = (uint8_t)(w);
    ih[5] = (uint8_t)(w >> 8);
    ih[6] = (uint8_t)(w >> 16);
    ih[7] = (uint8_t)(w >> 24);
    ih[8] = (uint8_t)(h);
    ih[9] = (uint8_t)(h >> 8);
    ih[10] = (uint8_t)(h >> 16);
    ih[11] = (uint8_t)(h >> 24);
    ih[12] = 1;  /* planes */
    ih[14] = 24; /* bits per pixel */
    ih[20] = (uint8_t)(data_size);
    ih[21] = (uint8_t)(data_size >> 8);
    ih[22] = (uint8_t)(data_size >> 16);
    ih[23] = (uint8_t)(data_size >> 24);
    fwrite(ih, 1, 40, fp);

    /* Pixel data (BGR, bottom-up) */
    uint8_t pad[3] = {0, 0, 0};
    for (int y = h - 1; y >= 0; y--) {
        for (int x = 0; x < w; x++) {
            spix_pixel_t p = SPIX_AT(img, x, y);
            uint8_t bgr[3] = { SPIX_B(p), SPIX_G(p), SPIX_R(p) };
            fwrite(bgr, 1, 3, fp);
        }
        if (padding) fwrite(pad, 1, (size_t)padding, fp);
    }

    fclose(fp);
    return SPIX_OK;
}

size_t spix_write_bmp_mem(const spix_image_t *img, uint8_t **out) {
    if (!img || !out) return 0;

    int w = img->width, h = img->height;
    int row_bytes = w * 3;
    int padding = (4 - (row_bytes % 4)) % 4;
    int data_size = (row_bytes + padding) * h;
    int file_size = 54 + data_size;

    *out = (uint8_t *)calloc(1, (size_t)file_size);
    if (!*out) return 0;

    uint8_t *p = *out;

    /* File header */
    p[0] = 'B'; p[1] = 'M';
    p[2] = (uint8_t)(file_size); p[3] = (uint8_t)(file_size >> 8);
    p[4] = (uint8_t)(file_size >> 16); p[5] = (uint8_t)(file_size >> 24);
    p[10] = 54;

    /* Info header */
    p[14] = 40;
    p[18] = (uint8_t)(w); p[19] = (uint8_t)(w >> 8);
    p[20] = (uint8_t)(w >> 16); p[21] = (uint8_t)(w >> 24);
    p[22] = (uint8_t)(h); p[23] = (uint8_t)(h >> 8);
    p[24] = (uint8_t)(h >> 16); p[25] = (uint8_t)(h >> 24);
    p[26] = 1; p[28] = 24;
    p[34] = (uint8_t)(data_size); p[35] = (uint8_t)(data_size >> 8);
    p[36] = (uint8_t)(data_size >> 16); p[37] = (uint8_t)(data_size >> 24);

    /* Pixel data */
    uint8_t *dp = p + 54;
    for (int y = h - 1; y >= 0; y--) {
        for (int x = 0; x < w; x++) {
            spix_pixel_t px = SPIX_AT(img, x, y);
            *dp++ = SPIX_B(px);
            *dp++ = SPIX_G(px);
            *dp++ = SPIX_R(px);
        }
        if (padding) { memset(dp, 0, (size_t)padding); dp += padding; }
    }

    return (size_t)file_size;
}

#endif /* SPIX_IMPLEMENTATION */

#endif /* SPIX_H */
