#!/usr/bin/env python3
"""Validate label JSONL structure and unique IDs."""

import argparse
import json
from collections import Counter
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True, type=Path)
    args = parser.parse_args()
    seen = set()
    statuses = Counter()
    count = 0
    with args.input.open("r", encoding="utf-8") as source:
        for line_number, line in enumerate(source, 1):
            if not line.strip():
                continue
            record = json.loads(line)
            record_id = record.get("id")
            if not isinstance(record_id, str) or not record_id:
                raise ValueError(f"line {line_number}: invalid id")
            if record_id in seen:
                raise ValueError(f"line {line_number}: duplicate id {record_id!r}")
            seen.add(record_id)
            status = record.get("status")
            if status not in ("ok", "error"):
                raise ValueError(f"line {line_number}: invalid status")
            if status == "ok":
                if not record.get("bestmove") or not isinstance(record.get("candidates"), list):
                    raise ValueError(f"line {line_number}: incomplete successful label")
            statuses[status] += 1
            count += 1
    print(f"records={count} ok={statuses['ok']} error={statuses['error']}")
    return 1 if statuses["error"] else 0


if __name__ == "__main__":
    raise SystemExit(main())

