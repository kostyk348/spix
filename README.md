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

## License

MIT — do whatever you want.
