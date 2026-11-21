// One-shot PS5 diagnostic: replace a fixed Daemon texture slot, or restore it.
// Build separately with PATCH_RESTORE=0 or 1. Do not run while GTA is open.
#include <stdint.h>
#include <stddef.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include "ghost-surgical-bytes.h"

static const char archive_path[] =
    "/data/homebrew/GTA/update/ps5/dlcpacks/patchDay1NG/dlc.rpf";
static const char status_path[] =
    "/data/homebrew/GTA/ghost-surgical-test.status";
static uint8_t current[CHUNK_SIZE];
static uint8_t before[CHUNK_SIZE];

static uint64_t fnv64(const uint8_t *bytes, size_t length) {
    uint64_t value = UINT64_C(0xcbf29ce484222325);
    for (size_t i = 0; i < length; i++) {
        value ^= bytes[i];
        value *= UINT64_C(0x100000001b3);
    }
    return value;
}

static int read_all(int fd, uint8_t *bytes, size_t length, off_t offset) {
    size_t done = 0;
    while (done < length) {
        ssize_t n = pread(fd, bytes + done, length - done, offset + done);
        if (n <= 0) return -1;
        done += (size_t)n;
    }
    return 0;
}

static int write_all(int fd, const uint8_t *bytes, size_t length, off_t offset) {
    size_t done = 0;
    while (done < length) {
        ssize_t n = pwrite(fd, bytes + done, length - done, offset + done);
        if (n <= 0) return -1;
        done += (size_t)n;
    }
    return 0;
}

static void report(int code) {
    int fd = open(status_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) return;
    char line[64];
    int length = snprintf(line, sizeof(line), "%s stage=%d\n",
#if PATCH_RESTORE
                          "restore",
#else
                          "apply",
#endif
                          code);
    if (length > 0) write(fd, line, (size_t)length);
    fsync(fd);
    close(fd);
}

int main(void) {
    report(0);
    int fd = open(archive_path, O_RDWR);
    if (fd < 0) { report(1); return 1; }
    if (lseek(fd, 0, SEEK_END) != ARCHIVE_SIZE) {
        report(2); close(fd); return 2;
    }
    uint8_t header[4];
    // RPF7 is stored as a little-endian u32, so its disk bytes are "7FPR".
    if (read_all(fd, header, 4, 0) || header[0] != '7' || header[1] != 'F' ||
        header[2] != 'P' || header[3] != 'R') {
        report(3); close(fd); return 3;
    }
    if (read_all(fd, current, CHUNK_SIZE, CHUNK_OFFSET) ||
        fnv64(current, CHUNK_SIZE) != EXPECTED_HASH) {
        report(4); close(fd); return 4;
    }
    memcpy(before, current, CHUNK_SIZE);
    if (write_all(fd, desired, CHUNK_SIZE, CHUNK_OFFSET) || fsync(fd)) {
        write_all(fd, before, CHUNK_SIZE, CHUNK_OFFSET);
        fsync(fd);
        report(5); close(fd); return 5;
    }
    if (read_all(fd, current, CHUNK_SIZE, CHUNK_OFFSET) ||
        fnv64(current, CHUNK_SIZE) != DESIRED_HASH) {
        write_all(fd, before, CHUNK_SIZE, CHUNK_OFFSET);
        fsync(fd);
        report(6); close(fd); return 6;
    }
    close(fd);
    report(100);
    return 0;
}
