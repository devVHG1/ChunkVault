"""Benchmark deduplication after inserting a byte at the start of a file."""

import argparse
from pathlib import Path
import random
import subprocess
import tempfile
import time


def run(executable, *arguments):
    started = time.perf_counter()
    result = subprocess.run(
        [executable, *arguments], capture_output=True, text=True, check=False
    )
    if result.returncode:
        raise RuntimeError(result.stderr.strip() or f"cv exited {result.returncode}")
    return time.perf_counter() - started


def object_bytes(repository):
    return sum(item.stat().st_size for item in (repository / "objects").iterdir())


def benchmark(executable, size):
    with tempfile.TemporaryDirectory(prefix="chunkvault-bench-") as workspace:
        root = Path(workspace)
        source = root / "input"
        source.mkdir()
        repository = root / "vault"
        restored = root / "restored"
        input_file = source / "sample.bin"
        original = random.Random(181).randbytes(size)
        input_file.write_bytes(original)

        run(executable, "init", repository)
        first = run(executable, "backup", repository, source, "first")
        initial_bytes = object_bytes(repository)
        modified = b"X" + original
        input_file.write_bytes(modified)
        second = run(executable, "backup", repository, source, "shifted")
        added_bytes = object_bytes(repository) - initial_bytes
        restore = run(executable, "restore", repository, "shifted", restored)
        if (restored / input_file.name).read_bytes() != modified:
            raise RuntimeError("restored bytes differ from the input")

        print(f"Input size              {size:,} bytes")
        print(f"First backup            {first:.3f} s")
        print(f"First snapshot objects  {initial_bytes:,} bytes")
        print(f"Second backup           {second:.3f} s")
        print(f"Second snapshot adds    {added_bytes:,} bytes")
        print(f"Restore                 {restore:.3f} s")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path, help="path to cv or cv.exe")
    parser.add_argument("--mib", type=int, default=16, help="input size in MiB")
    options = parser.parse_args()
    if not 1 <= options.mib <= 128:
        parser.error("--mib must be between 1 and 128")
    try:
        benchmark(options.executable.resolve(strict=True), options.mib * 1024 * 1024)
    except (OSError, RuntimeError) as error:
        parser.exit(1, f"{error}\n")


if __name__ == "__main__":
    main()
