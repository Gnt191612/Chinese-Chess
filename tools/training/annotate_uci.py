#!/usr/bin/env python3
"""Annotate Xiangqi FEN positions with a UCI-compatible teacher engine."""

import argparse
import json
import queue
import subprocess
import threading
import time
from pathlib import Path
from typing import Dict, List, Optional


class UciEngine:
    def __init__(self, config: dict):
        command = [config["engine_path"], *config.get("engine_args", [])]
        self.process = subprocess.Popen(
            command,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            encoding="utf-8",
            errors="replace",
            bufsize=1,
        )
        self.lines: "queue.Queue[Optional[str]]" = queue.Queue()
        self.stderr_tail: List[str] = []
        threading.Thread(target=self._read_stdout, daemon=True).start()
        threading.Thread(target=self._read_stderr, daemon=True).start()
        self.send("uci")
        self.wait_for("uciok", 15.0)
        self.set_option("Threads", config.get("threads", 1))
        self.set_option("Hash", config.get("hash_mb", 256))
        self.set_option("MultiPV", config.get("multipv", 3))
        for name, value in config.get("extra_options", {}).items():
            self.set_option(name, value)
        self.send("isready")
        self.wait_for("readyok", 15.0)
        self.send("ucinewgame")

    def _read_stdout(self) -> None:
        assert self.process.stdout is not None
        for line in self.process.stdout:
            self.lines.put(line.rstrip("\r\n"))
        self.lines.put(None)

    def _read_stderr(self) -> None:
        assert self.process.stderr is not None
        for line in self.process.stderr:
            self.stderr_tail.append(line.rstrip("\r\n"))
            del self.stderr_tail[:-20]

    def send(self, command: str) -> None:
        if self.process.poll() is not None:
            raise RuntimeError(f"engine exited with code {self.process.returncode}")
        assert self.process.stdin is not None
        self.process.stdin.write(command + "\n")
        self.process.stdin.flush()

    def set_option(self, name: str, value: object) -> None:
        self.send(f"setoption name {name} value {value}")

    def wait_for(self, token: str, timeout_seconds: float) -> List[str]:
        deadline = time.monotonic() + timeout_seconds
        captured = []
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError(f"engine did not return {token!r}")
            try:
                line = self.lines.get(timeout=remaining)
            except queue.Empty as exc:
                raise TimeoutError(f"engine did not return {token!r}") from exc
            if line is None:
                raise RuntimeError("engine closed stdout")
            captured.append(line)
            if line.startswith(token):
                return captured

    def analyse(self, fen: str, movetime_ms: int, timeout_ms: int) -> dict:
        self.send(f"position fen {fen}")
        started = time.monotonic()
        self.send(f"go movetime {movetime_ms}")
        lines = self.wait_for("bestmove", timeout_ms / 1000.0)
        elapsed_ms = round((time.monotonic() - started) * 1000)
        bestmove_line = lines[-1].split()
        bestmove = bestmove_line[1] if len(bestmove_line) > 1 else None
        candidates: Dict[int, dict] = {}
        for line in lines:
            parsed = parse_info(line)
            if parsed is not None:
                candidates[parsed["multipv"]] = parsed
        return {
            "elapsed_ms": elapsed_ms,
            "bestmove": bestmove,
            "candidates": [candidates[key] for key in sorted(candidates)],
        }

    def close(self) -> None:
        if self.process.poll() is None:
            try:
                self.send("quit")
                self.process.wait(timeout=3)
            except (BrokenPipeError, subprocess.TimeoutExpired):
                self.process.kill()
                self.process.wait()
        for stream in (self.process.stdin, self.process.stdout, self.process.stderr):
            if stream is not None:
                stream.close()


def parse_info(line: str) -> Optional[dict]:
    fields = line.split()
    if not fields or fields[0] != "info" or "score" not in fields or "pv" not in fields:
        return None
    result = {"multipv": 1}
    for key in ("depth", "seldepth", "multipv", "nodes", "nps", "time"):
        if key in fields:
            index = fields.index(key)
            if index + 1 < len(fields) and fields[index + 1].lstrip("-").isdigit():
                result[key] = int(fields[index + 1])
    score_index = fields.index("score")
    if score_index + 2 >= len(fields):
        return None
    result["score_type"] = fields[score_index + 1]
    try:
        result["score"] = int(fields[score_index + 2])
    except ValueError:
        return None
    result["bound"] = "lower" if "lowerbound" in fields else "upper" if "upperbound" in fields else "exact"
    pv_index = fields.index("pv")
    result["pv"] = fields[pv_index + 1 :]
    return result


def load_completed(path: Path) -> set:
    completed = set()
    if not path.exists():
        return completed
    with path.open("r", encoding="utf-8") as source:
        for line_number, line in enumerate(source, 1):
            if not line.strip():
                continue
            try:
                record = json.loads(line)
            except json.JSONDecodeError as exc:
                raise ValueError(f"existing output line {line_number} is invalid") from exc
            if record.get("status") == "ok":
                completed.add(record.get("id"))
    return completed


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--config", required=True, type=Path)
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--resume", action="store_true")
    args = parser.parse_args()

    config = json.loads(args.config.read_text(encoding="utf-8"))
    required = ("teacher_name", "engine_path", "multipv", "movetime_ms", "position_timeout_ms")
    missing = [name for name in required if name not in config]
    if missing:
        parser.error("missing config fields: " + ", ".join(missing))
    completed = load_completed(args.output) if args.resume else set()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    mode = "a" if args.resume else "w"
    engine = UciEngine(config)
    processed = skipped = failed = 0
    try:
        with args.input.open("r", encoding="utf-8") as source, args.output.open(
            mode, encoding="utf-8", newline="\n", buffering=1
        ) as destination:
            for line_number, line in enumerate(source, 1):
                if not line.strip():
                    continue
                record = json.loads(line)
                record_id = record.get("id")
                fen = record.get("fen")
                if not isinstance(record_id, str) or not isinstance(fen, str):
                    raise ValueError(f"input line {line_number}: id and fen are required")
                if record_id in completed:
                    skipped += 1
                    continue
                output = {
                    "schema_version": 1,
                    "id": record_id,
                    "teacher": config["teacher_name"],
                    "fen": fen,
                    "human_move": record.get("human_move"),
                    "settings": {
                        "threads": config.get("threads", 1),
                        "hash_mb": config.get("hash_mb", 256),
                        "multipv": config["multipv"],
                        "movetime_ms": config["movetime_ms"],
                    },
                }
                for metadata_key in ("game_id", "split", "features", "base_score_red", "feature_schema", "source_sha256"):
                    if metadata_key in record:
                        output[metadata_key] = record[metadata_key]
                try:
                    output.update(
                        engine.analyse(fen, config["movetime_ms"], config["position_timeout_ms"])
                    )
                    output["status"] = "ok" if output["bestmove"] else "error"
                except Exception as exc:  # preserve the failed ID for later retry
                    output.update({"status": "error", "error": type(exc).__name__ + ": " + str(exc)})
                    failed += 1
                destination.write(json.dumps(output, ensure_ascii=False, separators=(",", ":")) + "\n")
                processed += 1
    finally:
        engine.close()
    print(f"processed={processed} skipped={skipped} failed={failed}")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
