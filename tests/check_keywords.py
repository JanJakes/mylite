#!/usr/bin/env python3
import re
import sys
from pathlib import Path


def main():
    source = Path("src/lexer.c").read_text()
    words = re.findall(r'\{ "([^"]+)", [A-Z_]+ \}', source)
    for previous, current in zip(words, words[1:]):
        if previous > current:
            print(f"keyword table is not sorted: {previous} before {current}", file=sys.stderr)
            return 1
    if len(words) != len(set(words)):
        print("keyword table contains duplicates", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
