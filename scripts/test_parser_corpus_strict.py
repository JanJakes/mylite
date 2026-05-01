#!/usr/bin/env python3
import argparse
import re
import subprocess
import sys
from pathlib import Path

import test_parser_corpus as corpus


EXPECTED_FRAGMENT_FAILURES = [
    # Removed RESET MASTER syntax; permissive corpus mode keeps accepting the
    # legacy rows while strict mode requires RESET BINARY LOGS AND GTIDS.
    103,
    # Bare ALTER TABLE negative-test fragments; permissive corpus mode accepts
    # them, but strict mode should require an alter action.
    508,
    509,
    2410,
    # Removed RESET MASTER syntax.
    5873,
    5913,
    7185,
    7187,
    7188,
    7189,
    7190,
    # Removed SHOW MASTER STATUS syntax.
    13642,
    19528,
    # ANALYZE TABLE histogram negative-test cases with multiple table names;
    # permissive corpus mode accepts them, but strict mode follows MySQL 8.4's
    # single-table histogram grammar.
    29001,
    29029,
    # Bare ALTER TABLE negative-test fragments.
    31132,
    # Removed LOCK TABLES LOW_PRIORITY WRITE syntax; permissive corpus mode
    # keeps accepting the legacy test row.
    34746,
    41408,
    # Bare ALTER TABLE REORGANIZE PARTITION negative-test fragment.
    43296,
    # Removed SHOW MASTER STATUS syntax.
    50277,
    # Malformed CREATE USER negative-test artifact with a trailing quote.
    54522,
    # Removed PURGE MASTER LOGS syntax.
    55559,
]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", default="build/mylite-parse")
    parser.add_argument(
        "--corpus",
        default="tests/parser/.cache/mysql-server-tests-queries.csv",
    )
    args = parser.parse_args()

    corpus_path = Path(args.corpus)
    corpus.ensure_corpus(corpus_path)
    queries = corpus.read_queries(corpus_path, 0)
    payload = b"\0".join(
        query.encode("utf-8", "surrogateescape") for query in queries
    )

    completed = subprocess.run(
        [args.exe, "--nul"],
        input=payload,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    output = (completed.stdout + completed.stderr).decode("utf-8", "replace")
    failures = strict_failures(output)

    if failures != EXPECTED_FRAGMENT_FAILURES:
        sys.stdout.write(output)
        print(f"expected strict failures: {EXPECTED_FRAGMENT_FAILURES}")
        print(f"actual strict failures:   {failures}")
        return 1

    print(f"strict corpus failures: {failures}")
    print(f"strict corpus matched {len(failures)} expected fragment failures")
    return 0


def strict_failures(output: str) -> list[int]:
    failures: list[int] = []
    for line in output.splitlines():
        match = re.match(r"query (\d+): ", line)
        if match:
            failures.append(int(match.group(1)))
    return failures


if __name__ == "__main__":
    raise SystemExit(main())
