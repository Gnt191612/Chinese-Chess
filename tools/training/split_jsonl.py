#!/usr/bin/env python3
"""Deterministically split a JSONL file into round-robin shards."""

import argparse
import json
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--shards", required=True, type=int)
    args = parser.parse_args()

    if args.shards < 1:
        parser.error("--shards must be positive")
    args.output_dir.mkdir(parents=True, exist_ok=True)
    outputs = [
        (args.output_dir / f"part-{index:05d}.jsonl").open("w", encoding="utf-8", newline="\n")
        for index in range(args.shards)
    ]
    seen = set()
    count = 0
    try:
        with args.input.open("r", encoding="utf-8") as source:
            for line_number, raw_line in enumerate(source, 1):
                if not raw_line.strip():
                    continue
                record = json.loads(raw_line)
                record_id = record.get("id")
                if not isinstance(record_id, str) or not record_id:
                    raise ValueError(f"line {line_number}: missing non-empty id")
                if not isinstance(record.get("fen"), str) or not record["fen"]:
                    raise ValueError(f"line {line_number}: missing non-empty fen")
                if record_id in seen:
                    raise ValueError(f"line {line_number}: duplicate id {record_id!r}")
                seen.add(record_id)
                outputs[count % args.shards].write(
                    json.dumps(record, ensure_ascii=False, separators=(",", ":")) + "\n"
                )
                count += 1
    finally:
        for output in outputs:
            output.close()
    print(f"split {count} records into {args.shards} shards")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

