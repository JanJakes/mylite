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
        ("install component 'file://x';", 0, {"statements": "1", "admin": "1"}),
        ("uninstall plugin archive;", 0, {"statements": "1", "admin": "1"}),
        ("install nonsense;", 1, {}),
        ("import table from 't.sdi';", 0, {"statements": "1", "utility": "1"}),
        ("import nonsense;", 1, {}),
        ("cache index t in k;", 0, {"statements": "1", "admin": "1"}),
        ("cache nonsense;", 1, {}),
        ("kill query @id;", 0, {"statements": "1", "admin": "1"}),
        ("kill @id;", 0, {"statements": "1", "admin": "1"}),
        ("kill select;", 1, {}),
        ("deallocate prepare s;", 0, {"statements": "1", "prepared": "1"}),
        ("deallocate nonsense;", 1, {}),
        ("reset persist;", 0, {"statements": "1", "admin": "1"}),
        ("reset nonsense;", 1, {}),
        ("purge binary logs to 'bin.0001';", 0, {"statements": "1", "replication": "1"}),
        ("purge nonsense;", 1, {}),
        ("change replication source to source_host='127.0.0.1';", 0, {"statements": "1", "replication": "1"}),
        ("change nonsense;", 1, {}),
        ("xa start 'x';", 0, {"statements": "1", "replication": "1"}),
        ("xa recover;", 0, {"statements": "1", "replication": "1"}),
        ("xa nonsense;", 1, {}),
        ("show tables;", 0, {"statements": "1", "show": "1"}),
        ("show extended full tables from test;", 0, {"statements": "1", "show": "1"}),
        ("show count(*) warnings;", 0, {"statements": "1", "show": "1"}),
        ("show nonsense;", 1, {}),
        ("describe t;", 0, {"statements": "1", "show": "1"}),
        ("describe select * from t;", 0, {"statements": "1", "show": "1"}),
        ("describe from;", 1, {}),
        ("explain select * from t;", 0, {"statements": "1", "show": "1"}),
        ("explain format=tree select * from t;", 0, {"statements": "1", "show": "1"}),
        ("explain for connection 1;", 0, {"statements": "1", "show": "1"}),
        ("explain from;", 1, {}),
        ("use test;", 0, {"statements": "1", "utility": "1"}),
        ("use select;", 1, {}),
        ("handler t open;", 0, {"statements": "1", "utility": "1"}),
        ("handler select open;", 1, {}),
        ("call p();", 0, {"statements": "1", "stored_program": "1"}),
        ("call select();", 1, {}),
        ("binlog 'abc';", 0, {"statements": "1", "replication": "1"}),
        ("binlog select;", 1, {}),
        ("clone local data directory = 'x';", 0, {"statements": "1", "admin": "1"}),
        ("clone nonsense;", 1, {}),
        ("flush tables;", 0, {"statements": "1", "admin": "1"}),
        ("flush relay logs;", 0, {"statements": "1", "admin": "1"}),
        ("flush nonsense;", 1, {}),
        ("restart;", 0, {"statements": "1", "admin": "1"}),
        ("restart now;", 1, {}),
        ("shutdown;", 0, {"statements": "1", "admin": "1"}),
        ("shutdown now;", 1, {}),
        ("insert into t values (1);", 0, {"statements": "1", "insert": "1"}),
        ("insert ignore into t values (1);", 0, {"statements": "1", "insert": "1"}),
        ("insert select;", 1, {}),
        ("replace into t values (1);", 0, {"statements": "1", "replace": "1"}),
        ("replace select;", 1, {}),
        ("update t set a = 1;", 0, {"statements": "1", "update": "1"}),
        ("update select set a = 1;", 1, {}),
        ("delete from t;", 0, {"statements": "1", "delete": "1"}),
        ("delete quick from t;", 0, {"statements": "1", "delete": "1"}),
        ("delete select;", 1, {}),
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
