#!/usr/bin/env python3
import argparse
import subprocess
import sys
import urllib.request
from pathlib import Path


DEFAULT_URL = (
    "https://raw.githubusercontent.com/WordPress/sqlite-database-integration/"
    "trunk/packages/mysql-on-sqlite/tests/mysql/data/mysql-server-tests-queries.csv"
)
DEFAULT_CSV = Path("build/corpus/mysql-server-tests-queries.csv")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--parser", default="bin/mylite-parse")
    parser.add_argument("--csv", type=Path, default=DEFAULT_CSV)
    parser.add_argument("--download", action="store_true")
    parser.add_argument("--url", default=DEFAULT_URL)
    parser.add_argument("--limit", type=int, default=0)
    args = parser.parse_args()

    if args.download and not args.csv.exists():
        args.csv.parent.mkdir(parents=True, exist_ok=True)
        urllib.request.urlretrieve(args.url, args.csv)

    queries = read_queries(args.csv, args.limit)
    payload = b"\0".join(q.encode("utf-8", errors="surrogatepass") for q in queries)
    if payload:
        payload += b"\0"

    completed = subprocess.run(
        [args.parser, "--nul"],
        input=payload,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if completed.stdout:
        sys.stdout.buffer.write(completed.stdout)
    if completed.stderr:
        sys.stderr.buffer.write(completed.stderr)
    return completed.returncode


def read_queries(path, limit):
    queries = []
    text = path.read_text()
    offset = 0
    while offset < len(text):
        query, offset = read_record(text, offset)
        if query is None:
            break
        if query:
            queries.append(query)
        if limit and len(queries) >= limit:
            break
    return queries


def read_record(text, offset):
    while offset < len(text) and text[offset] in "\r\n":
        offset += 1
    if offset >= len(text):
        return None, offset

    if text[offset] != '"':
        end = text.find("\n", offset)
        if end == -1:
            return text[offset:].rstrip("\r\n"), len(text)
        return text[offset:end].rstrip("\r"), end + 1

    offset += 1
    out = []
    while offset < len(text):
        ch = text[offset]
        if ch == "\\" and offset + 1 < len(text):
            out.append(ch)
            out.append(text[offset + 1])
            offset += 2
            continue
        if ch == '"':
            next_ch = text[offset + 1] if offset + 1 < len(text) else ""
            if next_ch == '"':
                out.append('"')
                offset += 2
                continue
            if next_ch in ("\r", "\n", ""):
                offset += 1
                if offset < len(text) and text[offset] == "\r":
                    offset += 1
                if offset < len(text) and text[offset] == "\n":
                    offset += 1
                return "".join(out), offset
        out.append(ch)
        offset += 1

    return "".join(out), offset


if __name__ == "__main__":
    raise SystemExit(main())
