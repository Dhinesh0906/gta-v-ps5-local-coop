/*
 * New isolated split compositor. The PS5 display bit-address equations are
 * adapted from SDL_ps5tilemap.c by John Tornblom, also used by this project's
 * diagnostic decoder. This is an altered, independent implementation, not
 * the SDL framebuffer driver. No display handle or GTA buffer is modified.
 * https://github.com/ps5-payload-dev/SDL/blob/release-2.30.x-ps5/src/video/ps5/SDL_ps5tilemap.c
 *
 * Simple DirectMedia Layer
 * Copyright (C) 2026 John Törnblom <john.tornblom@gmail.com>
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */
#include "coop-split-compositor.h"

typedef uint32_t pixel_quad __attribute__((vector_size(16), aligned(16), may_alias));
typedef uint32_t pixel_block __attribute__((vector_size(64), aligned(16), may_alias));

static int valid_surface(const struct coop_split_surface *s) {
    uintptr_t p;
    if (!s || !s->pixels || s->bytes != COOP_SPLIT_BYTES ||
        s->width != COOP_SPLIT_WIDTH || s->height != COOP_SPLIT_HEIGHT ||
        s->storage_rows != COOP_SPLIT_STORAGE_ROWS ||
        s->format != COOP_SPLIT_FORMAT_RGB10A2) return 0;
    p = (uintptr_t)s->pixels;
    return !(p & 15u) && p <= UINTPTR_MAX - s->bytes;
}

static int range_overlap(uintptr_t first, size_t first_bytes,
                         uintptr_t second, size_t second_bytes) {
    return first < second + second_bytes && second < first + first_bytes;
}

static int overlap(const struct coop_split_surface *a, const struct coop_split_surface *b) {
    return range_overlap((uintptr_t)a->pixels, a->bytes,
                         (uintptr_t)b->pixels, b->bytes);
}

/* The lowest two address bits are x[0:1], so four consecutive horizontal
 * pixels form one naturally aligned 16-byte group. x/y contributions are
 * XORed inside a 512x128 tile, then added to its non-overlapping tile base.
 */
static uint32_t x_word(uint32_t x) {
    uint32_t within = (x & 3u) | ((x & 28u) << 3) | ((x & 64u) << 2) |
        ((x & 32u) << 4) | ((x & 16u) << 7) | ((x & 448u) << 7);
    return (x >> 9) * 65536u + within;
}

static uint32_t y_word(uint32_t y) {
    return ((y & 7u) << 2) | ((y & 56u) << 3) |
        ((y & 64u) << 3) | ((y & 8u) << 7) | ((y & 64u) << 6);
}

enum coop_split_result coop_split_compose_workspace_age(
    const struct coop_split_capture captures[2],
    const struct coop_split_surface *destination,
    uint64_t current_scene_frame, uint32_t max_age,
    uint32_t *workspace) {
    const size_t workspace_bytes = (COOP_SPLIT_WIDTH / 4u) * sizeof(uint32_t);
    uintptr_t workspace_address = (uintptr_t)workspace;
    if (!max_age || max_age>16u || !captures || !workspace || (workspace_address & 3u) ||
        workspace_address > UINTPTR_MAX - workspace_bytes ||
        !valid_surface(destination) ||
        !valid_surface(&captures[0].surface) || !valid_surface(&captures[1].surface))
        return COOP_SPLIT_BAD_LAYOUT;
    for (uint32_t player = 0; player < 2; player++) {
        const struct coop_split_capture *capture = &captures[player];
        if (capture->player != player + 1u || !capture->camera_identity ||
            capture->flags != (COOP_CAPTURE_OWNED | COOP_CAPTURE_COMPLETE))
            return COOP_SPLIT_BAD_CAPTURE;
        if (!capture->scene_frame || capture->scene_frame > current_scene_frame ||
            current_scene_frame - capture->scene_frame > max_age)
            return COOP_SPLIT_STALE_CAPTURE;
    }
    if (captures[0].camera_identity == captures[1].camera_identity)
        return COOP_SPLIT_BAD_CAPTURE;
    if (overlap(destination, &captures[0].surface) ||
        overlap(destination, &captures[1].surface) ||
        overlap(&captures[0].surface, &captures[1].surface))
        return COOP_SPLIT_ALIASED_MEMORY;
    if (range_overlap(workspace_address, workspace_bytes,
                      (uintptr_t)destination->pixels, destination->bytes) ||
        range_overlap(workspace_address, workspace_bytes,
                      (uintptr_t)captures[0].surface.pixels, captures[0].surface.bytes) ||
        range_overlap(workspace_address, workspace_bytes,
                      (uintptr_t)captures[1].surface.pixels, captures[1].surface.bytes))
        return COOP_SPLIT_ALIASED_MEMORY;

    for (uint32_t group = 0; group < COOP_SPLIT_WIDTH / 4u; group++)
        workspace[group] = x_word(group * 4u);
    for (uint32_t player = 0; player < 2; player++) {
        const uint32_t *source = captures[player].surface.pixels;
        /* Both crop boundaries and viewport origins are multiples of four.
         * x[0:1] and y[0:1] occupy the low four word-address bits: a 4x4
         * pixel block is contiguous in both layouts despite the row shift.
         * Copy it once instead of visiting those four rows separately. */
        for (uint32_t row = 0; row < COOP_SPLIT_HEIGHT / 2u; row+=4u) {
            uint32_t sy = row + COOP_SPLIT_HEIGHT / 4u;
            uint32_t dy = row + player * (COOP_SPLIT_HEIGHT / 2u);
            uint32_t source_base = (sy >> 7) * (128u * COOP_SPLIT_WIDTH);
            uint32_t dest_base = (dy >> 7) * (128u * COOP_SPLIT_WIDTH);
            uint32_t source_y = y_word(sy), dest_y = y_word(dy);
            for (uint32_t group = 0; group < COOP_SPLIT_WIDTH / 4u; group++) {
                const pixel_block *from = (const pixel_block *)(source + source_base + (workspace[group] ^ source_y));
                pixel_block *to = (pixel_block *)(destination->pixels + dest_base + (workspace[group] ^ dest_y));
                *to = *from;
            }
        }
    }
    return COOP_SPLIT_OK;
}

enum coop_split_result coop_split_compose_workspace(
    const struct coop_split_capture captures[2],
    const struct coop_split_surface *destination,
    uint64_t current_scene_frame, uint32_t *workspace) {
    return coop_split_compose_workspace_age(captures,destination,current_scene_frame,2u,workspace);
}

enum coop_split_result coop_split_compose(
    const struct coop_split_capture captures[2],
    const struct coop_split_surface *destination,
    uint64_t current_scene_frame) {
    uint32_t workspace[COOP_SPLIT_WIDTH / 4u];
    return coop_split_compose_workspace(captures, destination,
                                        current_scene_frame, workspace);
}
