/**
 * tests/test_spix.c — correctness tests for spix.h
 *
 * Zero external dependencies: a tiny assert harness plus an INDEPENDENT
 * golden-model resampler implemented *in this file* (integer/double
 * arithmetic, no spix code paths) used to score spix_resize_bilinear().
 *
 * Golden model convention — mirrors the mapping the library documents in
 * spix_resize_bilinear() itself:
 *     index_fp = i * ((src_dim - 1) << 16) / dst_dim      (corner-aligned, Q16.16)
 *     frac     = index_fp & 0xFFFF
 *     value    = trunc( bilinear(frac) ) clamped to 0..255
 * The reference reproduces the same rational value in double precision, so it
 * is bit-comparable with the fixed-point result (the accumulator is exact in
 * int64/double: |N| < 2^42).
 *
 * A second, *centred* (half-pixel) reference is included to quantify the
 * difference between the library's corner-aligned convention and the
 * conventional centred mapping. That gap is a convention choice, NOT a bug,
 * and is reported separately.
 *
 * Run:  make test
 */

#define SPIX_IMPLEMENTATION
#include "../spix.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

/* ══════════════════════════════════════════════════════════════════
 *  Minimal test harness
 * ══════════════════════════════════════════════════════════════════ */

static int g_checks = 0;
static int g_fails  = 0;
static const char *g_section = "?";

static void section(const char *name) {
    g_section = name;
    printf("\n-- %s\n", name);
}

static void failf(int line, const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    g_fails++;
    printf("  FAIL line %d [%s]: %s\n", line, g_section, buf);
}

#define CHECK(c) \
    do { g_checks++; if (!(c)) failf(__LINE__, "expected true: %s", #c); } while (0)

#define CHECK_MSG(c, ...) \
    do { g_checks++; if (!(c)) failf(__LINE__, __VA_ARGS__); } while (0)

#define CHECK_EQ(got, want) \
    do { \
        long long g_ = (long long)(got), w_ = (long long)(want); \
        g_checks++; \
        if (g_ != w_) failf(__LINE__, "%s: got %lld, want %lld", #got, g_, w_); \
    } while (0)

/* ══════════════════════════════════════════════════════════════════
 *  Golden model: independent resamplers (test-only, not the library)
 * ══════════════════════════════════════════════════════════════════ */

/** Raw interleaved RGB bitmap used by the golden model. */
typedef struct {
    const char *name;
    int      w, h;
    uint8_t *rgb;      /* w * h * 3 bytes, R,G,B per pixel */
} img_t;

static uint8_t ref_clamp_u8(long long v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (uint8_t)v;
}

/** Corner-aligned bilinear resampler (independent implementation). */
static void ref_bilinear_corner(const uint8_t *src, int sw, int sh,
                                uint8_t *dst, int dw, int dh) {
    for (int y = 0; y < dh; y++) {
        long long y_fp = ((long long)y * ((long long)(sh - 1) << 16)) / dh;
        int sy  = (int)(y_fp >> 16);
        int sy1 = (sy + 1 < sh) ? sy + 1 : sy;
        int fy  = (int)(y_fp & 0xFFFF);
        long long ify = 65536 - fy;
        for (int x = 0; x < dw; x++) {
            long long x_fp = ((long long)x * ((long long)(sw - 1) << 16)) / dw;
            int sx  = (int)(x_fp >> 16);
            int sx1 = (sx + 1 < sw) ? sx + 1 : sx;
            int fx  = (int)(x_fp & 0xFFFF);
            long long ifx = 65536 - fx;
            for (int c = 0; c < 3; c++) {
                long long c00 = src[((size_t)sy  * sw + sx ) * 3 + c];
                long long c01 = src[((size_t)sy  * sw + sx1) * 3 + c];
                long long c10 = src[((size_t)sy1 * sw + sx ) * 3 + c];
                long long c11 = src[((size_t)sy1 * sw + sx1) * 3 + c];
                /* exact rational value: N / 2^32, truncated like an int64 >> 32 */
                long long n = (c00 * ifx + c01 * fx) * ify +
                              (c10 * ifx + c11 * fx) * fy;
                double v = (double)n / 4294967296.0;
                long long q = (long long)floor(v);
                dst[((size_t)y * dw + x) * 3 + c] = ref_clamp_u8(q);
            }
        }
    }
}

/** Centred (half-pixel) bilinear resampler — the conventional mapping. */
static void ref_bilinear_centred(const uint8_t *src, int sw, int sh,
                                 uint8_t *dst, int dw, int dh) {
    for (int y = 0; y < dh; y++) {
        double fy_src = ((double)y + 0.5) * (double)sh / (double)dh - 0.5;
        if (fy_src < 0) fy_src = 0;
        if (fy_src > sh - 1) fy_src = (double)(sh - 1);
        int sy  = (int)floor(fy_src);
        int sy1 = (sy + 1 < sh) ? sy + 1 : sy;
        double fy = fy_src - sy;
        for (int x = 0; x < dw; x++) {
            double fx_src = ((double)x + 0.5) * (double)sw / (double)dw - 0.5;
            if (fx_src < 0) fx_src = 0;
            if (fx_src > sw - 1) fx_src = (double)(sw - 1);
            int sx  = (int)floor(fx_src);
            int sx1 = (sx + 1 < sw) ? sx + 1 : sx;
            double fx = fx_src - sx;
            for (int c = 0; c < 3; c++) {
                double c00 = src[((size_t)sy  * sw + sx ) * 3 + c];
                double c01 = src[((size_t)sy  * sw + sx1) * 3 + c];
                double c10 = src[((size_t)sy1 * sw + sx ) * 3 + c];
                double c11 = src[((size_t)sy1 * sw + sx1) * 3 + c];
                double v = (c00 * (1 - fx) + c01 * fx) * (1 - fy) +
                           (c10 * (1 - fx) + c11 * fx) * fy;
                dst[((size_t)y * dw + x) * 3 + c] = ref_clamp_u8((long long)floor(v + 0.5));
            }
        }
    }
}

/** Nearest-neighbour resampler (independent implementation). */
static void ref_nearest(const uint8_t *src, int sw, int sh,
                        uint8_t *dst, int dw, int dh) {
    for (int y = 0; y < dh; y++) {
        int sy = y * sh / dh;
        if (sy < 0) sy = 0;
        if (sy >= sh) sy = sh - 1;
        for (int x = 0; x < dw; x++) {
            int sx = x * sw / dw;
            if (sx < 0) sx = 0;
            if (sx >= sw) sx = sw - 1;
            for (int c = 0; c < 3; c++)
                dst[((size_t)y * dw + x) * 3 + c] = src[((size_t)sy * sw + sx) * 3 + c];
        }
    }
}

/* ── Buffers ─────────────────────────────────────────────────── */

static img_t img_make(const char *name, int w, int h) {
    img_t t;
    t.name = name;
    t.w = w;
    t.h = h;
    t.rgb = (uint8_t *)malloc((size_t)w * (size_t)h * 3);
    if (!t.rgb) { fprintf(stderr, "OOM in test\n"); exit(2); }
    memset(t.rgb, 0, (size_t)w * (size_t)h * 3);
    return t;
}

static void img_release(img_t *t) {
    free(t->rgb);
    t->rgb = NULL;
}

static spix_image_t *img_to_spix(const img_t *t) {
    spix_image_t *im = spix_alloc(t->w, t->h);
    if (!im) return NULL;
    for (int y = 0; y < t->h; y++)
        for (int x = 0; x < t->w; x++) {
            const uint8_t *p = &t->rgb[((size_t)y * t->w + x) * 3];
            SPIX_AT(im, x, y) = SPIX_RGB(p[0], p[1], p[2]);
        }
    return im;
}

static void spix_to_plane(const spix_image_t *im, uint8_t *rgb) {
    for (int y = 0; y < im->height; y++)
        for (int x = 0; x < im->width; x++) {
            spix_pixel_t p = SPIX_AT(im, x, y);
            uint8_t *o = &rgb[((size_t)y * im->width + x) * 3];
            o[0] = SPIX_R(p); o[1] = SPIX_G(p); o[2] = SPIX_B(p);
        }
}

/* ── Synthetic test images (deterministic, no files) ─────────── */

static img_t gen_gradient(int w, int h) {
    img_t t = img_make("gradient", w, h);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            uint8_t *p = &t.rgb[((size_t)y * w + x) * 3];
            p[0] = (uint8_t)(w > 1 ? x * 255 / (w - 1) : 0);
            p[1] = (uint8_t)(h > 1 ? y * 255 / (h - 1) : 0);
            p[2] = (uint8_t)((x + y) * 255 / (w + h - 2 > 0 ? w + h - 2 : 1));
        }
    return t;
}

static img_t gen_checker(int w, int h) {
    img_t t = img_make("checker", w, h);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            uint8_t *p = &t.rgb[((size_t)y * w + x) * 3];
            int cell = ((x / 8) + (y / 8)) & 1;
            uint8_t v = cell ? 255 : 0;
            if (y >= h / 2 && y < h / 2 + 4) v = 128;   /* grey band */
            p[0] = v; p[1] = (uint8_t)(cell ? 255 : 0); p[2] = (uint8_t)(cell ? 0 : 255);
        }
    return t;
}

static img_t gen_noise(int w, int h, uint32_t seed) {
    img_t t = img_make("noise", w, h);
    uint32_t s = seed;
    for (size_t i = 0; i < (size_t)w * (size_t)h * 3; i++) {
        s = s * 1664525u + 1013904223u;
        t.rgb[i] = (uint8_t)(s >> 24);
    }
    return t;
}

static img_t gen_lines(int w, int h) {
    img_t t = img_make("lines", w, h);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            uint8_t *p = &t.rgb[((size_t)y * w + x) * 3];
            uint8_t v = 0;
            if (x == 0 || x == w - 1 || y == 0 || y == h - 1) v = 255;
            if (x == w / 2 || y == h / 2) v = 255;
            if (x == y) v = 200;
            p[0] = v; p[1] = v; p[2] = v;
        }
    /* two isolated bright pixels: worst case for any resampler */
    {
        int px = w / 3, py = h / 3;
        t.rgb[((size_t)py * w + px) * 3 + 0] = 255;
        t.rgb[((size_t)py * w + px) * 3 + 1] = 90;
        t.rgb[((size_t)py * w + px) * 3 + 2] = 0;
    }
    return t;
}

