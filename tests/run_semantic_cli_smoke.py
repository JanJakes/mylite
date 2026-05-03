#!/usr/bin/env python3
import argparse
import subprocess
import sys


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--parser", required=True)
    args = parser.parse_args()

    failures = 0

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
            failures += 1

    sql = "SELECT a FROM t WHERE a = 1\n"
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
        "statement_kind=select",
        "semantic kind=clause",
        "clause_kind=from",
        "clause_kind=where",
        "expression_kind=binary",
        "operator=eq",
    )
    for needle in required:
        if needle not in completed.stdout:
            sys.stderr.write(f"missing semantic dump fragment: {needle}\n")
            sys.stderr.write(completed.stdout)
            failures += 1

    sql = "CREATE TABLE t (a INT DEFAULT 1)\n"
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
        "statement_kind=create",
        "semantic kind=data_type",
        "data_type_family=numeric",
        "data_type_kind=int",
        "storage_class=integer",
        'value="INT"',
    )
    for needle in required:
        if needle not in completed.stdout:
            sys.stderr.write(f"missing semantic dump fragment: {needle}\n")
            sys.stderr.write(completed.stdout)
            failures += 1

    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
