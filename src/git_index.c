#define _XOPEN_SOURCE 700
#define _DEFAULT_SOURCE
#include "git_index.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

// Read 32-bit network byte order integer
static uint32_t read_uint32(const unsigned char *data) {
    return ntohl(*(uint32_t*)data);
}

void git_index_free(IndexMap *map) {
    if (!map) return;

    for (size_t i = 0; i < map->count; i++) {
        free(map->entries[i].path);
    }
    free(map->entries);
    free(map);
}

GitIndexEntry* git_index_lookup(IndexMap *map, const char *path) {
    if (!map || !path) return NULL;

    // Linear search (simple implementation)
    for (size_t i = 0; i < map->count; i++) {
        if (strcmp(map->entries[i].path, path) == 0) {
            return &map->entries[i];
        }
    }

    return NULL;
}

bool git_index_parse(GitRepository *repo) {
    if (!repo || !repo->repo_root) return false;

    // Construct path to .git/index
    char index_path[PATH_MAX];
    snprintf(index_path, sizeof(index_path), "%s/.git/index", repo->repo_root);

    // Open index file
    int fd = open(index_path, O_RDONLY);
    if (fd < 0) {
        return false;
    }

    // Get file size
    struct stat st;
    if (fstat(fd, &st) != 0) {
        close(fd);
        return false;
    }

    // Store index mtime for cache invalidation
    repo->index_mtime = st.st_mtime;

    // mmap the index file
    unsigned char *data = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);

    if (data == MAP_FAILED) {
        return false;
    }

    // Verify signature "DIRC"
    if (st.st_size < 12 || memcmp(data, "DIRC", 4) != 0) {
        munmap(data, st.st_size);
        return false;
    }

    // Read version and entry count
    uint32_t version = read_uint32(data + 4);
    uint32_t entry_count = read_uint32(data + 8);

    // Support version 2, 3, 4
    if (version < 2 || version > 4) {
        munmap(data, st.st_size);
        return false;
    }

    // Sanity check entry count
    if (entry_count > 1000000) {
        munmap(data, st.st_size);
        return false;
    }

    // Allocate index map
    IndexMap *map = calloc(1, sizeof(IndexMap));
    if (!map) {
        munmap(data, st.st_size);
        return false;
    }

    map->entries = calloc(entry_count, sizeof(GitIndexEntry));
    if (!map->entries) {
        free(map);
        munmap(data, st.st_size);
        return false;
    }

    map->capacity = entry_count;

    // Parse entries
    const unsigned char *ptr = data + 12;
    const unsigned char *end = data + st.st_size - 20;  // Exclude checksum

    for (uint32_t i = 0; i < entry_count && ptr < end; i++) {
        // Check we have at least 62 bytes for fixed fields
        if (ptr + 62 > end) break;

        GitIndexEntry *entry = &map->entries[map->count];

        // Skip ctime (8 bytes)
        ptr += 8;

        // Read mtime (8 bytes)
        entry->mtime_sec = read_uint32(ptr);
        entry->mtime_nsec = read_uint32(ptr + 4);
        ptr += 8;

        // Skip dev, ino, mode, uid, gid, file_size (24 bytes)
        ptr += 24;

        // Read SHA-1 (20 bytes)
        memcpy(entry->sha1, ptr, GIT_SHA1_SIZE);
        ptr += GIT_SHA1_SIZE;

        // Read flags (2 bytes)
        uint16_t flags = (ptr[0] << 8) | ptr[1];
        ptr += 2;

        // Extract path length from flags (lower 12 bits)
        uint16_t path_len = flags & 0x0FFF;

        // Read path (variable length, null-terminated)
        if (ptr + path_len > end) break;

        entry->path = strndup((char*)ptr, path_len);
        if (!entry->path) break;

        if (i == 0) {
            fprintf(stderr, "[DEBUG INDEX] First entry: flags=0x%04x, path_len=%u, path='%s'\n",
                    flags, path_len, entry->path);
        }

        ptr += path_len;

        // Skip null terminator
        if (ptr < end && *ptr == '\0') ptr++;

        // Align to 8-byte boundary
        size_t entry_size = 62 + path_len + 1;
        size_t padding = (8 - (entry_size % 8)) % 8;
        ptr += padding;

        map->count++;
    }

    munmap(data, st.st_size);

    // Store index map in repository
    if (repo->index_entries) {
        git_index_free(repo->index_entries);
    }
    repo->index_entries = map;

    return true;
}
