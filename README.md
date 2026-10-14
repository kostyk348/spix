# spix — Single-header C99 image processing library

[![CI](https://github.com/kostyk348/spix/actions/workflows/ci.yml/badge.svg)](https://github.com/kostyk348/spix/actions/workflows/ci.yml)


Fixed-point arithmetic throughout. No FPU required. Zero dependencies beyond libc.

```c
#define SPIX_IMPLEMENTATION
#include "spix.h"

spix_image_t *img = spix_read_ppm("input.ppm");
spix_equalize(img);                    // histogram equalization
spix_open(img);                        // morphology: remove noise
spix_components_t cc;
spix_find_components(img, &cc, 0);     // find connected regions
spix_write_bmp(img, "output.bmp");     // save as BMP
```

## Features

| Category | Operations |
|----------|-----------|
| **IO** | PPM/PGM read/write (P2/P3/P5/P6), BMP write (24-bit uncompressed) |
| **Resize** | Nearest-neighbor, bilinear (Q16.16 fixed-point) |
| **Convolution** | 3×3 kernels: box blur, gaussian blur, sharpen, edge detect, emboss, sobel |
| **Color** | Grayscale, threshold, Otsu adaptive, histogram equalization |
| **Morphology** | Erode, dilate, open, close, top-hat |
| **Connected components** | Union-find labeling, bounding boxes, centroid |
| **Image arithmetic** | Add, subtract, blend, invert, brighten, contrast |
| **Dithering** | Floyd-Steinberg error diffusion, Bayer 4×4 ordered |
| **Geometry** | Horizontal/vertical flip, 90° rotation |
| **Pixel access** | Get/set individual channels |

## Design

- **No FPU**: integer arithmetic only (Q16.16 for geometry, integer for convolution)
- **No external deps**: pure libc (`stdio`, `stdint`, `stdlib`, `string`)
- **Single header**: `#define SPIX_IMPLEMENTATION` before inclusion
- **RGBA8888**: 32-bit pixels, 8 bits per channel

## Build demo

```bash
cc -O2 -o demo examples/demo.c -lm
./demo generate    # create test image
./demo             # apply all filters
```

Or just use the API directly:

```c
#define SPIX_IMPLEMENTATION

[![CI](https://github.com/kostyk348/spix/actions/workflows/ci.yml/badge.svg)](https://github.com/kostyk348/spix/actions/workflows/ci.yml)

#include "spix.h"

[![CI](https://github.com/kostyk348/spix/actions/workflows/ci.yml/badge.svg)](https://github.com/kostyk348/spix/actions/workflows/ci.yml)


int main(void) {
    spix_image_t *img = spix_read_ppm("photo.ppm");
    spix_equalize(img);
    spix_write_bmp(img, "equalized.bmp");
    spix_free(img);
    return 0;
}
```

## API Reference

### Lifecycle

```c
spix_image_t *spix_alloc(int width, int height);           // zeroed pixels
spix_image_t  spix_wrap(int w, int h, spix_pixel_t *p, int stride); // non-owning
void          spix_free(spix_image_t *img);
spix_image_t *spix_clone(const spix_image_t *src);         // deep copy
```

### IO

```c
spix_image_t *spix_read_ppm(const char *path);             // P2/P3/P5/P6
int           spix_write_ppm(const spix_image_t *img, const char *path); // P6
spix_image_t *spix_read_ppm_mem(const uint8_t *buf, size_t len);
size_t        spix_write_ppm_mem(const spix_image_t *img, uint8_t **out);
int           spix_write_bmp(const spix_image_t *img, const char *path); // 24-bit BMP
size_t        spix_write_bmp_mem(const spix_image_t *img, uint8_t **out);
```

### Resize

```c
spix_image_t *spix_resize_nn(const spix_image_t *src, int new_w, int new_h);
spix_image_t *spix_resize_bilinear(const spix_image_t *src, int new_w, int new_h);
```

### Convolution

```c
spix_image_t *spix_convolve(const spix_image_t *src, const spix_kernel_t *k);
int           spix_convolve_to(const spix_image_t *src, spix_image_t *dst, const spix_kernel_t *k);
```

Predefined kernels: `SPIX_KERNEL_BOX_BLUR_3`, `SPIX_KERNEL_GAUSS_BLUR_3`, `SPIX_KERNEL_SHARPEN`, `SPIX_KERNEL_EDGE_DETECT`, `SPIX_KERNEL_EMBOSS`, `SPIX_KERNEL_SOBEL_H`, `SPIX_KERNEL_SOBEL_V`.

### Color

```c
int     spix_grayscale(spix_image_t *img);                  // luminosity
int     spix_threshold(spix_image_t *img, uint8_t thresh);
uint8_t spix_threshold_otsu(const spix_image_t *img);      // returns threshold
int     spix_equalize(spix_image_t *img);                   // histogram equalization
```

### Morphology (3×3 structuring element)

```c
int           spix_erode(spix_image_t *img);    // in-place, binary
int           spix_dilate(spix_image_t *img);   // in-place, binary
int           spix_open(spix_image_t *img);     // erode + dilate
int           spix_close(spix_image_t *img);    // dilate + erode
spix_image_t *spix_tophat(const spix_image_t *src); // original - open
```

### Connected components

```c
typedef struct {
    spix_component_t *items;
    int count;
    int *labels;           // label map (width × height)
    int width, height;
} spix_components_t;

int  spix_find_components(const spix_image_t *img, spix_components_t *cc, int tolerance);
void spix_components_free(spix_components_t *cc);
int  spix_components_label(const spix_components_t *cc, int x, int y);
int  spix_draw_boxes(spix_image_t *img, const spix_components_t *cc, spix_pixel_t color);
```

### Image arithmetic

```c
spix_image_t *spix_add(const spix_image_t *a, const spix_image_t *b);
spix_image_t *spix_sub(const spix_image_t *a, const spix_image_t *b);
spix_image_t *spix_blend(const spix_image_t *a, const spix_image_t *b, float alpha);
int           spix_invert(spix_image_t *img);
spix_image_t *spix_brighten(const spix_image_t *src, int offset);   // -255..+255
spix_image_t *spix_contrast(const spix_image_t *src, float factor); // 1.0=identity
```

### Dithering

```c
int spix_dither_floyd(spix_image_t *img);   // Floyd-Steinberg error diffusion
int spix_dither_bayer(spix_image_t *img);   // Bayer ordered 4×4
```

### Geometry

```c
int           spix_flip_h(spix_image_t *img);
int           spix_flip_v(spix_image_t *img);
spix_image_t *spix_rotate_90(const spix_image_t *src);
```

### Pixel access

```c
int spix_get_channel(const spix_image_t *img, int x, int y, int channel); // 0=R,1=G,2=B,3=A
int spix_set_channel(spix_image_t *img, int x, int y, int channel, uint8_t value);
```

### Macros

```c
SPIX_RGBA(r, g, b, a)    // construct pixel
SPIX_RGB(r, g, b)         // construct pixel (alpha=0xFF)
SPIX_R(p) / SPIX_G(p) / SPIX_B(p) / SPIX_A(p)  // extract channel
SPIX_AT(img, x, y)        // pixel at coordinates
SPIX_CLAMP(v, lo, hi)
SPIX_BLACK / SPIX_WHITE
```

## Tests and benchmarks

```bash
make test     # correctness suite — exit code 0 means every check passed
make bench    # throughput benchmark (timing-sensitive, not run by CI)
make demo     # the original demo pipeline: generate a pattern, then filter it
```

CI (`.github/workflows/ci.yml`) runs `make` and `make test`.

### Correctness suite

`tests/test_spix.c` is a dependency-free suite (own assert harness, no
framework, no external code) with **511 checks** covering lifecycle, PPM/PGM
and BMP byte-level I/O, resize, convolution, colour, dithering, geometry,
connected components, morphology, image arithmetic and error paths.

It contains its own **golden model**: an independent, double-precision bilinear
resampler that implements the *same* Q16.16 mapping the library documents
(`index = i * (src_dim - 1) / dst_dim`, truncated). Because the fixed-point
accumulator is exact in 64-bit integers, the golden model is bit-comparable:
after the arithmetic fix below, `spix_resize_bilinear()` agrees with it
**exactly** on all 28 image/scale combinations. A second, *centred*
(half-pixel) reference is reported as well — the gap against it measures the
mapping convention, not a bug.

Quality of `spix_resize_bilinear()` against the golden model — 4 synthetic
images (gradient, 8-px checkerboard, seeded LCG noise, one-pixel lines) and 7
target sizes each (2× up, 2× down, 1.5× up, 1×1, W×1, 1×H, 53×31):

| image | source | PSNR vs golden model (before fix → now) | max abs deviation (now) | PSNR vs centred ref (now) |
|-------|--------|------------------------------------------|------------------------|---------------------------|
| gradient | 320×240 | 5.01 – 8.89 dB → exact | 0 | 6.05 – 49.58 dB |
| checkerboard | 256×256 | 3.03 – 4.77 dB → exact | 0 | 3.39 – 16.18 dB |
| seeded noise | 200×150 | 2.50 – 5.50 dB → exact | 0 | 10.11 – 17.02 dB |
| one-pixel lines | 257×129 | 0.01 – 17.55 dB → exact | 0 | 10.09 – 34.41 dB |

("exact" = 0 of all samples off by more than 0 LSB, PSNR infinite. The full
28-row table is printed by `make test`.)

Tolerance in the suite: `max |deviation| <= 1 LSB` against the golden model and
PSNR >= 45 dB; `spix_resize_nn()` is required to be bit-exact.

### Throughput

Measured on this machine, single-threaded, `-O2`:

```
cpu:      AMD Ryzen 7 7840HS, 16 threads
compiler: gcc 16.1.1
input:    1024x768 (0.786 Mpx) synthetic colour / binary image
method:   3 repetitions, auto-calibrated to ~0.35 s each;
          throughput in megapixels READ and WRITTEN per second ("best wr" is
          the least noise-contaminated of the three repetitions);
          in-place operations measured as clone+op (clone cost included)
```

Table below: one run of this binary (the least loaded of the runs taken; see
the variance note after it).

| case | iters | t1 (s) | t2 (s) | t3 (s) | MP/s read | MP/s written (median) | MP/s written (best) | spread |
|------|-------|--------|--------|--------|-----------|----------------------|---------------------|--------|
| resize_bilinear 1024×768 → 2048×1536 | 6 | 0.3253 | 0.3908 | 0.3788 | 12.46 | 49.83 | 58.03 | 19.5% |
| resize_bilinear 1024×768 → 512×384 | 98 | 0.4365 | 0.3552 | 0.4059 | 189.85 | 47.46 | 54.24 | 21.3% |
| resize_nn 1024×768 → 2048×1536 | 53 | 0.3765 | 0.4203 | 0.3651 | 110.72 | 442.86 | 456.61 | 13.5% |
| resize_nn 1024×768 → 512×384 | 698 | 0.3466 | 0.3325 | 0.3187 | 1650.86 | 412.72 | 430.62 | 8.4% |
| convolve gauss 3×3 | 16 | 0.3203 | 0.4786 | 0.4677 | 26.90 | 26.90 | 39.28 | 48.3% |
| convolve edge-detect 3×3 | 12 | 0.4390 | 0.3656 | 0.3548 | 25.82 | 25.82 | 26.60 | 19.8% |
| blend 0.5 + clone | 63 | 0.3531 | 0.2637 | 0.2922 | 169.55 | 169.55 | 187.90 | 28.1% |
| brighten +40 | 114 | 0.3110 | 0.2531 | 0.2483 | 354.28 | 354.28 | 361.07 | 20.6% |
| clone + grayscale | 167 | 0.3687 | 0.4090 | 0.4615 | 321.11 | 321.11 | 356.19 | 22.3% |
| clone + threshold 128 | 98 | 0.3020 | 0.2710 | 0.2646 | 284.34 | 284.34 | 291.28 | 12.7% |
| clone + equalize | 110 | 0.3524 | 0.3506 | 0.4107 | 245.46 | 245.46 | 246.76 | 14.7% |
| clone + invert | 173 | 0.3889 | 0.4869 | 0.6333 | 279.44 | 279.44 | 349.80 | 48.3% |
| clone + dither_floyd | 31 | 0.3404 | 0.2980 | 0.3743 | 71.63 | 71.63 | 81.81 | 23.3% |
| clone + dither_bayer | 41 | 0.2606 | 0.2894 | 0.1774 | 123.71 | 123.71 | 181.74 | 56.8% |
| morphology: clone + open (binary) | 13 | 0.3911 | 0.3828 | 0.3857 | 26.51 | 26.51 | 26.70 | 2.1% |
| find_components (binary, 64 shapes) | 29 | 0.3356 | 0.3247 | 0.3247 | 70.23 | 70.23 | 70.24 | 3.3% |
| write_ppm 1024×768 (disk I/O) | 12 | 0.3644 | 0.2222 | 0.2273 | 41.52 | 41.52 | 42.48 | 39.9% |
| read_ppm 1024×768 (disk I/O) | 29 | 0.2526 | 0.2206 | 0.2833 | 90.28 | 90.28 | 103.37 | 25.3% |

**These numbers are not a stable baseline — read the spread and the note, not
the medians.** The box is shared with other work, and repeated runs of the
*same binary on the same commit* differ a lot:

| case | MP/s written, run 1 (quiet) | run 2 (loaded) | run 3 (this table) |
|------|-----------------------------|----------------|--------------------|
| resize_nn 1024×768 → 512×384 | 208.21 | 118.88 | 412.72 |
| resize_bilinear 1024×768 → 2048×1536 | 22.35 | 18.96 | 49.83 |
| convolve edge-detect 3×3 | 13.66 | 7.56 | 25.82 |
| find_components (binary) | 96.61 | 62.30 | 70.23 |

Per-repetition spreads reach 134% on the short cases (`clone + invert`), which
is scheduler/allocator noise, not algorithmic variance. Only the compute-bound
cases are reproducible (`morphology open` 2.1%, `find_components` 3.3%,
`resize_nn` downscale 8.4% when the iteration count is high). Absolute
throughput on this library should be treated as order-of-magnitude until the
numbers are re-taken on an idle machine.

### What is NOT measured

- **No comparison with `stb_image_resize` or any other resampler.** The task
  scope forbids external dependencies, so the golden model is a
  double-precision bilinear with the *same* mapping. It is a correctness
  oracle, not a quality winner: for 2× downscaling a box/triangle filter would
  produce a different (arguably better) image, and the PSNR against the centred
  reference (3.4 – 49.6 dB) quantifies that convention gap.
- **No FPU-free verification.** The "no FPU required" claim was not checked by
  disassembly, `-msoft-float`, or on embedded hardware; `spix_blend`,
  `spix_contrast` and `spix_threshold_otsu` use `float`/`double` internally.
- **No stable throughput baseline.** Measurements were taken on a machine that
  also runs other jobs, so the same binary on the same commit varied by up to
  3.5× between runs; only compute-bound cases (morphology, components,
  high-iteration resize) reproduce within ~10%. No profiling (perf/VTune), no
  instruction counts, no comparison against SIMD/`-O3` variants.
- **No memory, cache or peak-RSS measurements**, no NUMA/multi-core scaling
  (everything is single-threaded), no thread-safety tests.
- **No ARM/RISC-V, no other compilers**, no `-O0`/`-O3`/auto-vectorisation
  comparison, no 4K/16-bit images (max tested size: 1024×768 → 2048×1536).
- **No visual quality judgement of dithering**; only "emits black/white only"
  and "preserves the mean within 2/255" are asserted.
- **Disk I/O numbers** are tmpfs/SSD numbers on this machine only.

### Behaviour changes in this branch (vs upstream `84be243`)

Every change below is pinned by a failing-then-passing check; `make test` was
red (81 failures / 477 checks) before them and is green (511 checks, 0
failures) after.

1. **The header did not compile at all.** `#ifndef SPIX_H` had no matching
   `#endif` (the only one at EOF closed `SPIX_IMPLEMENTATION`), so the include
   guard was unterminated: `cc -std=c99` → `error: unterminated #ifndef`.
   Upstream CI is red for this reason. Added `#endif /* SPIX_H */`.
2. **`spix_read_ppm()` lost one byte per read.** `_spix_read_int()` consumed the
   delimiter that ends `maxval`, and the reader then consumed one more byte —
   eating the first pixel byte of P6/P5 (byte-shifted image) or the first digit
   of P3/P2; small P5 files returned `NULL`. `_spix_read_int()` now leaves the
   delimiter in the stream and the reader consumes exactly one (CRLF-aware).
   Regression tests use first pixels whose bytes are whitespace values
   (`10`, `32`, `9`) so a future "just skip whitespace" fix cannot pass.
3. **`spix_read_ppm_mem()` ignored `maxval`** (a P6 with maxval 15 came back
   15× too bright). It now scales like the file reader.
4. **`spix_resize_bilinear()` overflowed 32-bit ints and shifted by 32**
   (intermediate values reach 2^41; `>> 32` on `int` is UB — output differed
   between `-O0` and `-O2`, and PSNR against the golden model was
   0.01 – 17.55 dB). The accumulator is now `int64_t`; the operator is
   bit-exact against the golden model.
5. **`spix_convolve_to()` subtracted 128 unconditionally**, so every kernel
   with `shift > 0` (box and gaussian blur) darkened its result: a flat grey
   100 image came out black (`0`). The mid-grey offset is now applied only to
   kernels that sum to zero (edge detect, sobel: flat areas become 128, which
   the tests pin); level-preserving kernels (blur, sharpen, emboss) are no
   longer offset.
6. **`spix_threshold_otsu()` was off by one against `spix_threshold()`**: Otsu
   returns the last level of the dark class, but `spix_threshold()` treats
   `value >= t` as foreground, so the documented pair
   `spix_threshold(img, spix_threshold_otsu(img))` marked the whole dark class
   white on a perfectly bimodal image. It now returns `t + 1` (clamped).
7. **`spix_find_components()` bounding boxes were scan-order dependent**: the
   width/height were grown only while pixels kept arriving to the right of the
   current box, so a component whose leftmost pixel is discovered after its
   rightmost one reported a too-narrow box (and `spix_draw_boxes()` drew it
   wrong). Min/max are now tracked separately; two regression fixtures cover
   it, including the scan-order case.

Cosmetic warnings from `-Wall -Wextra`: two `-Wmisleading-indentation` warnings
remain in `spix_grayscale()` and `spix_dither_floyd()` (untouched on purpose);
`-Wunused-variable` and `-Wshift-count-overflow` are gone with the fixes above.

### Known limitations (not changed)

- `SPIX_KERNEL_BOX_BLUR_3` has nine coefficients summing to 9 but uses
  `shift = 3` (÷8), so a flat grey 100 image comes out at 112 (+12.5%). This is
  the library's fixed-point normalisation choice, left as-is and pinned by the
  test. `SPIX_KERNEL_GAUSS_BLUR_3` (sum 16, shift 4) is exact.
- A **1:1 bilinear resize is not an identity**: the corner-aligned mapping
  `i * (src - 1) / dst` makes the far edge sample mostly the neighbour pixel
  (on 64×48 noise: max 229 LSB away from the source, PSNR 11.1 dB — while
  still matching the golden model exactly). Skip the call, or use
  `spix_resize_nn()`, when a copy is what you want.
- `spix_read_ppm_mem()` parses P5/P6 only; the ASCII P2/P3 forms are supported
  by `spix_read_ppm()` alone.
- `spix_histogram()`, `spix_equalize()`, `spix_threshold_otsu()` and
  `spix_find_components()` work on the **red** channel (they call it grayscale),
  so on colourful input their result is not a luminance segmentation.

Tests and benchmarks were added as part of the lost-tech-museum portfolio audit
follow-up for the `spix` record
(<https://kostyk348.github.io/lost-tech-museum/digests/portfolio-review/>).

## License

MIT — do whatever you want.
