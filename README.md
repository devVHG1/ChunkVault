# ChunkVault

ChunkVault is a C17 command-line tool for deduplicated local backups on Windows
and Linux. New repositories use the `draft-2` prototype format. Repositories
created by this version may require migration after the source specification is
reconciled. Existing `draft-1` repositories remain readable and writable.

## Build and test

Use CMake 3.20 or later and a C17 compiler.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
cmake --install build --config Release --prefix stage
```

With the default Visual Studio generator, the executable is
`build\Release\cv.exe`. On Linux, it is `build/cv`.

## Create and restore a snapshot

Run these commands with the built executable available as `cv`. The
repository and restore destination must be new directories. Put the repository
outside the source tree.

```sh
cv init vault
cv backup vault source first
cv list vault
cv verify vault first
cv restore vault first restored
```

Omit the snapshot ID to generate one. The snapshot records regular files and
directories, including empty ones. Relative names must be valid UTF-8,
fit the draft path limits, and be valid on Windows. Links,
special files, permissions, and timestamps are outside the draft format.
Keep the source tree stable while taking a snapshot. Backup checks each file's
size and modification metadata during the read and assembles the snapshot file
by file. Case-distinct Linux paths can conflict on a case-insensitive restore volume.

The verifier checks the manifest syntax and the referenced object contents.
Run `cv verify vault` to check every published snapshot.
The restore command validates data before publishing the completed directory
without overwriting the destination. Directory publication requires native
no-replace rename support on Linux. Interrupted writes can leave temporary
files and unreferenced chunks. Use `cv recover vault` to list abandoned
repository temporary files and `cv recover vault --remove` to delete them.
Recovery excludes active backups with a repository lock. Unreferenced
published chunks and temporary restore directories are outside its scope.

See the [draft format](docs/format.md) for the byte layout.

## Local benchmark

Use Python 3.9 or later to reproduce the benchmark with your built executable.

```sh
python tools/benchmark.py path/to/cv
```

In one Windows run, the first snapshot of a 16 MiB file stored 16 MiB of
objects. The second snapshot, after one byte was inserted at the start, added
41,532 bytes of objects. The first backup took 0.632 s, the second took
0.236 s, and verification plus restoration took 0.226 s. These timings
include command startup and apply to that machine and workload.
