#define _XOPEN_SOURCE 700
#define _DEFAULT_SOURCE
#include "git_repo.h"
#include "git_index.h"
#include "git_status.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

// Find .git directory by walking up from path
char* find_git_directory(const char *path) {
    char current[PATH_MAX];
    char git_path[PATH_MAX];
    struct stat st;

    // Get absolute path
    if (!realpath(path, current)) {
        return NULL;
    }

    // Walk up directory tree
    while (1) {
        // Check for .git in current directory
        snprintf(git_path, sizeof(git_path), "%s/.git", current);

        if (stat(git_path, &st) == 0 && S_ISDIR(st.st_mode)) {
            return strdup(current);  // Found repo root
        }

        // Check if we've reached root
        if (strcmp(current, "/") == 0) {
            break;
        }

        // Move to parent directory
        char *last_slash = strrchr(current, '/');
        if (!last_slash) {
            break;
        }

        if (last_slash == current) {
            strcpy(current, "/");
        } else {
            *last_slash = '\0';
        }
    }

    return NULL;  // No .git found
}

// Check if directory is a bare repository
bool is_bare_repository(const char *git_dir) {
    char path[PATH_MAX];
    struct stat st;

    // Bare repos have HEAD, objects, refs in the directory itself
    snprintf(path, sizeof(path), "%s/HEAD", git_dir);
    if (stat(path, &st) != 0) return false;

    snprintf(path, sizeof(path), "%s/objects", git_dir);
    if (stat(path, &st) != 0 || !S_ISDIR(st.st_mode)) return false;

    snprintf(path, sizeof(path), "%s/refs", git_dir);
    if (stat(path, &st) != 0 || !S_ISDIR(st.st_mode)) return false;

    return true;
}

GitRepository* git_repo_open(const char *path) {
    if (!path) return NULL;

    char *repo_root = find_git_directory(path);
    if (!repo_root) {
        return NULL;  // Not a git repository
    }

    // If we found .git directory, it's a normal repository (not bare)
    // Bare repos don't have .git subdirectory, so no need to check

    // Allocate repository structure
    GitRepository *repo = calloc(1, sizeof(GitRepository));
    if (!repo) {
        free(repo_root);
        return NULL;
    }

    repo->repo_root = repo_root;
    repo->valid = true;
    repo->index_mtime = 0;
    repo->index_entries = NULL;

    // Parse git index
    if (!git_index_parse(repo)) {
        git_repo_free(repo);
        return NULL;
    }

    return repo;
}

void git_repo_free(GitRepository *repo) {
    if (!repo) return;

    free(repo->repo_root);
    if (repo->index_entries) {
        git_index_free(repo->index_entries);
    }
    free(repo);
}

bool git_repo_is_valid(GitRepository *repo) {
    return repo && repo->valid;
}

const char* git_repo_root(GitRepository *repo) {
    if (!repo || !repo->valid) return NULL;
    return repo->repo_root;
}

GitStatus git_file_status(GitRepository *repo, const char *path) {
    return git_calculate_status(repo, path);
}

const char* git_status_emoji(GitStatus status) {
    switch (status) {
        case GIT_STATUS_MODIFIED:   return "🔴";
        case GIT_STATUS_UNTRACKED:  return "🟢";
        case GIT_STATUS_UNMODIFIED: return "⚪";
        case GIT_STATUS_ERROR:      return "";
        default:                    return "";
    }
}
