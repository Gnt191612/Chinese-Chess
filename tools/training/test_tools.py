#!/usr/bin/env python3
"""Smoke tests for the training helpers, including a fake UCI process."""

import sys
import unittest

from annotate_uci import UciEngine, parse_info


def run_fake_engine() -> None:
    for raw_line in sys.stdin:
        command = raw_line.strip()
        if command == "uci":
            print("id name fake-uci", flush=True)
            print("uciok", flush=True)
        elif command == "isready":
            print("readyok", flush=True)
        elif command.startswith("go "):
            print("info depth 8 multipv 1 score cp 23 nodes 1000 time 5 pv a0a1 b0b1", flush=True)
            print("info depth 8 multipv 2 score cp 10 nodes 1000 time 5 pv c0c1", flush=True)
            print("bestmove a0a1", flush=True)
        elif command == "quit":
            return


class TrainingToolsTest(unittest.TestCase):
    def test_parse_info(self) -> None:
        parsed = parse_info("info depth 12 multipv 2 score mate -3 nodes 42 pv a0a1")
        self.assertEqual(parsed["depth"], 12)
        self.assertEqual(parsed["multipv"], 2)
        self.assertEqual(parsed["score_type"], "mate")
        self.assertEqual(parsed["score"], -3)
        self.assertEqual(parsed["pv"], ["a0a1"])

    def test_fake_engine_round_trip(self) -> None:
        engine = UciEngine(
            {
                "engine_path": sys.executable,
                "engine_args": [__file__, "--fake-engine"],
                "threads": 1,
                "hash_mb": 16,
                "multipv": 2,
            }
        )
        try:
            result = engine.analyse("fake-fen", 10, 1000)
        finally:
            engine.close()
        self.assertEqual(result["bestmove"], "a0a1")
        self.assertEqual(len(result["candidates"]), 2)
        self.assertEqual(result["candidates"][0]["score"], 23)


if __name__ == "__main__":
    if "--fake-engine" in sys.argv:
        run_fake_engine()
    else:
        unittest.main()