/* ── Scoring ─────────────────────────────────────────────────── */

typedef struct {
    double psnr;
    int    maxdev;
    long   nbad;     /* samples whose deviation is > 0 */
    long   nsamp;
} score_t;

static score_t score_against(const uint8_t *got, const uint8_t *ref,
                             size_t npix) {
    score_t s;
    double se = 0.0;
    s.maxdev = 0;
    s.nbad = 0;
    s.nsamp = (long)(npix * 3);
    for (size_t i = 0; i < npix * 3; i++) {
        int d = (int)got[i] - (int)ref[i];
        if (d < 0) d = -d;
        if (d > s.maxdev) s.maxdev = d;
        if (d > 1) s.nbad++;
        se += (double)d * (double)d;
    }
    {
        double mse = se / (double)s.nsamp;
        s.psnr = (mse == 0.0) ? INFINITY : 10.0 * log10(255.0 * 255.0 / mse);
    }
    return s;
}

static void print_score_cell(const score_t *s) {
    if (isinf(s->psnr)) printf("  exact   ");
    else                printf("%8.2f", s->psnr);
}

/* ══════════════════════════════════════════════════════════════════
 *  Small helpers for building fixtures
 * ══════════════════════════════════════════════════════════════════ */

static spix_image_t *solid(int w, int h, int r, int g, int b) {
    spix_image_t *im = spix_alloc(w, h);
    if (!im) return NULL;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            SPIX_AT(im, x, y) = SPIX_RGB((uint8_t)r, (uint8_t)g, (uint8_t)b);
    return im;
}

static void set_rgb(spix_image_t *im, int x, int y, int r, int g, int b) {
    SPIX_AT(im, x, y) = SPIX_RGB((uint8_t)r, (uint8_t)g, (uint8_t)b);
}

static int count_nonzero(const spix_image_t *im) {
    int n = 0;
    for (int y = 0; y < im->height; y++)
        for (int x = 0; x < im->width; x++)
            if (SPIX_R(SPIX_AT(im, x, y))) n++;
    return n;
}

static long file_size_bytes(const char *path) {
    FILE *fp = fopen(path, "rb");
    long n;
    if (!fp) return -1;
    fseek(fp, 0, SEEK_END);
    n = ftell(fp);
    fclose(fp);
    return n;
}

static uint32_t rd32le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int rd16le(const uint8_t *p) {
    return (int)p[0] | ((int)p[1] << 8);
}

/* ══════════════════════════════════════════════════════════════════
 *  Tests
 * ══════════════════════════════════════════════════════════════════ */

static void test_lifecycle(void) {
    section("lifecycle: alloc / wrap / clone / free");
    spix_image_t *im = spix_alloc(4, 3);
    CHECK(im != NULL);
    if (im) {
        CHECK_EQ(im->width, 4);
        CHECK_EQ(im->height, 3);
        CHECK_EQ(im->stride, 4);
        CHECK_EQ(im->owned, 1);
        int allzero = 1;
        for (int i = 0; i < 12; i++) if (im->pixels[i] != 0) allzero = 0;
        CHECK_MSG(allzero, "spix_alloc must zero pixels");
        SPIX_AT(im, 3, 2) = SPIX_RGB(1, 2, 3);
        spix_image_t *cp = spix_clone(im);
        CHECK(cp != NULL);
        if (cp) {
            CHECK_EQ(SPIX_R(SPIX_AT(cp, 3, 2)), 1);
            CHECK_EQ(SPIX_B(SPIX_AT(cp, 3, 2)), 3);
            SPIX_AT(cp, 0, 0) = SPIX_RGB(9, 9, 9);
            CHECK_EQ(SPIX_R(SPIX_AT(im, 0, 0)), 0);   /* deep copy */
            spix_free(cp);
        }
        spix_free(im);
    }
    CHECK(spix_alloc(0, 4) == NULL);
    CHECK(spix_alloc(4, 0) == NULL);
    CHECK(spix_alloc(-1, 4) == NULL);
    CHECK(spix_clone(NULL) == NULL);
    spix_free(NULL);   /* must not crash */

    /* non-owning wrap with a padded stride */
    spix_pixel_t *buf = (spix_pixel_t *)calloc(8 * 2, sizeof(spix_pixel_t));
    spix_image_t wrapped = spix_wrap(3, 2, buf, 8);
    CHECK_EQ(wrapped.width, 3);
    CHECK_EQ(wrapped.stride, 8);
    CHECK_EQ(wrapped.owned, 0);
    SPIX_AT(&wrapped, 2, 1) = SPIX_RGB(7, 8, 9);
    CHECK_EQ(SPIX_R(buf[1 * 8 + 2]), 7);
    CHECK_EQ(SPIX_B(buf[1 * 8 + 2]), 9);
    CHECK_EQ(SPIX_R(buf[1 * 8 + 3]), 0);   /* padding untouched */
    spix_image_t *wclone = spix_clone(&wrapped);
    CHECK(wclone != NULL);
    if (wclone) {
        CHECK_EQ(wclone->stride, 3);
        CHECK_EQ(SPIX_R(SPIX_AT(wclone, 2, 1)), 7);
        spix_free(wclone);
    }
    free(buf);
}

