# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

Entries are derived from `git log` only — every line below refers to a commit that
exists in this repository. Where a version's history was thin (a handful of
`cleanup`/`test` commits and nothing else), the entry says so instead of inventing
feature text.

## [Unreleased]

Nothing since v0.1.0.

## [0.1.0] — 2026-10-14

Tag `v0.1.0` points at `c1f0d43` (2026-10-14); the GitHub release was published 2026-10-16.

### Added

- **Core library** (`spix.h`, single header): PPM/PGM read (P2/P3/P5/P6) and write (P6),
  24-bit uncompressed BMP writer, nearest-neighbour and Q16.16 fixed-point bilinear resize,
  3×3 convolution with box/gaussian/sharpen/edge/emboss/sobel kernels, grayscale, threshold,
  Otsu, histogram equalization, 3×3 morphology (erode, dilate, open, close, top-hat),
  union-find connected components with bounding boxes and centroids, image arithmetic
  (add, sub, blend, invert, brighten, contrast), Floyd–Steinberg and Bayer 4×4 dithering,
  horizontal/vertical flip, 90° rotation, per-channel pixel access (`e5072f5`, `ba9d7d9`).
- **Demo pipeline** (`examples/demo.c`): generates a test pattern and runs it through the
  filters (`make demo`, `make generate`).
- **Correctness suite** (`tests/test_spix.c`): dependency-free assert harness with a
  deliberate golden model — an independent double-precision bilinear resampler using the
  same documented Q16.16 mapping — plus a throughput benchmark (`bench/bench_spix.c`)
  and `make` targets `test` / `bench` (`c1f0d43`).
- **CI**: GitHub Actions workflow running `make` and `make test` on push and pull request
  (`394546d`, `84be243`).

### Fixed

- The include guard was unterminated (`#ifndef SPIX_H` without `#endif`), so
  `cc -std=c99` could not compile the header at all (`c1f0d43`).
- `spix_read_ppm()` lost one byte per read: `_spix_read_int()` consumed the delimiter
  ending `maxval` and the reader consumed one more, shifting P5/P6 pixels or eating the
  first digit of P2/P3; small P5 files returned `NULL` (`c1f0d43`).
- `spix_read_ppm_mem()` ignored `maxval` and returned 15×-too-bright P6 images (`c1f0d43`).
- `spix_resize_bilinear()` overflowed 32-bit `int` and shifted by 32 (UB); the accumulator
  is now `int64_t` and the operator is bit-exact against the golden model (`c1f0d43`).
- `spix_convolve_to()` subtracted 128 unconditionally, so level-preserving kernels (box,
  gaussian, sharpen, emboss) darkened their output (`c1f0d43`).
- `spix_threshold_otsu()` was off by one against `spix_threshold()`, so the documented pair
  `spix_threshold(img, spix_threshold_otsu(img))` marked the whole dark class white (`c1f0d43`).
- `spix_find_components()` bounding boxes depended on scan order
  (`c1f0d43`).

### Known limitations at v0.1.0

- `SPIX_KERNEL_BOX_BLUR_3` has nine coefficients summing to 9 but `shift = 3` (÷8), so a
  flat grey 100 comes out at 112 — pinned by the tests, left as the library's
  fixed-point normalisation choice.
- A 1:1 bilinear resize is not the identity (corner-aligned mapping), while still matching
  the golden model exactly.
- `spix_read_ppm_mem()` parses P5/P6 only.
- `spix_histogram()`, `spix_equalize()`, `spix_threshold_otsu()` and
  `spix_find_components()` work on the red channel.
- The "no FPU" claim was not verified by disassembly or on embedded hardware; `spix_blend`,
  `spix_contrast` and `spix_threshold_otsu` use `float`/`double` internally.

### History note

Between `e5072f5` (initial release) and `d80e3b6` the log contains only `cleanup` and
`test`/`test file creation` commits (`98345c0`, `2df26da`, `2dc7dcc`, `e4e3ab5`, `2b45be2`,
`f5d0b90`, `bd31056`, `5097161`). They were squashed into the two entries above rather than
guessed at.

[Unreleased]: https://github.com/kostyk348/spix/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/kostyk348/spix/releases/tag/v0.1.0
