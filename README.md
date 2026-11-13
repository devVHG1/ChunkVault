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

The list command prints snapshot IDs in lexical order.
The verifier checks the manifest syntax and the referenced object contents.
Run `cv verify vault` to check every published snapshot.
The restore command validates data before publishing the completed directory
without overwriting the destination. Directory publication requires native
no-replace rename support on Linux. In WSL, use the Linux filesystem for
the restore destination. Mounted Windows drives (such as /mnt/c) may lack
the required directory rename support. Interrupted writes can leave temporary
files and unreferenced chunks. Use `cv recover vault` to list abandoned
repository temporary files and `cv recover vault --remove` to delete them.
Recovery excludes active backups with a repository lock. Unreferenced
published chunks and temporary restore directories are outside its scope.

See the [draft format](docs/format.md) for the byte layout.

## Demo: two snapshots and a byte-for-byte restore

Run the demo with Python 3.9 or later and a built executable.

```powershell
python tools/benchmark.py build/Release/cv.exe
```

```sh
python3 tools/benchmark.py build/cv
```

The script creates a temporary vault and backs up a 16 MiB file. It inserts
one byte at the beginning of that file and takes a second snapshot. Then it
runs `cv verify`, restores the second snapshot, and compares the restored
file byte for byte with the modified input.

| Measurement | Example Windows run |
| --- | ---: |
| First snapshot chunk data | 16,777,216 bytes |
| New chunk data for the second snapshot | 41,532 bytes |
| Second snapshot manifest | 7,268 bytes |
| Restored file | Byte-for-byte match |

The second snapshot added 48,800 bytes in chunk and manifest contents.
Filesystem allocation overhead is excluded. Content-defined chunking found
matching chunks again after the insertion shifted the rest of the file.

## Reliability and limits

CTest exercises interrupted backup at six points, malformed manifests, and
concurrent writes. GitHub Actions builds and tests Debug and Release on
Windows with MSVC and on Ubuntu with GCC. See the
[CI results](https://github.com/devVHG1/ChunkVault/actions).

The on-disk format is a draft. The backup preserves file contents and
directory structure. Permissions and timestamps are outside its scope.
The interruption tests simulate process termination. Power-loss behavior
has yet to be validated.