static void test_ppm_io(void) {
    section("IO: PPM/PGM read/write");
    /* P6 write → byte-level check */
    spix_image_t *im = spix_alloc(3, 2);
    set_rgb(im, 0, 0, 1, 2, 3);   set_rgb(im, 1, 0, 4, 5, 6);   set_rgb(im, 2, 0, 7, 8, 9);
    set_rgb(im, 0, 1, 10, 11, 12); set_rgb(im, 1, 1, 13, 14, 15); set_rgb(im, 2, 1, 16, 17, 18);
    const char *path = "spix_test_io.ppm";
    CHECK_EQ(spix_write_ppm(im, path), SPIX_OK);
    {
        uint8_t buf[64];
        FILE *fp = fopen(path, "rb");
        CHECK(fp != NULL);
        if (fp) {
            size_t n = fread(buf, 1, sizeof buf, fp);
            fclose(fp);
            CHECK_EQ(n, (size_t)(11 + 18));
            CHECK_EQ(memcmp(buf, "P6\n3 2\n255\n", 11), 0);
            CHECK_EQ(buf[11], 1);  CHECK_EQ(buf[12], 2);  CHECK_EQ(buf[13], 3);
            CHECK_EQ(buf[26], 16); CHECK_EQ(buf[27], 17); CHECK_EQ(buf[28], 18);
        }
    }
    /* P6 read back → identical RGB */
    spix_image_t *back = spix_read_ppm(path);
    CHECK(back != NULL);
    if (back) {
        CHECK_EQ(back->width, 3);
        CHECK_EQ(back->height, 2);
        int same = 1;
        for (int y = 0; y < 2; y++)
            for (int x = 0; x < 3; x++) {
                int v = y * 3 + x;
                spix_pixel_t a = SPIX_AT(im, x, y), b = SPIX_AT(back, x, y);
                if (SPIX_R(a) != 1 + v * 3 || SPIX_R(b) != SPIX_R(a) ||
                    SPIX_G(b) != SPIX_G(a) || SPIX_B(b) != SPIX_B(a)) same = 0;
            }
        CHECK_MSG(same, "P6 round-trip must preserve RGB");
        spix_free(back);
    }
    spix_free(im);
    remove(path);

    /* ASCII P3 with a comment line and maxval 100 */
    {
        const char *p3 = "spix_test_ascii.ppm";
        FILE *fp = fopen(p3, "wb");
        CHECK(fp != NULL);
        if (fp) {
            fputs("P3\n# a comment\n2 1\n100\n100 50 0\n0 25 100\n", fp);
            fclose(fp);
        }
        spix_image_t *a = spix_read_ppm(p3);
        CHECK(a != NULL);
        if (a) {
            CHECK_EQ(a->width, 2);
            CHECK_EQ(a->height, 1);
            CHECK_EQ(SPIX_R(SPIX_AT(a, 0, 0)), 255);  /* 100 * 255 / 100 */
            CHECK_EQ(SPIX_G(SPIX_AT(a, 0, 0)), 127);  /* 50  * 255 / 100 */
            CHECK_EQ(SPIX_B(SPIX_AT(a, 0, 0)), 0);
            CHECK_EQ(SPIX_G(SPIX_AT(a, 1, 0)), 63);   /* 25  * 255 / 100 */
            spix_free(a);
        }
        remove(p3);
    }
    /* binary P5 grey with maxval 15 */
    {
        const char *p5 = "spix_test_p5.pgm";
        FILE *fp = fopen(p5, "wb");
        CHECK(fp != NULL);
        if (fp) {
            uint8_t data[2] = {15, 1};
            fputs("P5\n2 1\n15\n", fp);
            fwrite(data, 1, 2, fp);
            fclose(fp);
        }
        spix_image_t *g = spix_read_ppm(p5);
        CHECK(g != NULL);
        if (g) {
            CHECK_EQ(SPIX_R(SPIX_AT(g, 0, 0)), 255);
            CHECK_EQ(SPIX_G(SPIX_AT(g, 0, 0)), 255);
            CHECK_EQ(SPIX_R(SPIX_AT(g, 1, 0)), 17);   /* 1 * 255 / 15 */
            spix_free(g);
        }
        remove(p5);
    }
    CHECK(spix_read_ppm("spix_test_does_not_exist.ppm") == NULL);

    /* first pixel bytes may themselves be whitespace values: the reader must
       consume exactly one delimiter, never a pixel byte (R=10 '\n', G=32 ' ', B=9 '\t') */
    {
        const char *pw = "spix_test_ws.ppm";
        spix_image_t *w = spix_alloc(1, 1);
        set_rgb(w, 0, 0, 10, 32, 9);
        CHECK_EQ(spix_write_ppm(w, pw), SPIX_OK);
        CHECK_EQ(file_size_bytes(pw), 11 + 3);
        spix_image_t *r = spix_read_ppm(pw);
        CHECK(r != NULL);
        if (r) {
            CHECK_EQ(SPIX_R(SPIX_AT(r, 0, 0)), 10);
            CHECK_EQ(SPIX_G(SPIX_AT(r, 0, 0)), 32);
            CHECK_EQ(SPIX_B(SPIX_AT(r, 0, 0)), 9);
            spix_free(r);
        }
        spix_free(w);
        remove(pw);
    }
    /* CRLF header must not shift the pixel data either */
    {
        const char *pc = "spix_test_crlf.ppm";
        FILE *fp = fopen(pc, "wb");
        CHECK(fp != NULL);
        if (fp) {
            fputs("P6\r\n2 1\r\n255\r\n", fp);
            fputc(7, fp);  fputc(8, fp);  fputc(9, fp);
            fputc(11, fp); fputc(12, fp); fputc(13, fp);
            fclose(fp);
        }
        spix_image_t *r = spix_read_ppm(pc);
        CHECK(r != NULL);
        if (r) {
            CHECK_EQ(SPIX_R(SPIX_AT(r, 0, 0)), 7);
            CHECK_EQ(SPIX_B(SPIX_AT(r, 0, 0)), 9);
            CHECK_EQ(SPIX_R(SPIX_AT(r, 1, 0)), 11);
            CHECK_EQ(SPIX_B(SPIX_AT(r, 1, 0)), 13);
            spix_free(r);
        }
        remove(pc);
    }

    /* memory round-trip */
    {
        spix_image_t *m = solid(4, 4, 11, 22, 33);
        uint8_t *buf = NULL;
        size_t n = spix_write_ppm_mem(m, &buf);
        CHECK(n > 0);
        CHECK_EQ(n, (size_t)(11 + 48));
        CHECK_EQ(buf[n], 0);                       /* NUL-terminated */
        spix_image_t *r = spix_read_ppm_mem(buf, n);
        CHECK(r != NULL);
        if (r) {
            CHECK_EQ(r->width, 4);
            CHECK_EQ(SPIX_R(SPIX_AT(r, 3, 3)), 11);
            CHECK_EQ(SPIX_G(SPIX_AT(r, 3, 3)), 22);
            CHECK_EQ(SPIX_B(SPIX_AT(r, 3, 3)), 33);
            spix_free(r);
        }
        free(buf);
        spix_free(m);
    }
    /* in-memory P6 with maxval != 255 must be scaled like the file reader */
    {
        const char *hdr = "P6\n2 1\n15\n";
        uint8_t buf[64];
        size_t hn = strlen(hdr);
        uint8_t data[6] = {15, 0, 15, 0, 15, 0};
        memcpy(buf, hdr, hn);
        memcpy(buf + hn, data, 6);
        spix_image_t *r = spix_read_ppm_mem(buf, hn + 6);
        CHECK(r != NULL);
        if (r) {
            CHECK_EQ(SPIX_R(SPIX_AT(r, 0, 0)), 255);
            CHECK_EQ(SPIX_G(SPIX_AT(r, 0, 0)), 0);
            CHECK_EQ(SPIX_R(SPIX_AT(r, 1, 0)), 0);
            CHECK_EQ(SPIX_G(SPIX_AT(r, 1, 0)), 255);
            spix_free(r);
        }
    }
}

static void test_bmp_io(void) {
    section("IO: BMP writer (24-bit, bottom-up, padded rows)");
    spix_image_t *im = spix_alloc(3, 2);   /* 3 px rows → 3 padding bytes */
    set_rgb(im, 0, 0, 255, 0, 0);   set_rgb(im, 1, 0, 0, 255, 0);   set_rgb(im, 2, 0, 0, 0, 255);
    set_rgb(im, 0, 1, 1, 2, 3);     set_rgb(im, 1, 1, 4, 5, 6);     set_rgb(im, 2, 1, 7, 8, 9);

    const char *path = "spix_test_out.bmp";
    uint8_t *mem = NULL;
    size_t mem_n = spix_write_bmp_mem(im, &mem);
    CHECK(mem_n > 0);
    CHECK_EQ(spix_write_bmp(im, path), SPIX_OK);
    CHECK_EQ(file_size_bytes(path), (long)mem_n);

    {
        uint8_t *fb = (uint8_t *)malloc(mem_n);
        FILE *fp = fopen(path, "rb");
        CHECK(fp != NULL);
        if (fp && fb) {
            size_t n = fread(fb, 1, mem_n, fp);
            CHECK_EQ(n, mem_n);
            CHECK_MSG(memcmp(fb, mem, mem_n) == 0,
                      "spix_write_bmp() and spix_write_bmp_mem() must agree byte for byte");
        }
        if (fp) fclose(fp);

        CHECK_EQ(fb[0], 'B');
        CHECK_EQ(fb[1], 'M');
        CHECK_EQ(rd32le(fb + 2), (uint32_t)mem_n);
        CHECK_EQ(rd32le(fb + 10), 54);
        CHECK_EQ(rd32le(fb + 14), 40);
        CHECK_EQ(rd32le(fb + 18), 3);
        CHECK_EQ(rd32le(fb + 22), 2);
        CHECK_EQ(rd16le(fb + 26), 1);
        CHECK_EQ(rd16le(fb + 28), 24);
        CHECK_EQ(rd32le(fb + 34), 12 * 2);
        /* row 0 in the file is the BOTTOM row of the image, BGR order */
        CHECK_EQ(fb[54 + 0], 3);  /* B of (0,1) */
        CHECK_EQ(fb[54 + 1], 2);  /* G */
        CHECK_EQ(fb[54 + 2], 1);  /* R */
        CHECK_EQ(fb[54 + 9], 0);  /* padding byte */
        CHECK_EQ(fb[54 + 12 + 0], 0);    /* B of (0,0) = pure red */
        CHECK_EQ(fb[54 + 12 + 1], 0);    /* G */
        CHECK_EQ(fb[54 + 12 + 2], 255);  /* R */
        CHECK_EQ(fb[54 + 12 + 4], 255);  /* G of (1,0) = pure green */
        free(fb);
    }
    free(mem);
    spix_free(im);
    remove(path);
}

