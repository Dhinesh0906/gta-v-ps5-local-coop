/* CPU-only composition into caller-owned memory. No GPU or game calls.
 * The caller must supply two completed, immutable, owned camera captures.
 * This interface does not obtain a GPU fence or capture a GTA camera itself.
 */
#ifndef GTA_COOP_SPLIT_COMPOSITOR_H
#define GTA_COOP_SPLIT_COMPOSITOR_H
#include <stddef.h>
#include <stdint.h>

#define COOP_SPLIT_WIDTH 3840u
#define COOP_SPLIT_HEIGHT 2160u
#define COOP_SPLIT_STORAGE_ROWS 2176u
#define COOP_SPLIT_BYTES ((size_t)COOP_SPLIT_WIDTH * COOP_SPLIT_STORAGE_ROWS * 4u)
#define COOP_SPLIT_FORMAT_RGB10A2 24u
#define COOP_CAPTURE_OWNED 1u
#define COOP_CAPTURE_COMPLETE 2u

struct coop_split_surface {
    uint32_t *pixels;
    size_t bytes;
    uint32_t width, height, storage_rows, format;
};

struct coop_split_capture {
    struct coop_split_surface surface;
    uint64_t camera_identity;
    uint64_t scene_frame;
    uint32_t player;
    uint32_t flags;
};

enum coop_split_result {
    COOP_SPLIT_OK = 0,
    COOP_SPLIT_BAD_LAYOUT = 1,
    COOP_SPLIT_BAD_CAPTURE = 2,
    COOP_SPLIT_STALE_CAPTURE = 3,
    COOP_SPLIT_ALIASED_MEMORY = 4
};

/* Writes only visible output pixels after all validation succeeds. Retains
 * the raw 10-bit HDR code values and alpha, without color conversion.
 * P1/P2 each contribute the central 3840x1080 region of their own frame.
 * This crops each independent view without vertically stretching it.
 * Invalid pairs leave the entire destination untouched.
 */
enum coop_split_result coop_split_compose(
    const struct coop_split_capture captures[2],
    const struct coop_split_surface *destination,
    uint64_t current_scene_frame);

/* Caller-supplied workspace avoids a 15 KiB stack allocation in render-thread
 * hooks. The workspace must hold COOP_SPLIT_WIDTH / 4 words, be 4-byte aligned,
 * and must not overlap any capture or destination surface.
 */
enum coop_split_result coop_split_compose_workspace(
    const struct coop_split_capture captures[2],
    const struct coop_split_surface *destination,
    uint64_t current_scene_frame,
    uint32_t *workspace);
/* Explicit bounded queue latency for the live captured-frame pipeline. */
enum coop_split_result coop_split_compose_workspace_age(
    const struct coop_split_capture captures[2],
    const struct coop_split_surface *destination,
    uint64_t current_scene_frame, uint32_t max_age,
    uint32_t *workspace);
#endif
