/**
 * bench/bench_spix.c — throughput benchmark for spix.h
 *
 * Measures wall-clock time and megapixels/second for the library's main
 * operations on FIXED synthetic inputs, so runs are comparable.
 *
 * Protocol
 *   - every case is measured 3 times (REPS), each repetition running `iters`
 *     iterations of the operation on the same input;
 *   - iterations are auto-calibrated so one repetition takes ~0.35 s;
 *   - the report shows all three timings, the median throughput read/written,
 *     the best-of-three throughput (the least noise-contaminated estimate on a
 *     busy machine) and the spread ((max-min)/median) instead of one number;
 *   - throughput is counted in megapixels READ and megapixels WRITTEN per
 *     second (for resize these differ by the scale factor);
 *   - in-place operations are measured as "clone + op", because the operation
 *     itself destroys its input (the clone cost is part of the timing).
 *
 * Run:  make bench
 */

#define _POSIX_C_SOURCE 199309L
#define SPIX_IMPLEMENTATION
#include "../spix.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define REPS        3
#define TARGET_S    0.35
#define MAX_ITERS   2000

/* ── inputs ──────────────────────────────────────────────────── */

static spix_image_t *g_img;      /* 1024x768 colour, deterministic */
static spix_image_t *g_half;     /* same size, second operand for blend */
static spix_image_t *g_bin;      /* 1024x768 binary shapes */
static const char   *IO_PATH = "spix_bench_io.ppm";

static uint32_t lcg(uint32_t *s) {
    *s = *s * 1664525u + 1013904223u;
    return *s;
}

static void build_inputs(void) {
    uint32_t s = 0xC0FFEEu;
    g_img = spix_alloc(1024, 768);
    g_half = spix_alloc(1024, 768);
    for (int y = 0; y < 768; y++)
        for (int x = 0; x < 1024; x++) {
            /* mixture of gradients, hard edges and noise: not a trivial image */
            int r = (x * 255) / 1023;
            int g = (y * 255) / 767;
            int b = ((lcg(&s) >> 24) * (x / 128 + 1)) / 9;
            if (((x / 32) + (y / 32)) & 1) { r = 255 - r; b = 255 - b; }
            SPIX_AT(g_img, x, y) = SPIX_RGB((uint8_t)r, (uint8_t)g, (uint8_t)b);
            SPIX_AT(g_half, x, y) = SPIX_RGB((uint8_t)g, (uint8_t)b, (uint8_t)r);
        }
    g_bin = spix_alloc(1024, 768);
    for (int i = 0; i < 64; i++) {
        int w = 20 + (int)(lcg(&s) % 120);
        int h = 20 + (int)(lcg(&s) % 120);
        int x0 = (int)(lcg(&s) % (unsigned)(1024 - w));
        int y0 = (int)(lcg(&s) % (unsigned)(768 - h));
        for (int y = y0; y < y0 + h; y++)
            for (int x = x0; x < x0 + w; x++)
                SPIX_AT(g_bin, x, y) = SPIX_WHITE;
    }
}

/* ── timing ──────────────────────────────────────────────────── */