static void test_resize_nearest(void) {
    section("resize: nearest-neighbour is an exact integer remap");
    img_t src = gen_gradient(37, 23);
    spix_image_t *im = img_to_spix(&src);
    const int targets[][2] = {{74, 46}, {18, 11}, {55, 34}, {1, 1}, {37, 1}, {1, 23}, {100, 7}};
    for (size_t i = 0; i < sizeof targets / sizeof targets[0]; i++) {
        int dw = targets[i][0], dh = targets[i][1];
        spix_image_t *out = spix_resize_nn(im, dw, dh);
        CHECK(out != NULL);
        if (!out) continue;
        uint8_t *got = (uint8_t *)malloc((size_t)dw * dh * 3);
        uint8_t *ref = (uint8_t *)malloc((size_t)dw * dh * 3);
        spix_to_plane(out, got);
        ref_nearest(src.rgb, src.w, src.h, ref, dw, dh);
        score_t s = score_against(got, ref, (size_t)dw * dh);
        CHECK_MSG(s.maxdev == 0, "NN %dx%d->%dx%d: max|dev|=%d (must be 0)",
                  src.w, src.h, dw, dh, s.maxdev);
        free(got); free(ref);
        spix_free(out);
    }
    CHECK(spix_resize_nn(NULL, 4, 4) == NULL);
    CHECK(spix_resize_nn(im, 0, 4) == NULL);
    CHECK(spix_resize_nn(im, 4, -1) == NULL);
    spix_free(im);
    img_release(&src);
}

static void test_resize_bilinear_quality(void) {
    section("resize: bilinear vs golden reference (per image / per scale)");

    img_t imgs[4];
    imgs[0] = gen_gradient(320, 240);
    imgs[1] = gen_checker(256, 256);
    imgs[2] = gen_noise(200, 150, 0x2F6E2B1u);
    imgs[3] = gen_lines(257, 129);

    printf("  %-9s %-10s %-10s | %-9s %-7s %-11s | %s\n",
           "image", "source", "target", "PSNR", "max|d|", "samples>1LSB",
           "PSNR vs centred ref");
    printf("  %-9s %-10s %-10s | %-9s %-7s %-11s | %s\n",
           "---------", "----------", "----------", "---------", "-------",
           "------------", "-------------------------");

    int rows = 0;
    for (int k = 0; k < 4; k++) {
        spix_image_t *im = img_to_spix(&imgs[k]);
        int W = imgs[k].w, H = imgs[k].h;
        const int targets[][2] = {
            {W * 2, H * 2},          /* 2x up   */
            {W / 2, H / 2},          /* 2x down */
            {W * 3 / 2, H * 3 / 2},  /* 1.5x up (odd) */
            {1, 1}, {W, 1}, {1, H},  /* degenerate */
            {53, 31}                 /* odd, non-proportional */
        };
        for (size_t i = 0; i < sizeof targets / sizeof targets[0]; i++) {
            int dw = targets[i][0], dh = targets[i][1];
            if (dw <= 0 || dh <= 0) continue;
            spix_image_t *out = spix_resize_bilinear(im, dw, dh);
            CHECK(out != NULL);
            if (!out) continue;
            CHECK_EQ(out->width, dw);
            CHECK_EQ(out->height, dh);

            uint8_t *got = (uint8_t *)malloc((size_t)dw * dh * 3);
            uint8_t *ref = (uint8_t *)malloc((size_t)dw * dh * 3);
            uint8_t *cen = (uint8_t *)malloc((size_t)dw * dh * 3);
            spix_to_plane(out, got);
            ref_bilinear_corner(imgs[k].rgb, W, H, ref, dw, dh);
            ref_bilinear_centred(imgs[k].rgb, W, H, cen, dw, dh);
            score_t s = score_against(got, ref, (size_t)dw * dh);
            score_t sc = score_against(got, cen, (size_t)dw * dh);

            char srcbuf[32], dstbuf[32];
            snprintf(srcbuf, sizeof srcbuf, "%dx%d", W, H);
            snprintf(dstbuf, sizeof dstbuf, "%dx%d", dw, dh);
            printf("  %-9s %-10s %-10s |", imgs[k].name, srcbuf, dstbuf);
            print_score_cell(&s);
            printf(" %6d %6ld/%-6ld |", s.maxdev, s.nbad, s.nsamp);
            print_score_cell(&sc);
            printf("\n");
            rows++;

            CHECK_MSG(s.maxdev <= 1,
                      "bilinear %s %dx%d->%dx%d: max|dev|=%d vs golden model",
                      imgs[k].name, W, H, dw, dh, s.maxdev);
            CHECK_MSG(s.psnr >= 45.0 || isinf(s.psnr),
                      "bilinear %s %dx%d->%dx%d: PSNR %.2f dB < 45 dB",
                      imgs[k].name, W, H, dw, dh, s.psnr);

            free(got); free(ref); free(cen);
            spix_free(out);
        }
        spix_free(im);
    }
    printf("  (%d comparisons, tolerance: max|dev| <= 1 LSB, PSNR >= 45 dB)\n", rows);
    for (int k = 0; k < 4; k++) img_release(&imgs[k]);
}

static void test_resize_bilinear_pins(void) {
    section("resize: bilinear pins the documented mapping");
    /* 2x1 source [0,255] upscaled to 4x1 with the corner-aligned mapping
       x_fp = x * 1 << 16 / 4  →  fracs 0, .25, .5, .75  →  0, 63, 127, 191 */
    img_t src = img_make("pin", 2, 1);
    for (int c = 0; c < 3; c++) {
        src.rgb[0 * 3 + c] = 0;
        src.rgb[1 * 3 + c] = 255;
    }
    spix_image_t *im = img_to_spix(&src);
    spix_image_t *out = spix_resize_bilinear(im, 4, 1);
    CHECK(out != NULL);
    if (out) {
        const int expect[4] = {0, 63, 127, 191};
        for (int x = 0; x < 4; x++)
            CHECK_EQ(SPIX_R(SPIX_AT(out, x, 0)), expect[x]);
        spix_free(out);
    }
    /* 1:1 resize goes through the same mapping, so it is NOT the identity:
       the dest coordinate x maps to x*(w-1)/w, which at the far edge is a full
       pixel short. The result still matches the golden model exactly; the
       deviation from the source is printed as informational evidence and
       documented in the README as a limitation. */
    img_t g = gen_noise(64, 48, 7u);
    spix_image_t *gi = img_to_spix(&g);
    spix_image_t *id = spix_resize_bilinear(gi, 64, 48);
    CHECK(id != NULL);
    if (id) {
        size_t np = 64 * 48;
        uint8_t *got = (uint8_t *)malloc(np * 3);
        uint8_t *ref = (uint8_t *)malloc(np * 3);
        spix_to_plane(id, got);
        ref_bilinear_corner(g.rgb, 64, 48, ref, 64, 48);
        score_t s = score_against(got, ref, np);         /* 0 expected */
        score_t vsrc = score_against(got, g.rgb, np);    /* informational */
        printf("  1:1 resize vs golden model: max|dev| = %d; vs source: max|dev| = %d LSB (PSNR %.2f dB)\n",
               s.maxdev, vsrc.maxdev, vsrc.psnr);
        CHECK_MSG(s.maxdev == 0, "1:1 resize differs from the golden model by %d LSB", s.maxdev);
        CHECK(id->width == 64 && id->height == 48);
        free(got);
        free(ref);
        spix_free(id);
    }
    CHECK(spix_resize_bilinear(NULL, 4, 4) == NULL);
    CHECK(spix_resize_bilinear(gi, 0, 4) == NULL);
    CHECK(spix_resize_bilinear(gi, 4, 0) == NULL);
    spix_free(gi);
    spix_free(im);
    img_release(&src);
    img_release(&g);
}

