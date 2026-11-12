# Draft repository format

New repositories contain a `format` file with the exact bytes
`ChunkVault draft-2\n` (LF), an `objects` directory, and a `snapshots` directory.
Objects contain raw chunk bytes. Each name is the lowercase hexadecimal
SHA-256 digest of those bytes.

Each published snapshot is named `<id>.cvm`. Its first eight bytes are
`CVDRAFT2`, followed by records in traversal order. All integers are
unsigned and little-endian.

| Field | Width in bytes |
| --- | --- |
| Record kind, ASCII `D` for directory or `F` for file | 1 |
| Relative path length `p` | 4 |
| UTF-8 relative path, slash separators and no NUL terminator | `p` |
| File size `n`, only for `F` | 8 |
| Chunk references, only for `F` | `36 * k` |

Each chunk reference is a four-byte length followed by its 32-byte SHA-256
digest. The reader consumes references until the lengths sum to `n`. Zero
lengths and lengths greater than the bytes still needed are invalid. The
maximum is 256 KiB, and a nonfinal chunk must contain at least 16 KiB. A file
of size zero has no references. A directory record occupies `5 + p` bytes.
A file record occupies `13 + p + 36 * k` bytes for `k` references.
Directory records precede their children. A path can appear only once.
The manifest ends with a one-byte ASCII `E` and no trailing bytes.

## Content-defined boundaries

The draft uses a Gear hash and two masks inspired by FastCDC. The minimum,
target and maximum sizes are 16 KiB, 64 KiB and 256 KiB. For each chunk, the
hash starts at zero, skips the first 16 KiB, then updates with
`hash = (hash << 1) + gear[byte]` modulo 2^64. Cut after the current byte
when `hash & mask` is zero. The mask is `0x1ffff` before zero-based offset
65,536 and `0x7fff` from that offset onward. If neither mask finds a boundary,
cut at 256 KiB or EOF, whichever comes first. The Gear table is the first
256 outputs of SplitMix64 with initial state `0x43564d414e494645`, in byte
value order. These draft choices may differ from the original specification.

## Legacy draft-1

The reader and writer still support repositories with the marker
`ChunkVault draft-1\n` and manifests headed by `CVDRAFT1`. That version
uses fixed 65,536-byte chunks except for the last chunk. References contain
only the 32-byte digest. A file record occupies
`13 + p + 32 * ceil(n / 65536)` bytes. Each repository uses its own format
version consistently.

## Publication and failure recovery

A backup stages each new object in the destination directory, flushes it with
`fsync` on Linux or `_commit` on Windows, closes it, and publishes it without
replacement. Before reusing an existing object, the backup checks its exact
size and SHA-256 digest. A conflicting object causes the backup to fail.

The same no-replace operation publishes a completed manifest after its
referenced objects. Linux uses `renameat2` with `RENAME_NOREPLACE`, with a
hard-link fallback for regular files where supported. Windows uses
`MoveFileExW` without replacement or cross-volume copying. A restore builds a
temporary sibling directory and uses native no-replace rename to publish it.

On Linux, ChunkVault synchronizes the object directory before publishing a
manifest and synchronizes the snapshot directory afterward. Restore
synchronizes the staged directory tree before renaming and synchronizes
the destination parent afterward. If synchronization fails after the rename,
the command reports an error and leaves the published entry visible.
Windows flushes file contents and uses same-volume no-replace publication.
Power-loss durability for Windows directory entries has not been verified.
Interrupted backups may leave unreachable objects.

References: [Linux renameat2](https://man7.org/linux/man-pages/man2/renameat2.2.html),
[Windows MoveFileExW](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-movefileexw),
and [NIST FIPS 180-4](https://csrc.nist.gov/pubs/fips/180-4/upd1/final).

[FastCDC paper](https://www.usenix.org/conference/atc16/technical-sessions/presentation/xia).

Backup holds a shared lock on the repository's persistent `.lock` file.
Recovery takes an exclusive lock and lists recognized temporary regular files.
With `--remove`, it removes those files while holding the lock. Published
manifests and chunks are outside recovery's scope.

[Linux fsync](https://man7.org/linux/man-pages/man2/fsync.2.html).
