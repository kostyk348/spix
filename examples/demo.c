/**
 * demo.c — spix image processing library demo
 *
 * Generates a test pattern and applies various filters:
 *   demo              → processes `input.ppm` → writes filtered outputs
 *   demo generate     → generates test pattern → input.ppm
 *
 * Build:
 *   cc -O2 -o demo demo.c
 *   ./demo generate   # create test image
 *   ./demo             # apply filters
 */
#define SPIX_IMPLEMENTATION
#include "../spix.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ── Generate test pattern ───────────────────────────────── */

static void gen_test_pattern(const char *path) {
    int w = 512, h = 512;
    spix_image_t *img = spix_alloc(w, h);

    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            /* Smooth color gradient + sharp edges + text-like regions */
            int r = (x * 255) / w;
            int g = (y * 255) / h;
            int b = 128 + (int)(127 * sin(x * 0.05) * cos(y * 0.05));

            /* Checkerboard in center */
            int cx = x - w/2, cy = y - h/2;
            if (abs(cx) < 100 && abs(cy) < 100) {
                int cell = ((cx + 100) / 20 + (cy + 100) / 20) & 1;
                if (cell) { r = 255; g = 255; b = 255; }
                else      { r = 0;   g = 0;   b = 0; }
            }

            /* Thin line patterns for edge detection test */
            if (y == h/2 || x == w/2 || x == y || x + y == w) {
                r = 255; g = 0; b = 0;
            }

            SPIX_AT(img, x, y) = SPIX_RGB((uint8_t)r, (uint8_t)g, (uint8_t)b);
        }

    /* White border */
    for (int x = 0; x < w; x++) {
        SPIX_AT(img, x, 0) = SPIX_WHITE;
        SPIX_AT(img, x, h-1) = SPIX_WHITE;
    }
    for (int y = 0; y < h; y++) {
        SPIX_AT(img, 0, y) = SPIX_WHITE;
        SPIX_AT(img, w-1, y) = SPIX_WHITE;
    }

    spix_write_ppm(img, path);
    printf("Generated: %s (%dx%d)\n", path, w, h);
    spix_free(img);
}

/* ── Apply filter and save ───────────────────────────────── */

static void save_filter(const spix_image_t *src, const char *name,
                        spix_image_t *(*filter)(const spix_image_t *)) {
    spix_image_t *result = filter(src);
    if (!result) { printf("  FAIL: %s\n", name); return; }
    char fname[256];
    snprintf(fname, sizeof(fname), "%s.ppm", name);
    spix_write_ppm(result, fname);
    printf("  -> %s\n", fname);
    spix_free(result);
}

static void save_filter_inplace(const spix_image_t *src, const char *name,
                                int (*filter)(spix_image_t *)) {
    spix_image_t *copy = spix_clone(src);
    if (!copy) { printf("  FAIL: %s\n", name); return; }
    filter(copy);
    char fname[256];
    snprintf(fname, sizeof(fname), "%s.ppm", name);
    spix_write_ppm(copy, fname);
    printf("  -> %s\n", fname);
    spix_free(copy);
}

static void save_convolve(const spix_image_t *src, const char *name,
                          const spix_kernel_t *k) {
    spix_image_t *result = spix_convolve(src, k);
    if (!result) { printf("  FAIL: %s\n", name); return; }
    char fname[256];
    snprintf(fname, sizeof(fname), "%s.ppm", name);
    spix_write_ppm(result, fname);
    printf("  -> %s\n", fname);
    spix_free(result);
}

/* ── Main ────────────────────────────────────────────────── */

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "generate") == 0) {
        gen_test_pattern("input.ppm");
        return 0;
    }

    printf("spix demo — Image Processing Library\n");
    printf("====================================\n\n");

    /* Load input */
    spix_image_t *src = spix_read_ppm("input.ppm");
    if (!src) {
        fprintf(stderr, "No input.ppm found. Run: %s generate\n", argv[0]);
        return 1;
    }
    printf("Loaded: input.ppm (%dx%d)\n\n", src->width, src->height);

    /* ── Resize ────────────────────────────────────────── */
    printf("1. Resize\n");
    {
        spix_image_t *nn = spix_resize_nn(src, src->width / 2, src->height / 2);
        if (nn) { spix_write_ppm(nn, "resize_nn.ppm"); printf("  -> resize_nn.ppm (half)\n"); spix_free(nn); }

        spix_image_t *bl = spix_resize_bilinear(src, src->width / 2, src->height / 2);
        if (bl) { spix_write_ppm(bl, "resize_bilinear.ppm"); printf("  -> resize_bilinear.ppm (half)\n"); spix_free(bl); }
    }

    /* ── Convolution filters ───────────────────────────── */
    printf("2. Convolution filters\n");
    save_convolve(src, "blur_box",    &SPIX_KERNEL_BOX_BLUR_3);
    save_convolve(src, "blur_gauss",  &SPIX_KERNEL_GAUSS_BLUR_3);
    save_convolve(src, "sharpen",     &SPIX_KERNEL_SHARPEN);
    save_convolve(src, "edge",        &SPIX_KERNEL_EDGE_DETECT);
    save_convolve(src, "emboss",      &SPIX_KERNEL_EMBOSS);
    save_convolve(src, "sobel_h",     &SPIX_KERNEL_SOBEL_H);
    save_convolve(src, "sobel_v",     &SPIX_KERNEL_SOBEL_V);

    /* ── Color operations ──────────────────────────────── */
    printf("3. Color operations\n");
    save_filter_inplace(src, "grayscale", spix_grayscale);
    save_filter_inplace(src, "threshold_128", spix_threshold);
    {
        spix_image_t *copy = spix_clone(src);
        if (copy) {
            uint8_t t = spix_threshold_otsu(copy);
            printf("  Otsu threshold: %d\n", t);
            spix_threshold(copy, t);
            spix_write_ppm(copy, "threshold_otsu.ppm");
            printf("  -> threshold_otsu.ppm\n");
            spix_free(copy);
        }
    }

    /* ── Dithering ─────────────────────────────────────── */
    printf("4. Dithering\n");
    save_filter_inplace(src, "dither_floyd", spix_dither_floyd);
    save_filter_inplace(src, "dither_bayer", spix_dither_bayer);

    /* ── Geometry ──────────────────────────────────────── */
    printf("5. Geometry\n");
    {
        spix_image_t *copy = spix_clone(src);
        if (copy) { spix_flip_h(copy); spix_write_ppm(copy, "flip_h.ppm"); printf("  -> flip_h.ppm\n"); spix_free(copy); }
    }
    {
        spix_image_t *copy = spix_clone(src);
        if (copy) { spix_flip_v(copy); spix_write_ppm(copy, "flip_v.ppm"); printf("  -> flip_v.ppm\n"); spix_free(copy); }
    }
    {
        spix_image_t *rot = spix_rotate_90(src);
        if (rot) { spix_write_ppm(rot, "rotate_90.ppm"); printf("  -> rotate_90.ppm\n"); spix_free(rot); }
    }

    printf("\nAll outputs written. View with any PPM viewer.\n");

    spix_free(src);
    return 0;
}