static void test_convolution(void) {
    section("convolution: kernels, bias and edges");

    /* flat 100 grey: blur/sharpen/emboss keep it, zero-sum kernels get +128 */
    spix_image_t *flat = solid(9, 9, 100, 100, 100);
    struct { const spix_kernel_t *k; const char *name; int expect; } cases[] = {
        {&SPIX_KERNEL_GAUSS_BLUR_3, "gauss",   100},
        {&SPIX_KERNEL_BOX_BLUR_3,   "box",     112},  /* 9*100 >> 3 */
        {&SPIX_KERNEL_SHARPEN,      "sharpen", 100},
        {&SPIX_KERNEL_EMBOSS,       "emboss",  100},
        {&SPIX_KERNEL_EDGE_DETECT,  "edge",    128},  /* sum 0 + mid-grey bias */
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        spix_image_t *out = spix_convolve(flat, cases[i].k);
        CHECK(out != NULL);
        if (!out) continue;
        int ok = 1, first = -1;
        for (int y = 0; y < 9 && ok; y++)
            for (int x = 0; x < 9; x++)
                if (SPIX_R(SPIX_AT(out, x, y)) != cases[i].expect) {
                    ok = 0; first = SPIX_R(SPIX_AT(out, x, y)); break;
                }
        CHECK_MSG(ok, "flat grey through '%s': expected %d everywhere, got %d",
                  cases[i].name, cases[i].expect, first);
        spix_free(out);
    }

    /* vertical edge: black | white, sobel_h */
    spix_image_t *edge = spix_alloc(8, 8);
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++)
            set_rgb(edge, x, y, x < 4 ? 0 : 255, x < 4 ? 0 : 255, x < 4 ? 0 : 255);
    {
        spix_image_t *sh = spix_convolve(edge, &SPIX_KERNEL_SOBEL_H);
        CHECK(sh != NULL);
        if (sh) {
            /* flat black region → mid grey; the two columns straddling the
               edge carry the full gradient */
            CHECK_EQ(SPIX_R(SPIX_AT(sh, 0, 3)), 128);
            CHECK_EQ(SPIX_R(SPIX_AT(sh, 1, 3)), 128);
            CHECK_EQ(SPIX_R(SPIX_AT(sh, 3, 3)), 255);
            CHECK_EQ(SPIX_R(SPIX_AT(sh, 4, 3)), 255);
            CHECK_EQ(SPIX_R(SPIX_AT(sh, 5, 3)), 128);
            CHECK_EQ(SPIX_R(SPIX_AT(sh, 7, 3)), 128);
            spix_free(sh);
        }
        spix_image_t *sv = spix_convolve(edge, &SPIX_KERNEL_SOBEL_V);
        CHECK(sv != NULL);
        if (sv) {
            /* constant along Y → no vertical gradient anywhere */
            CHECK_EQ(SPIX_R(SPIX_AT(sv, 3, 3)), 128);
            CHECK_EQ(SPIX_R(SPIX_AT(sv, 5, 5)), 128);
            spix_free(sv);
        }
    }

    /* box blur really averages: a single bright pixel spreads to 9x9/9 */
    {
        spix_image_t *dot = spix_alloc(5, 5);
        set_rgb(dot, 2, 2, 255, 255, 255);
        spix_image_t *b = spix_convolve(dot, &SPIX_KERNEL_BOX_BLUR_3);
        CHECK(b != NULL);
        if (b) {
            CHECK_EQ(SPIX_R(SPIX_AT(b, 2, 2)), 31);   /* 255 >> 3 */
            CHECK_EQ(SPIX_R(SPIX_AT(b, 1, 1)), 31);
            CHECK_EQ(SPIX_R(SPIX_AT(b, 0, 0)), 0);
            spix_free(b);
        }
        spix_free(dot);
    }

    /* convolve_to: mismatched size is rejected, in-place is possible */
    {
        spix_image_t *src = solid(4, 4, 10, 20, 30);
        spix_image_t *dst = spix_alloc(5, 4);
        CHECK_EQ(spix_convolve_to(src, dst, &SPIX_KERNEL_GAUSS_BLUR_3), SPIX_ERR_SIZE);
        spix_free(dst);
        dst = spix_alloc(4, 4);
        CHECK_EQ(spix_convolve_to(src, dst, &SPIX_KERNEL_GAUSS_BLUR_3), SPIX_OK);
        CHECK_EQ(SPIX_R(SPIX_AT(dst, 0, 0)), 10);
        CHECK_EQ(spix_convolve_to(src, src, &SPIX_KERNEL_GAUSS_BLUR_3), SPIX_OK);
        CHECK_EQ(SPIX_B(SPIX_AT(src, 2, 2)), 30);
        spix_free(dst);
        spix_free(src);
    }
    CHECK(spix_convolve(NULL, &SPIX_KERNEL_SHARPEN) == NULL);
    CHECK(spix_convolve(flat, NULL) == NULL);
    CHECK_EQ(spix_convolve_to(NULL, flat, &SPIX_KERNEL_SHARPEN), SPIX_ERR_SIZE);
    spix_free(flat);
    spix_free(edge);
}

static void test_color(void) {
    section("color: grayscale / threshold / Otsu / histogram / equalize");
    spix_image_t *im = spix_alloc(4, 1);
    set_rgb(im, 0, 0, 255, 0, 0);
    set_rgb(im, 1, 0, 0, 255, 0);
    set_rgb(im, 2, 0, 0, 0, 255);
    set_rgb(im, 3, 0, 255, 255, 255);
    CHECK_EQ(spix_grayscale(im), SPIX_OK);
    CHECK_EQ(SPIX_R(SPIX_AT(im, 0, 0)), 76);    /* 255*299/1000 */
    CHECK_EQ(SPIX_R(SPIX_AT(im, 1, 0)), 149);   /* 255*587/1000 */
    CHECK_EQ(SPIX_R(SPIX_AT(im, 2, 0)), 29);    /* 255*114/1000 */
    CHECK_EQ(SPIX_R(SPIX_AT(im, 3, 0)), 255);
    CHECK_EQ(SPIX_A(SPIX_AT(im, 0, 0)), 255);   /* alpha preserved */
    spix_free(im);

    /* threshold converts to grey first, then binarizes */
    spix_image_t *t = spix_alloc(3, 1);
    set_rgb(t, 0, 0, 255, 0, 0);      /* grey 76  → black */
    set_rgb(t, 1, 0, 0, 255, 0);      /* grey 149 → white */
    set_rgb(t, 2, 0, 128, 128, 128);  /* grey 128 → white (>= threshold) */
    CHECK_EQ(spix_threshold(t, 128), SPIX_OK);
    CHECK_EQ(SPIX_R(SPIX_AT(t, 0, 0)), 0);
    CHECK_EQ(SPIX_R(SPIX_AT(t, 1, 0)), 255);
    CHECK_EQ(SPIX_R(SPIX_AT(t, 2, 0)), 255);
    CHECK_EQ(SPIX_AT(t, 2, 0), SPIX_WHITE);
    CHECK_EQ(SPIX_AT(t, 0, 0), SPIX_BLACK);
    spix_free(t);

    /* Otsu on a bimodal image */
    {
        spix_image_t *o = spix_alloc(10, 10);
        int i = 0;
        for (int y = 0; y < 10; y++)
            for (int x = 0; x < 10; x++, i++)
                set_rgb(o, x, y, i < 60 ? 50 : 200, i < 60 ? 50 : 200, i < 60 ? 50 : 200);
        uint8_t th = spix_threshold_otsu(o);
        CHECK_MSG(th > 50 && th <= 200, "Otsu on {50,200} returned %d", (int)th);
        CHECK_EQ(spix_threshold(o, th), SPIX_OK);
        CHECK_EQ(SPIX_R(SPIX_AT(o, 0, 0)), 0);        /* 50 < th → black */
        CHECK_EQ(SPIX_R(SPIX_AT(o, 9, 9)), 255);      /* 200 >= th → white */
        spix_free(o);
    }

    /* histogram sums to the pixel count */
    {
        spix_image_t *h = spix_alloc(8, 4);
        for (int y = 0; y < 4; y++)
            for (int x = 0; x < 8; x++)
                set_rgb(h, x, y, (x < 4 ? 10 : 200), (x < 4 ? 10 : 200), (x < 4 ? 10 : 200));
        uint32_t hist[256];
        CHECK_EQ(spix_histogram(h, hist), SPIX_OK);
        long sum = 0;
        for (int i = 0; i < 256; i++) sum += hist[i];
        CHECK_EQ(sum, 32);
        CHECK_EQ(hist[10], 16);
        CHECK_EQ(hist[200], 16);
        CHECK_EQ(hist[0], 0);
        /* equalize stretches the two levels to 0 and 255 */
        CHECK_EQ(spix_equalize(h), SPIX_OK);
        CHECK_EQ(SPIX_R(SPIX_AT(h, 0, 0)), 0);
        CHECK_EQ(SPIX_R(SPIX_AT(h, 4, 0)), 255);
        spix_free(h);
    }
    /* a uniform image is a fixed point of equalization */
    {
        spix_image_t *u = solid(6, 6, 77, 77, 77);
        CHECK_EQ(spix_equalize(u), SPIX_OK);
        CHECK_EQ(SPIX_R(SPIX_AT(u, 3, 3)), 77);
        spix_free(u);
    }
    CHECK_EQ(spix_grayscale(NULL), SPIX_ERR_SIZE);
    CHECK_EQ(spix_threshold(NULL, 128), SPIX_ERR_SIZE);
    CHECK_EQ(spix_threshold_otsu(NULL), 128);
    CHECK_EQ(spix_histogram(NULL, NULL), SPIX_ERR_SIZE);
    CHECK_EQ(spix_equalize(NULL), SPIX_ERR_SIZE);
}

