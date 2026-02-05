#ifndef GIT_REPO_INTERNAL_H
#define GIT_REPO_INTERNAL_H

#include "../cupidgit.h"
#include <time.h>

// Internal repository structure
struct GitRepository {
    char *repo_root;        // Absolute path to repo root
    time_t index_mtime;     // .git/index modification time
    void *index_entries;    // Hash map (to be implemented)
    bool valid;             // Repository is valid
};

// Internal functions
char* find_git_directory(const char *path);
bool is_bare_repository(const char *git_dir);

#endif // GIT_REPO_INTERNAL_H
