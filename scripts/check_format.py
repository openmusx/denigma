#!/usr/bin/env python3
"""Check (or fix) project-owned C++ formatting with the repository's .clang-format."""

import argparse
import re
import shutil
import subprocess
import sys
from pathlib import Path

# The exact release CI installs (`pip install clang-format==...`). Output differs between
# clang-format releases, so a local run must use the same major version to agree with CI.
CLANG_FORMAT_VERSION = "19.1.6"
SOURCE_ROOTS = ("include", "src", "tests")
SUFFIXES = (".cpp", ".h")


def project_sources(root: Path):
    """Project-owned C++ files. `.clang-format-ignore` excludes vendored and generated code."""
    for source_root in SOURCE_ROOTS:
        for path in sorted((root / source_root).rglob("*")):
            if path.suffix in SUFFIXES and path.is_file():
                yield path.relative_to(root)


def check_version(clang_format: str) -> str | None:
    output = subprocess.run([clang_format, "--version"], capture_output=True, text=True, check=True).stdout
    match = re.search(r"version (\d+)\.(\d+)\.(\d+)", output)
    if not match:
        return f"could not parse `{clang_format} --version` output: {output.strip()}"
    if match.group(1) != CLANG_FORMAT_VERSION.split(".")[0]:
        return f"{clang_format} is {match.group(0)}; this repository pins {CLANG_FORMAT_VERSION}"
    return None


def main() -> bool:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fix", action="store_true", help="rewrite files in place instead of reporting")
    parser.add_argument("--clang-format", default=shutil.which("clang-format"), help="clang-format executable")
    parser.add_argument("--no-version-check", action="store_true", help="accept any clang-format major version")
    args = parser.parse_args()

    if not args.clang_format:
        print("clang-format was not found; install version " + CLANG_FORMAT_VERSION, file=sys.stderr)
        return True
    if not args.no_version_check:
        problem = check_version(args.clang_format)
        if problem:
            print(problem + " (pass --no-version-check to override)", file=sys.stderr)
            return True

    root = Path(__file__).resolve().parents[1]
    files = [str(path) for path in project_sources(root)]
    command = [args.clang_format, "-i" if args.fix else "--dry-run", "-Werror", "--style=file"] + files
    result = subprocess.run(command, cwd=root, capture_output=True, text=True)
    if result.returncode == 0:
        return False
    if args.fix:
        print(result.stderr, file=sys.stderr)
        return True
    failing = sorted({line.split(":", 1)[0] for line in result.stderr.splitlines() if ": error:" in line})
    print("Files not formatted per .clang-format (run scripts/check_format.py --fix):", file=sys.stderr)
    print("\n".join(failing), file=sys.stderr)
    return True


if __name__ == "__main__":
    sys.exit(main())
