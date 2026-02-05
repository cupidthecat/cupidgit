#define _XOPEN_SOURCE 700
#define _DEFAULT_SOURCE
#include "git_status.h"
#include "git_index.h"
#include "git_hash.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <limits.h>
#include <stdlib.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

// Make path relative to repository root
static char* make_relative_path(const char *repo_root, const char *path) {
    char abs_path[PATH_MAX];
    char abs_repo[PATH_MAX];

    if (!realpath(path, abs_path)) return NULL;
    if (!realpath(repo_root, abs_repo)) return NULL;

    size_t repo_len = strlen(abs_repo);

    // Check if path is within repository
    if (strncmp(abs_path, abs_repo, repo_len) != 0) {
        return NULL;
    }

    // Skip repo root and leading slash
    const char *rel = abs_path + repo_len;
    if (*rel == '/') rel++;

    return strdup(rel);
}

GitStatus git_calculate_status(GitRepository *repo, const char *path) {
    if (!repo || !repo->valid || !path) {
        return GIT_STATUS_ERROR;
    }

    IndexMap *map = (IndexMap*)repo->index_entries;
    if (!map) {
        return GIT_STATUS_ERROR;
    }

    // Convert to relative path
    char *rel_path = make_relative_path(repo->repo_root, path);
    if (!rel_path) {
        return GIT_STATUS_ERROR;
    }

    // Lookup in index
    GitIndexEntry *entry = git_index_lookup(map, rel_path);
    if (!entry) {
        free(rel_path);
        return GIT_STATUS_UNTRACKED;  // Not in index
    }

    free(rel_path);

    // Get file stats
    struct stat st;
    if (lstat(path, &st) != 0) {
        return GIT_STATUS_ERROR;
    }

    // Fast path: Check mtime
    if (st.st_mtime == entry->mtime_sec) {
        // Mtime matches, likely unmodified
        // Could also check nsec but most filesystems don't support it
        return GIT_STATUS_UNMODIFIED;
    }

    // Slow path: Compute SHA-1 hash
    unsigned char file_sha1[GIT_SHA1_SIZE];
    bool hash_ok;

    if (S_ISLNK(st.st_mode)) {
        hash_ok = git_hash_symlink(path, file_sha1);
    } else {
        hash_ok = git_hash_file(path, file_sha1);
    }

    if (!hash_ok) {
        // Could not hash (too large, permission error, etc.)
        // Conservatively mark as modified
        return GIT_STATUS_MODIFIED;
    }

    // Compare hashes
    if (memcmp(file_sha1, entry->sha1, GIT_SHA1_SIZE) == 0) {
        return GIT_STATUS_UNMODIFIED;  // Content unchanged
    }

    return GIT_STATUS_MODIFIED;  // Content differs
}
