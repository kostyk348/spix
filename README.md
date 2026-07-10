# spix — Single-header C99 image processing library

Fixed-point arithmetic throughout. No FPU required. Zero dependencies beyond libc.

```
spix_image_t *img = spix_read_ppm("input.ppm");
spix_image_t *blurred = spix_convolve(img, &SPIX_KERNEL_GAUSS_BLUR_3);
spix_write_ppm(blurred, "blurred.ppm");
```

## Features

| Operation | Description |
|-----------|-------------|
| **Resize** | Nearest-neighbor, bilinear (Q16.16 fixed-point) |
| **Convolution** | 3×3 kernels: box blur, gaussian blur, sharpen, edge detect, emboss, sobel |
| **Grayscale** | Luminosity (0.299R + 0.587G + 0.114B, fixed-point) |
| **Threshold** | Global + Otsu adaptive |
| **Dithering** | Floyd-Steinberg error diffusion, Bayer 4×4 ordered |
| **Geometry** | Horizontal/vertical flip, 90° rotation |
| **IO** | PPM/PGM read/write (P2/P3/P5/P6) |

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
#include "spix.h"

int main(void) {
    spix_image_t *img = spix_read_ppm("photo.ppm");
    spix_grayscale(img);
    spix_dither_floyd(img);
    spix_write_ppm(img, "dithered.ppm");
    spix_free(img);
    return 0;
}
```

## Predefined kernels

| Name | Effect |
|------|--------|
| `SPIX_KERNEL_BOX_BLUR_3` | 3×3 uniform blur (average) |
| `SPIX_KERNEL_GAUSS_BLUR_3` | 3×3 gaussian blur |
| `SPIX_KERNEL_SHARPEN` | Edge emphasis |
| `SPIX_KERNEL_EDGE_DETECT` | Laplacian edge detection |
| `SPIX_KERNEL_EMBOSS` | Emboss relief effect |
| `SPIX_KERNEL_SOBEL_H` | Horizontal Sobel operator |
| `SPIX_KERNEL_SOBEL_V` | Vertical Sobel operator |

## License

MIT — do whatever you want.
