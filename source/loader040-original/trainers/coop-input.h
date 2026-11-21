#pragma once

/* The platform adapter supplies only the secondary pad to this state. On a
 * disconnect, pause or session entry, require neutral before accepting input.
 * A held X must never become a fresh jump/start when a controller reconnects. */
typedef struct {
    unsigned held, pressed, released, armed;
    float x, y;
} CoopInput;

static float coop_abs(float n) { return n < 0 ? -n : n; }
static float coop_axis(unsigned byte) {
    int n = (int)byte - 128;
    if (n >= -24 && n <= 24) return 0;
    return n < 0 ? (n + 24) / 104.0f : (n - 24) / 103.0f;
}
static void coop_input_update(CoopInput *s, const unsigned char *data,
                              unsigned readable, unsigned blocked) {
    unsigned raw = 0;
    float x = 0, y = 0, rx = 0, ry = 0;
    if (readable && data[0x4c]) {
        for (unsigned i = 0; i < 4; i++) raw |= (unsigned)data[i] << (i * 8);
        x = coop_axis(data[4]); y = -coop_axis(data[5]);
        rx = coop_axis(data[6]); ry = coop_axis(data[7]);
    } else blocked = 1;
    unsigned before = s->held;
    s->held = s->pressed = s->released = 0;
    s->x = s->y = 0;
    if (blocked) { s->armed = 0; return; }
    if (!s->armed) {
        if (!raw && x == 0 && y == 0 && rx == 0 && ry == 0 && data[8]<=24 && data[9]<=24) s->armed = 1;
        return;
    }
    s->held = raw;
    s->pressed = raw & ~before;
    s->released = before & ~raw;
    /* Bound diagonals without a freestanding math-library dependency. */
    float squared = x*x + y*y;
    if (squared > 1) {
        float length = squared;
        for (unsigned i = 0; i < 5; i++) length = (length + squared/length)*0.5f;
        x /= length; y /= length;
    }
    s->x = x; s->y = y;
}

/* Never accept the primary user's controller as a secondary one. */
static int coop_secondary_user(const int users[4], int primary) {
    if (primary <= 0) return -1;
    for (unsigned i = 0; i < 4; i++)
        if (users[i] > 0 && users[i] != primary) return users[i];
    return -1;
}

static unsigned coop_position_valid(const float p[3]) {
    return p[0] == p[0] && p[1] == p[1] && p[2] == p[2] &&
        coop_abs(p[0]) < 15000 && coop_abs(p[1]) < 15000 &&
        p[2] > -100 && p[2] < 2500;
}
static float coop_distance_squared(const float a[3], const float b[3]) {
    float x=a[0]-b[0], y=a[1]-b[1], z=a[2]-b[2];
    return x*x+y*y+z*z;
}
