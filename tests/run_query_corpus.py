#!/usr/bin/env python3
"""Run the MyLite parser CLI against a CSV file with one SQL query per row."""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path


SQL_STARTERS = {
    "ADMIN",
    "ALTER",
    "ANALYZE",
    "BEGIN",
    "BINLOG",
    "CALL",
    "CHANGE",
    "CHECK",
    "CHECKSUM",
    "COMMIT",
    "CREATE",
    "DEALLOCATE",
    "DELETE",
    "DESC",
    "DESCRIBE",
    "DO",
    "DROP",
    "EXECUTE",
    "EXPLAIN",
    "FLUSH",
    "GRANT",
    "HANDLER",
    "HELP",
    "IMPORT",
    "INSERT",
    "INSTALL",
    "KILL",
    "LOAD",
    "LOCK",
    "OPTIMIZE",
    "PREPARE",
    "PURGE",
    "RELEASE",
    "RENAME",
    "REPAIR",
    "REPLACE",
    "RESET",
    "RESTART",
    "REVOKE",
    "ROLLBACK",
    "SAVEPOINT",
    "SELECT",
    "SET",
    "SHOW",
    "SHUTDOWN",
    "START",
    "TABLE",
    "TRACE",
    "TRUNCATE",
    "UNINSTALL",
    "UNLOCK",
    "UPDATE",
    "USE",
    "VALUES",
    "WITH",
    "XA",
}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--parser", required=True, type=Path)
    parser.add_argument("--corpus", required=True, type=Path)
    parser.add_argument("--limit", type=int, default=0)
    parser.add_argument("--fail-fast", action="store_true")
    args = parser.parse_args()

    parsed = 0
    skipped = 0
    failed: list[tuple[int, str, str]] = []
    for row_number, query in iter_corpus_queries(args.corpus):
        if not query.strip():
            continue
        if not starts_like_sql(query):
            skipped += 1
            continue
        result = subprocess.run(
            [str(args.parser)],
            input=query,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
        )
        parsed += 1
        if result.returncode != 0:
            failed.append((row_number, query, result.stderr.strip()))
            if args.fail_fast:
                break
        if args.limit and parsed >= args.limit:
            break

    print(f"parsed={parsed} skipped={skipped} failed={len(failed)}")
    for row_number, query, error in failed[:20]:
        preview = query.replace("\n", "\\n")
        if len(preview) > 240:
            preview = preview[:237] + "..."
        print(f"row={row_number} error={error} query={preview}", file=sys.stderr)

    return 0 if not failed else 1


def iter_corpus_queries(path: Path):
    with path.open(encoding="utf-8", errors="replace") as handle:
        row_number = 0
        while True:
            char = handle.read(1)
            if char == "":
                return
            if char in "\r\n":
                continue

            row_number += 1
            if char != '"':
                yield row_number, (char + handle.readline()).rstrip("\r\n")
                continue

            query: list[str] = []
            while True:
                char = handle.read(1)
                if char == "":
                    yield row_number, "".join(query)
                    return
                if char == "\\":
                    next_char = handle.read(1)
                    if next_char == "":
                        query.append("\\")
                        yield row_number, "".join(query)
                        return
                    if next_char == '"':
                        query.append("\\")
                        query.append('"')
                    else:
                        query.append("\\")
                        query.append(next_char)
                    continue
                if char == '"':
                    next_position = handle.tell()
                    next_char = handle.read(1)
                    if next_char == '"':
                        query.append('"')
                        continue
                    if next_char not in {"", "\n"}:
                        handle.seek(next_position)
                        consume_record_end(handle)
                    yield row_number, "".join(query)
                    if next_char == "":
                        return
                    break
                query.append(char)


def consume_record_end(handle) -> None:
    while True:
        char = handle.read(1)
        if char in {"", "\n"}:
            return


def starts_like_sql(query: str) -> bool:
    stripped = query.lstrip()
    if not stripped:
        return False
    if stripped.startswith(("/*", "--", "#", "(")):
        return True

    first = []
    for char in stripped:
        if char.isalnum() or char == "_":
            first.append(char)
            continue
        break
    return "".join(first).upper() in SQL_STARTERS


if __name__ == "__main__":
    raise SystemExit(main())