static double now_s(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

/* ── operations ──────────────────────────────────────────────── */

static volatile size_t g_sink;   /* keeps the optimiser honest */

static void op_resize_bilinear_up(int n) {
    for (int i = 0; i < n; i++) {
        spix_image_t *d = spix_resize_bilinear(g_img, 2048, 1536);
        g_sink += d ? (size_t)d->pixels[0] : 0;
        spix_free(d);
    }
}
static void op_resize_bilinear_down(int n) {
    for (int i = 0; i < n; i++) {
        spix_image_t *d = spix_resize_bilinear(g_img, 512, 384);
        g_sink += d ? (size_t)d->pixels[0] : 0;
        spix_free(d);
    }
}
static void op_resize_nn_up(int n) {
    for (int i = 0; i < n; i++) {
        spix_image_t *d = spix_resize_nn(g_img, 2048, 1536);
        g_sink += d ? (size_t)d->pixels[0] : 0;
        spix_free(d);
    }
}
static void op_resize_nn_down(int n) {
    for (int i = 0; i < n; i++) {
        spix_image_t *d = spix_resize_nn(g_img, 512, 384);
        g_sink += d ? (size_t)d->pixels[0] : 0;
        spix_free(d);
    }
}
static void op_gauss3(int n) {
    for (int i = 0; i < n; i++) {
        spix_image_t *d = spix_convolve(g_img, &SPIX_KERNEL_GAUSS_BLUR_3);
        g_sink += d ? (size_t)d->pixels[0] : 0;
        spix_free(d);
    }
}
static void op_edge3(int n) {
    for (int i = 0; i < n; i++) {
        spix_image_t *d = spix_convolve(g_img, &SPIX_KERNEL_EDGE_DETECT);
        g_sink += d ? (size_t)d->pixels[0] : 0;
        spix_free(d);
    }
}
static void op_grayscale(int n) {
    for (int i = 0; i < n; i++) {
        spix_image_t *c = spix_clone(g_img);
        spix_grayscale(c);
        g_sink += SPIX_R(SPIX_AT(c, 10, 10));
        spix_free(c);
    }
}
static void op_threshold(int n) {
    for (int i = 0; i < n; i++) {
        spix_image_t *c = spix_clone(g_img);
        spix_threshold(c, 128);
        g_sink += SPIX_R(SPIX_AT(c, 10, 10));
        spix_free(c);
    }
}
static void op_equalize(int n) {
    for (int i = 0; i < n; i++) {
        spix_image_t *c = spix_clone(g_img);
        spix_equalize(c);
        g_sink += SPIX_R(SPIX_AT(c, 10, 10));
        spix_free(c);
    }
}
static void op_invert(int n) {
    for (int i = 0; i < n; i++) {
        spix_image_t *c = spix_clone(g_img);
        spix_invert(c);
        g_sink += SPIX_R(SPIX_AT(c, 10, 10));
        spix_free(c);
    }
}
static void op_brighten(int n) {
    for (int i = 0; i < n; i++) {
        spix_image_t *d = spix_brighten(g_img, 40);
        g_sink += d ? (size_t)d->pixels[0] : 0;
        spix_free(d);
    }
}
static void op_blend(int n) {
    for (int i = 0; i < n; i++) {
        spix_image_t *d = spix_blend(g_img, g_half, 0.5f);
        g_sink += d ? (size_t)d->pixels[0] : 0;
        spix_free(d);
    }
}
static void op_dither_floyd(int n) {
    for (int i = 0; i < n; i++) {
        spix_image_t *c = spix_clone(g_img);
        spix_dither_floyd(c);
        g_sink += SPIX_R(SPIX_AT(c, 10, 10));
        spix_free(c);
    }
}
static void op_dither_bayer(int n) {
    for (int i = 0; i < n; i++) {
        spix_image_t *c = spix_clone(g_img);
        spix_dither_bayer(c);
        g_sink += SPIX_R(SPIX_AT(c, 10, 10));
        spix_free(c);
    }
}
static void op_components(int n) {
    for (int i = 0; i < n; i++) {
        spix_components_t cc;
        if (spix_find_components(g_bin, &cc, 0) == SPIX_OK) {
            g_sink += (size_t)cc.count;
            spix_components_free(&cc);
        }
    }
}
static void op_morphology(int n) {
    for (int i = 0; i < n; i++) {
        spix_image_t *c = spix_clone(g_bin);
        spix_open(c);
        g_sink += SPIX_R(SPIX_AT(c, 10, 10));
        spix_free(c);
    }
}
static void op_write_ppm(int n) {
    for (int i = 0; i < n; i++)
        if (spix_write_ppm(g_img, IO_PATH) != SPIX_OK) return;
}
static void op_read_ppm(int n) {
    for (int i = 0; i < n; i++) {
        spix_image_t *d = spix_read_ppm(IO_PATH);
        g_sink += d ? (size_t)d->width : 0;
        spix_free(d);
    }
}

/* ── runner ──────────────────────────────────────────────────── */

typedef struct {
    const char *label;
    double      mpix_in;    /* megapixels read per iteration  */
    double      mpix_out;   /* megapixels written per iteration */
    void      (*fn)(int);
} case_t;

#define MP_IN   (1024.0 * 768.0 / 1e6)
#define MP_OUT2 (2048.0 * 1536.0 / 1e6)
#define MP_HALF (512.0 * 384.0 / 1e6)

static int cmp_double(const void *a, const void *b) {
    double x = *(const double *)a, y = *(const double *)b;
    return (x < y) ? -1 : (x > y) ? 1 : 0;
}

static void run_case(const case_t *c) {
    double t;
    int iters = 1;
    /* calibrate */
    t = now_s();
    c->fn(1);
    t = now_s() - t;
    if (t > 0.0) {
        double want = TARGET_S / t;
        if (want < 1.0) want = 1.0;
        if (want > (double)MAX_ITERS) want = (double)MAX_ITERS;
        iters = (int)(want + 0.5);
    }

    double times[REPS], mps_out[REPS], mps_in[REPS];
    for (int r = 0; r < REPS; r++) {
        double t0 = now_s();
        c->fn(iters);
        times[r] = now_s() - t0;
        mps_in[r]  = (times[r] > 0.0) ? (double)iters * c->mpix_in  / times[r] : 0.0;
        mps_out[r] = (times[r] > 0.0) ? (double)iters * c->mpix_out / times[r] : 0.0;
    }
    double sorted[REPS];
    memcpy(sorted, mps_out, sizeof sorted);
    qsort(sorted, REPS, sizeof(double), cmp_double);
    double med = sorted[REPS / 2];
    double spread = (med > 0.0) ? (sorted[REPS - 1] - sorted[0]) / med * 100.0 : 0.0;
    /* best of the three repetitions: the least noise-contaminated estimate of
       what the code can do when the machine is not busy with something else */
    double tmin = times[0];
    for (int r = 1; r < REPS; r++) if (times[r] < tmin) tmin = times[r];
    double best = (tmin > 0.0) ? (double)iters * c->mpix_out / tmin : 0.0;
    double med_in;
    {
        double sc[REPS];
        memcpy(sc, mps_in, sizeof sc);
        qsort(sc, REPS, sizeof(double), cmp_double);
        med_in = sc[REPS / 2];
    }

    printf("  %-38s %6d  ", c->label, iters);
    for (int r = 0; r < REPS; r++) printf("%7.4f ", times[r]);
    printf(" %8.2f %8.2f %8.2f %6.1f%%\n", med_in, med, best, spread);
    fflush(stdout);
}

static void print_cpu(void) {
    FILE *fp = fopen("/proc/cpuinfo", "r");
    char line[256];
    if (!fp) return;
    while (fgets(line, sizeof line, fp)) {
        if (strncmp(line, "model name", 10) == 0) {
            char *p = strchr(line, ':');
            if (p) {
                p++;
                while (*p == ' ') p++;
                p[strcspn(p, "\n")] = 0;
                printf("cpu:         %s\n", p);
            }
            break;
        }
    }
    fclose(fp);
    {
        long n = sysconf(_SC_NPROCESSORS_ONLN);
        if (n > 0) printf("cores:       %ld (benchmark is single-threaded)\n", n);
    }
}

int main(void) {
    build_inputs();
    if (!g_img || !g_half || !g_bin) { fprintf(stderr, "OOM\n"); return 2; }
    spix_write_ppm(g_img, IO_PATH);

    static const case_t cases[] = {
        {"resize_bilinear 1024x768 -> 2048x1536", MP_IN, MP_OUT2, op_resize_bilinear_up},
        {"resize_bilinear 1024x768 ->  512x384",  MP_IN, MP_HALF, op_resize_bilinear_down},
        {"resize_nn       1024x768 -> 2048x1536", MP_IN, MP_OUT2, op_resize_nn_up},
        {"resize_nn       1024x768 ->  512x384",  MP_IN, MP_HALF, op_resize_nn_down},
        {"convolve gauss 3x3",                    MP_IN, MP_IN,   op_gauss3},
        {"convolve edge-detect 3x3",              MP_IN, MP_IN,   op_edge3},
        {"blend 0.5 + clone",                     MP_IN, MP_IN,   op_blend},
        {"brighten +40",                          MP_IN, MP_IN,   op_brighten},
        {"clone + grayscale",                     MP_IN, MP_IN,   op_grayscale},
        {"clone + threshold 128",                 MP_IN, MP_IN,   op_threshold},
        {"clone + equalize",                      MP_IN, MP_IN,   op_equalize},
        {"clone + invert",                        MP_IN, MP_IN,   op_invert},
        {"clone + dither_floyd",                  MP_IN, MP_IN,   op_dither_floyd},
        {"clone + dither_bayer",                  MP_IN, MP_IN,   op_dither_bayer},
        {"morphology: clone + open (binary)",     MP_IN, MP_IN,   op_morphology},
        {"find_components (binary, 64 shapes)",   MP_IN, MP_IN,   op_components},
        {"write_ppm 1024x768 (disk I/O)",         MP_IN, MP_IN,   op_write_ppm},
        {"read_ppm  1024x768 (disk I/O)",         MP_IN, MP_IN,   op_read_ppm}
    };

    printf("spix benchmark\n");
    printf("==============\n");
    print_cpu();
    printf("compiler:    %s\n", __VERSION__);
    printf("input:       1024x768 (0.786 Mpx) synthetic colour / binary image\n");
    printf("method:      %d repetitions, auto-calibrated to ~%.2f s each. Throughput is\n"
           "             megapixels READ and WRITTEN per second (resize writes a different\n"
           "             number of pixels than it reads). In-place ops are measured as\n"
           "             clone+op, the clone cost included.\n\n",
           REPS, TARGET_S);

    printf("  %-38s %6s  %7s %7s %7s  %8s %8s %8s %7s\n",
           "case", "iters", "t1(s)", "t2(s)", "t3(s)",
           "MP/s rd", "MP/s wr", "best wr", "spread");
    printf("  %-38s %6s  %7s %7s %7s  %8s %8s %8s %7s\n",
           "--------------------------------------", "-----",
           "-------", "-------", "-------", "--------", "--------", "--------",
           "------");

    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++)
        run_case(&cases[i]);

    printf("\n  (sink=%zu, kept alive so the compiler cannot drop the work)\n", g_sink);
    remove(IO_PATH);
    spix_free(g_img);
    spix_free(g_half);
    spix_free(g_bin);
    return 0;
}