static void test_dither(void) {
    section("dithering: Floyd-Steinberg and Bayer 4x4");
    img_t g = gen_gradient(64, 64);
    spix_image_t *im = img_to_spix(&g);
    long in_sum = 0;
    for (int y = 0; y < 64; y++)
        for (int x = 0; x < 64; x++)
            in_sum += SPIX_R(SPIX_AT(im, x, y));

    spix_image_t *f = spix_clone(im);
    CHECK_EQ(spix_dither_floyd(f), SPIX_OK);
    int only_bw = 1, black = 0, white = 0;
    long out_sum = 0;
    for (int y = 0; y < 64; y++)
        for (int x = 0; x < 64; x++) {
            spix_pixel_t p = SPIX_AT(f, x, y);
            if (p != SPIX_BLACK && p != SPIX_WHITE) only_bw = 0;
            if (p == SPIX_WHITE) white++; else black++;
            out_sum += SPIX_R(p);
        }
    CHECK_MSG(only_bw, "Floyd-Steinberg must emit only black/white");
    CHECK(black > 0 && white > 0);
    CHECK_MSG(labs((long)(out_sum / 4096 - in_sum / 4096)) <= 2,
              "Floyd-Steinberg lost the mean: in %ld, out %ld",
              in_sum / 4096, out_sum / 4096);
    spix_free(f);

    spix_image_t *b = spix_clone(im);
    CHECK_EQ(spix_dither_bayer(b), SPIX_OK);
    only_bw = 1; black = 0; white = 0;
    for (int y = 0; y < 64; y++)
        for (int x = 0; x < 64; x++) {
            spix_pixel_t p = SPIX_AT(b, x, y);
            if (p != SPIX_BLACK && p != SPIX_WHITE) only_bw = 0;
            if (p == SPIX_WHITE) white++; else black++;
        }
    CHECK_MSG(only_bw, "Bayer must emit only black/white");
    CHECK(black > 0 && white > 0);
    spix_free(b);

    /* Bayer on a flat 128 grey should dither to roughly half/half */
    {
        spix_image_t *mid = solid(8, 8, 128, 128, 128);
        CHECK_EQ(spix_dither_bayer(mid), SPIX_OK);
        int w = 0;
        for (int y = 0; y < 8; y++)
            for (int x = 0; x < 8; x++)
                if (SPIX_AT(mid, x, y) == SPIX_WHITE) w++;
        CHECK_MSG(w >= 24 && w <= 40, "Bayer(128) gave %d/64 white pixels", w);
        spix_free(mid);
    }
    CHECK_EQ(spix_dither_floyd(NULL), SPIX_ERR_SIZE);
    CHECK_EQ(spix_dither_bayer(NULL), SPIX_ERR_SIZE);
    spix_free(im);
    img_release(&g);
}

static void test_geometry(void) {
    section("geometry: flips, 90 degree rotation, channel access");
    spix_image_t *im = spix_alloc(3, 2);
    int n = 0;
    for (int y = 0; y < 2; y++)
        for (int x = 0; x < 3; x++, n++)
            set_rgb(im, x, y, n * 10, n * 10 + 1, n * 10 + 2);

    spix_image_t *h = spix_clone(im);
    CHECK_EQ(spix_flip_h(h), SPIX_OK);
    CHECK_EQ(SPIX_R(SPIX_AT(h, 0, 0)), 20);
    CHECK_EQ(SPIX_R(SPIX_AT(h, 2, 0)), 0);
    CHECK_EQ(SPIX_R(SPIX_AT(h, 0, 1)), 50);
    spix_free(h);

    spix_image_t *v = spix_clone(im);
    CHECK_EQ(spix_flip_v(v), SPIX_OK);
    CHECK_EQ(SPIX_R(SPIX_AT(v, 0, 0)), 30);
    CHECK_EQ(SPIX_R(SPIX_AT(v, 2, 1)), 20);
    spix_free(v);

    spix_image_t *r = spix_rotate_90(im);
    CHECK(r != NULL);
    if (r) {
        CHECK_EQ(r->width, 2);
        CHECK_EQ(r->height, 3);
        CHECK_EQ(SPIX_R(SPIX_AT(r, 0, 0)), 30);   /* src(0,1) → dst(0,0) */
        CHECK_EQ(SPIX_R(SPIX_AT(r, 1, 0)), 0);    /* src(0,0) → dst(1,0) */
        CHECK_EQ(SPIX_R(SPIX_AT(r, 0, 2)), 50);   /* src(2,1) */
        spix_image_t *r2 = spix_rotate_90(r);
        spix_image_t *r3 = spix_rotate_90(r2);
        spix_image_t *r4 = spix_rotate_90(r3);
        int same = 1;
        for (int y = 0; y < 2 && same; y++)
            for (int x = 0; x < 3; x++)
                if (SPIX_AT(r4, x, y) != SPIX_AT(im, x, y)) { same = 0; break; }
        CHECK_MSG(same, "rotating 4x90 must be the identity");
        spix_free(r2); spix_free(r3); spix_free(r4);
        spix_free(r);
    }

    CHECK_EQ(spix_get_channel(im, 1, 1, 0), 40);
    CHECK_EQ(spix_get_channel(im, 1, 1, 3), 255);
    CHECK_EQ(spix_get_channel(im, -1, 0, 0), -1);
    CHECK_EQ(spix_get_channel(im, 0, 2, 0), -1);
    CHECK_EQ(spix_get_channel(im, 0, 0, 4), -1);
    CHECK_EQ(spix_get_channel(NULL, 0, 0, 0), -1);
    CHECK_EQ(spix_set_channel(im, 2, 1, 1, 200), SPIX_OK);
    CHECK_EQ(spix_get_channel(im, 2, 1, 1), 200);
    CHECK_EQ(spix_set_channel(im, 9, 9, 0, 1), SPIX_ERR_SIZE);
    CHECK_EQ(spix_set_channel(im, 0, 0, 7, 1), SPIX_ERR_SIZE);
    CHECK_EQ(spix_flip_h(NULL), SPIX_ERR_SIZE);
    CHECK_EQ(spix_flip_v(NULL), SPIX_ERR_SIZE);
    CHECK(spix_rotate_90(NULL) == NULL);
    spix_free(im);
}

