#!/usr/bin/env python3
import argparse
import subprocess


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", default="build/mylite-parse")
    args = parser.parse_args()

    cases = [
        ("select 1;", 0, {"statements": "1", "select": "1"}),
        ("select ;", 1, {}),
        ("nonsense 1;", 1, {}),
        ("/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE */;", 0, {"utility": "1"}),
        (
            "create procedure p() begin select 1; select 2; end;",
            0,
            {"statements": "1", "ddl": "1"},
        ),
    ]

    for sql, expected_rc, expected_stats in cases:
        completed = subprocess.run(
            [args.exe, "--stats"],
            input=sql.encode(),
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
        )
        if completed.returncode != expected_rc:
            print(f"unexpected return code for {sql!r}: {completed.returncode}")
            print(completed.stdout.decode())
            print(completed.stderr.decode())
            return 1

        stats = parse_stats(completed.stdout.decode())
        for key, value in expected_stats.items():
            if stats.get(key) != value:
                print(f"unexpected stat for {sql!r}: {key}={stats.get(key)!r}")
                print(completed.stdout.decode())
                return 1

    print(f"passed {len(cases)} parser smoke cases")
    return 0


def parse_stats(output: str) -> dict[str, str]:
    stats: dict[str, str] = {}
    for line in output.splitlines():
        if "=" not in line:
            continue
        key, value = line.split("=", 1)
        stats[key] = value
    return stats


if __name__ == "__main__":
    raise SystemExit(main())
