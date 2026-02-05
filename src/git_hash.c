#define _XOPEN_SOURCE 700
#define _DEFAULT_SOURCE
#include "git_hash.h"
#include "sha1.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define MAX_FILE_SIZE (100 * 1024 * 1024)  // 100MB limit

bool git_hash_file(const char *filepath, unsigned char sha1_out[GIT_SHA1_SIZE]) {
    if (!filepath || !sha1_out) return false;

    // Get file size
    struct stat st;
    if (lstat(filepath, &st) != 0) {
        return false;
    }

    // Check if file is too large
    if (st.st_size > MAX_FILE_SIZE) {
        return false;
    }

    // Open file
    FILE *f = fopen(filepath, "rb");
    if (!f) return false;

    // Read file contents
    char *contents = malloc(st.st_size);
    if (!contents) {
        fclose(f);
        return false;
    }

    size_t read_bytes = fread(contents, 1, st.st_size, f);
    fclose(f);

    if (read_bytes != (size_t)st.st_size) {
        free(contents);
        return false;
    }

    // Compute git object hash: sha1("blob " + size + "\0" + contents)
    SHA1_CTX ctx;
    SHA1_Init(&ctx);

    char header[64];
    int header_len = snprintf(header, sizeof(header), "blob %ld", (long)st.st_size);

    SHA1_Update(&ctx, header, header_len);
    SHA1_Update(&ctx, "\0", 1);
    SHA1_Update(&ctx, contents, st.st_size);

    SHA1_Final(sha1_out, &ctx);

    free(contents);
    return true;
}

bool git_hash_symlink(const char *filepath, unsigned char sha1_out[GIT_SHA1_SIZE]) {
    if (!filepath || !sha1_out) return false;

    char target[PATH_MAX];
    ssize_t len = readlink(filepath, target, sizeof(target) - 1);
    if (len < 0) return false;

    target[len] = '\0';

    // Compute git object hash for symlink target
    SHA1_CTX ctx;
    SHA1_Init(&ctx);

    char header[64];
    int header_len = snprintf(header, sizeof(header), "blob %ld", (long)len);

    SHA1_Update(&ctx, header, header_len);
    SHA1_Update(&ctx, "\0", 1);
    SHA1_Update(&ctx, target, len);

    SHA1_Final(sha1_out, &ctx);

    return true;
}
