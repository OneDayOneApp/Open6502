#ifndef GEM6502_RENDER_H
#define GEM6502_RENDER_H

#include <stdint.h>

#include "gem6502.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GEM_RENDER_WIDTH  256
#define GEM_RENDER_HEIGHT 240

/*
 * Render the current PPU state into an RGB24, row-major framebuffer.
 *
 * The NES image is 256x240 pixels. If width/height are larger, the NES image
 * is placed in the top-left corner and the remaining pixels are set to black.
 * If width/height are smaller, the image is cropped to the top-left corner.
 * No scaling is performed.
 *
 * Interprets PPU ctrl/mask bits for background and sprite enable and uses the
 * compact NTSC 2C02 palette. Rendering is a pure function of the Gem state:
 * it does not mutate the PPU (including the sprite-0 hit flag).
 *
 * gem may be NULL, rgb may be NULL, and width/height may be non-positive; in
 * those cases the function is a no-op.
 */
void gem_render_frame(const Gem *gem, uint8_t *rgb, int width, int height);

#ifdef __cplusplus
}
#endif

#endif /* GEM6502_RENDER_H */
