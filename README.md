# cupidgit

Minimal, read-only git library for status detection in cupidfm.

## Features

- Repository detection (find .git directory)
- Git index parsing (.git/index)
- File status calculation (modified/untracked/unmodified)
- SHA-1 hashing for content comparison

## API

See `cupidgit.h` for the public API.

## Building

```bash
make
```

Produces `libcupidgit.a` static library.

## Dependencies

- OpenSSL (for SHA-1 hashing)
