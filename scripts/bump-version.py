#!/usr/bin/env python3
"""Keep the application version and package examples in sync for a release."""

import argparse
from pathlib import Path
import re
import tomllib


def bump_version(root: Path, increment: str) -> str:
    positions = {"major": 0, "minor": 1, "patch": 2}
    if increment not in positions:
        raise ValueError(f"Unknown version increment: {increment}")

    paths = (
        "Cargo.toml",
        "Cargo.lock",
        "kwin-effect/CMakeLists.txt",
        "README.md",
        "docs/distribution-packaging.md",
    )
    contents = {name: (root / name).read_text() for name in paths}
    current = tomllib.loads(contents["Cargo.toml"])["package"]["version"]
    if not re.fullmatch(r"(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)", current):
        raise ValueError(f"Expected a stable major.minor.patch version, got {current}")

    packages = tomllib.loads(contents["Cargo.lock"])["package"]
    locked_versions = [p["version"] for p in packages if p["name"] == "pulsepointer"]
    effect = re.search(
        r"(?m)^project\(pulsepointer-kwin-effect VERSION ([^ ]+) LANGUAGES CXX\)$",
        contents["kwin-effect/CMakeLists.txt"],
    )
    if locked_versions != [current] or effect is None or effect[1] != current:
        raise ValueError("Cargo.toml, Cargo.lock, and the KWin project versions must match")

    numbers = [int(number) for number in current.split(".")]
    position = positions[increment]
    numbers[position] += 1
    numbers[position + 1:] = [0] * (2 - position)
    updated = ".".join(str(number) for number in numbers)

    patterns = {
        "Cargo.toml": r'(?m)^(version = ")[^"]+("$)',
        "Cargo.lock": r'(?m)^(name = "pulsepointer"\nversion = ")[^"]+("$)',
        "kwin-effect/CMakeLists.txt": (
            r"(?m)^(project\(pulsepointer-kwin-effect VERSION )[^ ]+( LANGUAGES CXX\)$)"
        ),
    }
    for name, pattern in patterns.items():
        contents[name], count = re.subn(
            pattern, lambda match: match[1] + updated + match[2], contents[name]
        )
        if count != 1:
            raise ValueError(f"Expected exactly one application version in {name}")
    for name in ("README.md", "docs/distribution-packaging.md"):
        contents[name] = re.sub(
            r"(?<![0-9.])" + re.escape(current) + r"(?![0-9.])",
            updated,
            contents[name],
        )

    # Validate every input before modifying any file.
    for name, content in contents.items():
        (root / name).write_text(content)
    return updated


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("increment", choices=("patch", "minor", "major"))
    args = parser.parse_args()
    try:
        print(bump_version(Path(__file__).resolve().parent.parent, args.increment))
    except (ValueError, KeyError, OSError) as error:
        parser.exit(1, f"Version update failed: {error}\n")


if __name__ == "__main__":
    main()
