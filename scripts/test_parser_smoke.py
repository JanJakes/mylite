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
        ("select from t;", 1, {}),
        ("select * from t;", 0, {"statements": "1", "select": "1"}),
        ("nonsense 1;", 1, {}),
        ("/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE */;", 0, {"utility": "1"}),
        (
            "create procedure p() begin select 1; select 2; end;",
            0,
            {"statements": "1", "ddl": "1"},
        ),
        ("create table t (a int);", 0, {"statements": "1", "ddl": "1"}),
        ("create nonsense;", 1, {}),
        ("drop table if exists t;", 0, {"statements": "1", "ddl": "1"}),
        ("drop nonsense;", 1, {}),
        ("alter table t add column a int;", 0, {"statements": "1", "ddl": "1"}),
        ("alter algorithm=merge view v as select 1;", 0, {"statements": "1", "ddl": "1"}),
        ("alter nonsense;", 1, {}),
        ("rename table a to b;", 0, {"statements": "1", "ddl": "1"}),
        ("rename tables a to b;", 0, {"statements": "1", "ddl": "1"}),
        ("rename user a to b;", 0, {"statements": "1", "ddl": "1"}),
        ("rename nonsense;", 1, {}),
        ("truncate table t;", 0, {"statements": "1", "ddl": "1"}),
        ("truncate t;", 0, {"statements": "1", "ddl": "1"}),
        ("truncate select;", 1, {}),
        ("load data infile 'x' into table t;", 0, {"statements": "1", "utility": "1"}),
        ("load xml infile 'x' into table t;", 0, {"statements": "1", "utility": "1"}),
        ("load index into cache t;", 0, {"statements": "1", "utility": "1"}),
        ("load nonsense;", 1, {}),
        ("start transaction read only;", 0, {"statements": "1", "transaction": "1"}),
        ("start nonsense;", 1, {}),
        ("savepoint s1;", 0, {"statements": "1", "transaction": "1"}),
        ("savepoint select;", 1, {}),
        ("release savepoint s1;", 0, {"statements": "1", "transaction": "1"}),
        ("release nonsense;", 1, {}),
        ("lock tables t write;", 0, {"statements": "1", "transaction": "1"}),
        ("lock nonsense;", 1, {}),
        ("unlock tables;", 0, {"statements": "1", "transaction": "1"}),
        ("unlock nonsense;", 1, {}),
        ("analyze table t;", 0, {"statements": "1", "admin": "1"}),
        ("check tables t;", 0, {"statements": "1", "admin": "1"}),
        ("checksum table t;", 0, {"statements": "1", "admin": "1"}),
        ("optimize local table t;", 0, {"statements": "1", "admin": "1"}),
        ("repair no_write_to_binlog table t;", 0, {"statements": "1", "admin": "1"}),
        ("analyze nonsense;", 1, {}),
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
