#!/usr/bin/env python3
import argparse
import csv
import subprocess
import sys
import urllib.request
from pathlib import Path


CORPUS_URL = (
    "https://raw.githubusercontent.com/WordPress/sqlite-database-integration/"
    "trunk/packages/mysql-on-sqlite/tests/mysql/data/mysql-server-tests-queries.csv"
)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", default="build/mylite-parse")
    parser.add_argument("--corpus", default="tests/parser/.cache/mysql-server-tests-queries.csv")
    parser.add_argument("--limit", type=int, default=0)
    args = parser.parse_args()

    corpus_path = Path(args.corpus)
    ensure_corpus(corpus_path)
    queries = read_queries(corpus_path, args.limit)
    payload = b"\0".join(
        query.encode("utf-8", "surrogateescape") for query in queries
    )

    completed = subprocess.run(
        [args.exe, "--nul", "--permissive"],
        input=payload,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )

    if completed.returncode != 0:
        sys.stdout.buffer.write(completed.stdout)
        sys.stderr.buffer.write(completed.stderr)
        return completed.returncode

    print(f"parsed {len(queries)} corpus queries")
    return 0


def ensure_corpus(path: Path) -> None:
    if path.exists():
        return

    path.parent.mkdir(parents=True, exist_ok=True)
    print(f"downloading parser corpus to {path}")
    urllib.request.urlretrieve(CORPUS_URL, path)


def read_queries(path: Path, limit: int) -> list[str]:
    queries: list[str] = []
    with path.open(newline="", encoding="utf-8", errors="surrogateescape") as f:
        for row in csv.reader(f):
            if not row:
                continue
            queries.append(row[0])
            if limit and len(queries) >= limit:
                break
    return queries


if __name__ == "__main__":
    raise SystemExit(main())
