#!/usr/bin/env python3
"""Check repository and optional teacher configuration before running jobs."""

import argparse
import json
import os
from pathlib import Path


REQUIRED_REPO_FILES = (
    "README.md",
    "LICENSE",
    ".gitignore",
    "THIRD_PARTY_NOTICES.md",
    "training/README.md",
    "tools/training/annotate_uci.py",
)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", default=Path.cwd(), type=Path)
    parser.add_argument("--config", type=Path)
    args = parser.parse_args()
    repo = args.repo.resolve()
    errors = []
    for relative in REQUIRED_REPO_FILES:
        if not (repo / relative).is_file():
            errors.append(f"missing repository file: {relative}")
    if args.config:
        config_path = args.config if args.config.is_absolute() else repo / args.config
        if not config_path.is_file():
            errors.append(f"missing config: {config_path}")
        else:
            config = json.loads(config_path.read_text(encoding="utf-8"))
            engine_path = Path(config.get("engine_path", ""))
            if not engine_path.is_file():
                errors.append(f"engine is not a file: {engine_path}")
            elif os.name != "nt" and not os.access(engine_path, os.X_OK):
                errors.append(f"engine is not executable: {engine_path}")
            for positive in ("threads", "hash_mb", "multipv", "movetime_ms", "position_timeout_ms"):
                if not isinstance(config.get(positive), int) or config[positive] < 1:
                    errors.append(f"config field must be a positive integer: {positive}")
    if errors:
        for error in errors:
            print("ERROR:", error)
        return 1
    print("preflight passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