static void test_components(void) {
    section("connected components: labels, boxes, centroids, tolerance");
    spix_image_t *im = spix_alloc(64, 64);
    for (int y = 5; y <= 20; y++)
        for (int x = 5; x <= 20; x++) set_rgb(im, x, y, 255, 255, 255);
    for (int y = 40; y <= 49; y++)
        for (int x = 30; x <= 39; x++) set_rgb(im, x, y, 255, 255, 255);
    set_rgb(im, 60, 60, 255, 255, 255);

    spix_components_t cc;
    CHECK_EQ(spix_find_components(im, &cc, 0), SPIX_OK);
    CHECK_EQ(cc.count, 3);
    if (cc.count == 3) {
        CHECK_EQ(cc.items[0].x, 5);   CHECK_EQ(cc.items[0].y, 5);
        CHECK_EQ(cc.items[0].w, 16);  CHECK_EQ(cc.items[0].h, 16);
        CHECK_EQ(cc.items[0].area, 256);
        CHECK_EQ(cc.items[0].cx, 12); CHECK_EQ(cc.items[0].cy, 12);
        CHECK_EQ(cc.items[1].x, 30);  CHECK_EQ(cc.items[1].y, 40);
        CHECK_EQ(cc.items[1].w, 10);  CHECK_EQ(cc.items[1].h, 10);
        CHECK_EQ(cc.items[1].area, 100);
        CHECK_EQ(cc.items[1].cx, 34); CHECK_EQ(cc.items[1].cy, 44);
        CHECK_EQ(cc.items[2].area, 1);
        CHECK_EQ(cc.items[2].cx, 60); CHECK_EQ(cc.items[2].cy, 60);
        CHECK_EQ(SPIX_R(cc.items[0].color), 255);
        CHECK_EQ(spix_components_label(&cc, 6, 6), 0);
        CHECK_EQ(spix_components_label(&cc, 35, 45), 1);
        CHECK_EQ(spix_components_label(&cc, 60, 60), 2);
        CHECK_EQ(spix_components_label(&cc, 0, 0), -1);   /* background */
        CHECK_EQ(spix_components_label(&cc, -1, 0), -1);
        CHECK_EQ(spix_components_label(NULL, 0, 0), -1);
        CHECK_EQ(SPIX_R(SPIX_AT(im, 5, 5)), 255);
    }
    /* draw bounding boxes */
    {
        spix_components_t cc2;
        CHECK_EQ(spix_find_components(im, &cc2, 0), SPIX_OK);
        CHECK_EQ(spix_draw_boxes(im, &cc2, SPIX_RGB(255, 0, 0)), SPIX_OK);
        if (cc2.count >= 2) {
            CHECK_EQ(SPIX_R(SPIX_AT(im, cc2.items[0].x, cc2.items[0].y)), 255);
            CHECK_EQ(SPIX_G(SPIX_AT(im, cc2.items[0].x, cc2.items[0].y)), 0);
            CHECK_EQ(SPIX_B(SPIX_AT(im, cc2.items[0].x, cc2.items[0].y)), 0);
            CHECK_EQ(SPIX_R(SPIX_AT(im, cc2.items[1].x + cc2.items[1].w - 1,
                                  cc2.items[1].y + cc2.items[1].h - 1)), 255);
            CHECK_EQ(SPIX_G(SPIX_AT(im, cc2.items[1].x + cc2.items[1].w - 1,
                                    cc2.items[1].y + cc2.items[1].h - 1)), 0);
        }
        spix_components_free(&cc2);
        CHECK(cc2.items == NULL && cc2.labels == NULL && cc2.count == 0);
    }
    spix_components_free(&cc);
    spix_components_free(NULL);   /* must not crash */
    spix_free(im);

    /* bounding box must survive scan-order: max-x pixel found BEFORE min-x */
    {
        spix_image_t *L = spix_alloc(64, 16);
        for (int y = 2; y <= 6; y++) set_rgb(L, 40, y, 255, 255, 255);   /* column  */
        for (int x = 10; x <= 39; x++) set_rgb(L, x, 6, 255, 255, 255);  /* row     */
        spix_components_t c;
        CHECK_EQ(spix_find_components(L, &c, 0), SPIX_OK);
        CHECK_EQ(c.count, 1);
        if (c.count == 1) {
            CHECK_EQ(c.items[0].x, 10);
            CHECK_EQ(c.items[0].y, 2);
            CHECK_EQ(c.items[0].w, 31);    /* 10..40 inclusive */
            CHECK_EQ(c.items[0].h, 5);     /* 2..6  inclusive */
            CHECK_EQ(c.items[0].area, 35); /* 5 + 30 */
            CHECK_EQ(c.items[0].cx, 26);
            CHECK_EQ(c.items[0].cy, 5);
        }
        spix_components_free(&c);
        spix_free(L);
    }

    /* bounding box must not depend on scan order: here the widest pixel is
       found on the first row and the leftmost one only three rows later */
    {
        spix_image_t *S = spix_alloc(64, 8);
        for (int x = 6; x <= 40; x++) set_rgb(S, x, 1, 255, 255, 255);
        set_rgb(S, 6, 2, 255, 255, 255);
        set_rgb(S, 5, 2, 255, 255, 255);
        set_rgb(S, 5, 3, 255, 255, 255);
        spix_components_t c;
        CHECK_EQ(spix_find_components(S, &c, 0), SPIX_OK);
        CHECK_EQ(c.count, 1);
        if (c.count == 1) {
            CHECK_EQ(c.items[0].x, 5);
            CHECK_EQ(c.items[0].y, 1);
            CHECK_EQ(c.items[0].w, 36);     /* 5..40 inclusive */
            CHECK_EQ(c.items[0].h, 3);      /* 1..3  inclusive */
            CHECK_EQ(c.items[0].area, 38);  /* 35 + 2 + 1 */
            CHECK_EQ(c.items[0].cx, 21);
            CHECK_EQ(c.items[0].cy, 1);
        }
        spix_components_free(&c);
        spix_free(S);
    }

    /* tolerance: two adjacent grey levels merge only when tolerance allows */
    {
        spix_image_t *g = spix_alloc(8, 2);
        for (int y = 0; y < 2; y++)
            for (int x = 0; x < 8; x++)
                set_rgb(g, x, y, x < 4 ? 200 : 130, x < 4 ? 200 : 130, x < 4 ? 200 : 130);
        spix_components_t c;
        CHECK_EQ(spix_find_components(g, &c, 0), SPIX_OK);
        CHECK_EQ(c.count, 2);
        spix_components_free(&c);
        CHECK_EQ(spix_find_components(g, &c, 100), SPIX_OK);
        CHECK_EQ(c.count, 1);
        if (c.count == 1) {
            CHECK_EQ(c.items[0].x, 0);
            CHECK_EQ(c.items[0].w, 8);
            CHECK_EQ(c.items[0].area, 16);
        }
        spix_components_free(&c);
        spix_free(g);
    }
    CHECK_EQ(spix_find_components(NULL, &cc, 0), SPIX_ERR_SIZE);
    spix_image_t *dummy = solid(2, 2, 255, 255, 255);
    CHECK_EQ(spix_find_components(dummy, NULL, 0), SPIX_ERR_SIZE);
    CHECK_EQ(spix_draw_boxes(NULL, NULL, 0), SPIX_ERR_SIZE);
    spix_free(dummy);
}

static void test_morphology(void) {
    section("morphology: erode / dilate / open / close / top-hat");
    /* dilate a single pixel → 3x3 block, erode it back */
    spix_image_t *dot = spix_alloc(11, 11);
    set_rgb(dot, 5, 5, 255, 255, 255);
    CHECK_EQ(spix_dilate(dot), SPIX_OK);
    CHECK_EQ(count_nonzero(dot), 9);
    CHECK_EQ(SPIX_R(SPIX_AT(dot, 4, 4)), 255);
    CHECK_EQ(SPIX_R(SPIX_AT(dot, 3, 3)), 0);
    CHECK_EQ(spix_erode(dot), SPIX_OK);
    CHECK_EQ(count_nonzero(dot), 1);
    CHECK_EQ(SPIX_R(SPIX_AT(dot, 5, 5)), 255);
    spix_free(dot);

    /* open removes a single-pixel speckle */
    spix_image_t *sp = spix_alloc(11, 11);
    set_rgb(sp, 5, 5, 255, 255, 255);
    CHECK_EQ(spix_open(sp), SPIX_OK);
    CHECK_EQ(count_nonzero(sp), 0);
    spix_free(sp);

    /* close fills a single-pixel hole inside a 9x9 block */
    spix_image_t *blk = spix_alloc(13, 13);
    for (int y = 2; y <= 10; y++)
        for (int x = 2; x <= 10; x++) set_rgb(blk, x, y, 255, 255, 255);
    set_rgb(blk, 6, 6, 0, 0, 0);
    CHECK_EQ(count_nonzero(blk), 80);
    CHECK_EQ(spix_close(blk), SPIX_OK);
    CHECK_EQ(SPIX_R(SPIX_AT(blk, 6, 6)), 255);
    CHECK_EQ(count_nonzero(blk), 81);
    CHECK_EQ(SPIX_R(SPIX_AT(blk, 1, 1)), 0);
    spix_free(blk);

    /* erode shrinks a 9x9 block to 7x7 */
    {
        spix_image_t *b2 = spix_alloc(13, 13);
        for (int y = 2; y <= 10; y++)
            for (int x = 2; x <= 10; x++) set_rgb(b2, x, y, 255, 255, 255);
        CHECK_EQ(spix_erode(b2), SPIX_OK);
        CHECK_EQ(count_nonzero(b2), 49);
        CHECK_EQ(SPIX_R(SPIX_AT(b2, 3, 3)), 255);
        CHECK_EQ(SPIX_R(SPIX_AT(b2, 2, 2)), 0);
        spix_free(b2);
    }

    /* top-hat keeps a bright detail that opening would erase */
    {
        spix_image_t *th = spix_alloc(15, 15);
        set_rgb(th, 7, 7, 255, 255, 255);
        spix_image_t *out = spix_tophat(th);
        CHECK(out != NULL);
        if (out) {
            CHECK_EQ(SPIX_R(SPIX_AT(out, 7, 7)), 255);
            CHECK_EQ(SPIX_R(SPIX_AT(out, 0, 0)), 0);
            spix_free(out);
        }
        spix_free(th);
    }
    CHECK_EQ(spix_erode(NULL), SPIX_ERR_SIZE);
    CHECK_EQ(spix_dilate(NULL), SPIX_ERR_SIZE);
    CHECK_EQ(spix_open(NULL), SPIX_ERR_SIZE);
    CHECK_EQ(spix_close(NULL), SPIX_ERR_SIZE);
    CHECK(spix_tophat(NULL) == NULL);
}

