#!/usr/bin/env python3
import argparse
import subprocess
import sys


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--parser", required=True)
    args = parser.parse_args()

    sql = "INSERT INTO t (a) VALUES (1)\n"
    completed = subprocess.run(
        [args.parser, "--semantic"],
        input=sql,
        text=True,
        capture_output=True,
        check=False,
    )
    if completed.returncode != 0:
        sys.stderr.write(completed.stderr)
        return completed.returncode

    required = (
        "semantic_statements=1",
        "statement_kind=insert",
        "target_kind=table",
        "descriptor_kind=column",
        'value="a"',
        "children=1 descriptor_kind=value",
        "expression_kind=literal",
        'value="1"',
    )
    for needle in required:
        if needle not in completed.stdout:
            sys.stderr.write(f"missing semantic dump fragment: {needle}\n")
            sys.stderr.write(completed.stdout)
            return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