static void test_arithmetic(void) {
    section("image arithmetic: add / sub / blend / invert / brighten / contrast");
    spix_image_t *a = spix_alloc(2, 2);
    spix_image_t *b = spix_alloc(2, 2);
    for (int y = 0; y < 2; y++)
        for (int x = 0; x < 2; x++) {
            set_rgb(a, x, y, 200, 10, 100);
            set_rgb(b, x, y, 100, 250, 100);
        }
    spix_image_t *sum = spix_add(a, b);
    CHECK(sum != NULL);
    if (sum) {
        CHECK_EQ(SPIX_R(SPIX_AT(sum, 0, 0)), 255);   /* clamped */
        CHECK_EQ(SPIX_G(SPIX_AT(sum, 0, 0)), 255);
        CHECK_EQ(SPIX_B(SPIX_AT(sum, 0, 0)), 200);
        spix_free(sum);
    }
    spix_image_t *dif = spix_sub(a, b);
    CHECK(dif != NULL);
    if (dif) {
        CHECK_EQ(SPIX_R(SPIX_AT(dif, 0, 0)), 100);
        CHECK_EQ(SPIX_G(SPIX_AT(dif, 0, 0)), 0);     /* clamped */
        CHECK_EQ(SPIX_B(SPIX_AT(dif, 0, 0)), 0);
        spix_free(dif);
    }
    spix_image_t *bl0 = spix_blend(a, b, 0.0f);
    spix_image_t *bl1 = spix_blend(a, b, 1.0f);
    spix_image_t *blh = spix_blend(a, b, 0.5f);
    CHECK(bl0 && bl1 && blh);
    if (bl0 && bl1 && blh) {
        CHECK_EQ(SPIX_R(SPIX_AT(bl0, 0, 0)), 200);
        CHECK_EQ(SPIX_R(SPIX_AT(bl1, 0, 0)), 100);
        CHECK_EQ(SPIX_R(SPIX_AT(blh, 0, 0)), 150);
        CHECK_EQ(SPIX_G(SPIX_AT(blh, 0, 0)), 130);
    }
    spix_free(bl0); spix_free(bl1); spix_free(blh);

    spix_image_t *inv = spix_clone(a);
    CHECK_EQ(spix_invert(inv), SPIX_OK);
    CHECK_EQ(SPIX_R(SPIX_AT(inv, 1, 1)), 55);
    CHECK_EQ(SPIX_A(SPIX_AT(inv, 1, 1)), 255);
    spix_free(inv);

    spix_image_t *br = spix_brighten(a, 40);
    CHECK(br != NULL);
    if (br) {
        CHECK_EQ(SPIX_R(SPIX_AT(br, 0, 0)), 240);
        CHECK_EQ(SPIX_G(SPIX_AT(br, 0, 0)), 50);
        spix_free(br);
    }
    spix_image_t *dk = spix_brighten(a, -40);
    CHECK(dk != NULL);
    if (dk) {
        CHECK_EQ(SPIX_R(SPIX_AT(dk, 0, 0)), 160);
        CHECK_EQ(SPIX_G(SPIX_AT(dk, 0, 0)), 0);
        spix_free(dk);
    }
    spix_image_t *c1 = spix_contrast(a, 1.0f);
    spix_image_t *c2 = spix_contrast(a, 2.0f);
    CHECK(c1 && c2);
    if (c1 && c2) {
        CHECK_EQ(SPIX_R(SPIX_AT(c1, 0, 0)), 200);   /* 1.0 = identity */
        CHECK_EQ(SPIX_G(SPIX_AT(c1, 0, 0)), 10);
        CHECK_EQ(SPIX_R(SPIX_AT(c2, 0, 0)), 255);   /* (200-128)*2+128 */
        CHECK_EQ(SPIX_G(SPIX_AT(c2, 0, 0)), 0);     /* (10-128)*2+128 <0 */
        spix_free(c1); spix_free(c2);
    }
    /* blend of 0 and 255 at 0.5 is 127 (truncated, not rounded) */
    {
        spix_image_t *z = solid(1, 1, 0, 0, 0);
        spix_image_t *o = solid(1, 1, 255, 255, 255);
        spix_image_t *m = spix_blend(z, o, 0.5f);
        CHECK(m != NULL);
        if (m) { CHECK_EQ(SPIX_R(SPIX_AT(m, 0, 0)), 127); spix_free(m); }
        spix_free(z); spix_free(o);
    }
    CHECK(spix_add(NULL, a) == NULL);
    CHECK(spix_add(a, NULL) == NULL);
    {
        spix_image_t *small = solid(1, 1, 0, 0, 0);
        CHECK(spix_add(a, small) == NULL);
        CHECK(spix_sub(a, small) == NULL);
        CHECK(spix_blend(a, small, 0.5f) == NULL);
        spix_free(small);
    }
    CHECK(spix_sub(NULL, NULL) == NULL);
    CHECK(spix_blend(a, b, 0.5f) != NULL);
    CHECK(spix_invert(NULL) == SPIX_ERR_SIZE);
    CHECK(spix_brighten(NULL, 1) == NULL);
    CHECK(spix_contrast(NULL, 1.0f) == NULL);
    spix_free(a);
    spix_free(b);
}

static void test_demo_workflow(void) {
    section("integration: full pipeline on one image (no files left behind)");
    /* mirrors examples/demo.c but with deterministic input */
    img_t g = gen_checker(128, 96);
    spix_image_t *src = img_to_spix(&g);
    spix_image_t *half = spix_resize_bilinear(src, 64, 48);
    CHECK(half != NULL);
    if (half) { CHECK_EQ(half->width, 64); CHECK_EQ(half->height, 48); spix_free(half); }
    spix_image_t *blur = spix_convolve(src, &SPIX_KERNEL_GAUSS_BLUR_3);
    CHECK(blur != NULL);
    if (blur) { CHECK_EQ(blur->width, 128); spix_free(blur); }
    spix_image_t *work = spix_clone(src);
    CHECK_EQ(spix_grayscale(work), SPIX_OK);
    uint8_t th = spix_threshold_otsu(work);
    CHECK_EQ(spix_threshold(work, th), SPIX_OK);
    CHECK_EQ(spix_open(work), SPIX_OK);
    spix_components_t cc;
    CHECK_EQ(spix_find_components(work, &cc, 0), SPIX_OK);
    CHECK(cc.count >= 1);
    CHECK_EQ(spix_draw_boxes(work, &cc, SPIX_RGB(255, 0, 0)), SPIX_OK);
    spix_components_free(&cc);
    const char *out = "spix_test_pipeline.bmp";
    CHECK_EQ(spix_write_bmp(work, out), SPIX_OK);
    spix_image_t *reread = spix_read_ppm("spix_test_io.ppm");   /* absent → NULL */
    CHECK(reread == NULL);
    remove(out);
    CHECK_EQ(file_size_bytes(out), -1);
    spix_free(work);
    spix_free(src);
    img_release(&g);
}

/* ══════════════════════════════════════════════════════════════════
 *  main
 * ══════════════════════════════════════════════════════════════════ */

int main(void) {
    printf("spix test suite\n");
    printf("===============\n");

    test_lifecycle();
    test_ppm_io();
    test_bmp_io();
    test_resize_nearest();
    test_resize_bilinear_quality();
    test_resize_bilinear_pins();
    test_convolution();
    test_color();
    test_dither();
    test_geometry();
    test_components();
    test_morphology();
    test_arithmetic();
    test_demo_workflow();

    printf("\n========================================\n");
    if (g_fails == 0)
        printf("tests: %d checks, 0 failures — OK\n", g_checks);
    else
        printf("tests: %d checks, %d FAILURES\n", g_checks, g_fails);
    return g_fails ? 1 : 0;
}
