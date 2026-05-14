#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_table_maintenance_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_table_maintenance_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names "$@"
}

expect_output() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql "$sql" "$@")
    if [ "$output" != "$expected" ]; then
        fail "$label: expected [$expected], got [$output]"
    fi
}

expect_error() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    set +e
    output=$(run_mysql "$sql" "$@" 2>&1)
    status=$?
    set -e

    if [ "$status" -eq 0 ]; then
        fail "$label: expected error containing [$expected], got success [$output]"
    fi

    case "$output" in
        *"$expected"*) ;;
        *) fail "$label: expected error containing [$expected], got [$output]" ;;
    esac
}

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql \
    "CREATE DATABASE ${DATABASE}; "\
"CREATE TABLE ${DATABASE}.a (id INT PRIMARY KEY, v VARCHAR(20)) ENGINE=InnoDB; "\
"CREATE TABLE ${DATABASE}.b (id INT PRIMARY KEY) ENGINE=InnoDB; "\
"INSERT INTO ${DATABASE}.a VALUES (1, 'one'), (2, 'two');" \
    >/dev/null

expect_output \
    "basic maintenance result rows" \
    "${DATABASE}.a	analyze	status	OK
-1	0
${DATABASE}.a	check	status	OK
-1	0
${DATABASE}.a	optimize	note	Table does not support optimize, doing recreate + analyze instead
${DATABASE}.a	optimize	status	OK
-1	0
${DATABASE}.a	repair	note	The storage engine for the table doesn't support repair
-1	0" \
    "USE ${DATABASE}; "\
"ANALYZE TABLE a; SELECT ROW_COUNT(), @@warning_count; "\
"CHECK TABLE a; SELECT ROW_COUNT(), @@warning_count; "\
"OPTIMIZE TABLE a; SELECT ROW_COUNT(), @@warning_count; "\
"REPAIR TABLE a; SELECT ROW_COUNT(), @@warning_count;"

expect_output \
    "options are accepted for baseline forms" \
    "${DATABASE}.a	analyze	status	OK
${DATABASE}.a	analyze	status	OK
${DATABASE}.a	check	status	OK
${DATABASE}.a	check	status	OK
${DATABASE}.a	optimize	note	Table does not support optimize, doing recreate + analyze instead
${DATABASE}.a	optimize	status	OK
${DATABASE}.a	repair	note	The storage engine for the table doesn't support repair" \
    "USE ${DATABASE}; "\
"ANALYZE NO_WRITE_TO_BINLOG TABLE a; "\
"ANALYZE LOCAL TABLE a; "\
"CHECK TABLE a QUICK; "\
"CHECK TABLE a FAST MEDIUM EXTENDED CHANGED FOR UPGRADE; "\
"OPTIMIZE LOCAL TABLE a; "\
"REPAIR LOCAL TABLE a QUICK EXTENDED USE_FRM;"

expect_output \
    "multiple tables preserve input order" \
    "${DATABASE}.b	analyze	status	OK
${DATABASE}.a	analyze	status	OK" \
    "USE ${DATABASE}; ANALYZE TABLE b, a;"

expect_output \
    "schema-qualified table name works without selected schema" \
    "${DATABASE}.a	check	status	OK
-1	0" \
    "CHECK TABLE ${DATABASE}.a; SELECT ROW_COUNT(), @@warning_count;"

expect_error \
    "missing default schema for unqualified table" \
    "ERROR 1046 (3D000)" \
    "ANALYZE TABLE a;"

expect_error \
    "duplicate table aliases are rejected" \
    "ERROR 1066 (42000)" \
    "USE ${DATABASE}; ANALYZE TABLE a, a;"

expect_output \
    "unknown table result rows do not populate warnings" \
    "${DATABASE}.missing	analyze	Error	Table '${DATABASE}.missing' doesn't exist
${DATABASE}.missing	analyze	status	Operation failed
-1	0" \
    "USE ${DATABASE}; ANALYZE TABLE missing; SELECT ROW_COUNT(), @@warning_count; SHOW WARNINGS;"

expect_output \
    "unknown schema result rows" \
    "missing_schema.t	check	Error	Unknown database 'missing_schema'
missing_schema.t	check	error	Corrupt
-1	0" \
    "CHECK TABLE missing_schema.t; SELECT ROW_COUNT(), @@warning_count; SHOW WARNINGS;"

expect_output \
    "implicit commit and savepoint cleanup" \
    "${DATABASE}.a	analyze	status	OK
1" \
    "USE ${DATABASE}; "\
"START TRANSACTION; "\
"INSERT INTO a VALUES (3, 'three'); "\
"SAVEPOINT before_maintenance; "\
"ANALYZE TABLE a; "\
"ROLLBACK; "\
"SELECT COUNT(*) FROM a WHERE id = 3;"

expect_error \
    "maintenance clears savepoints" \
    "ERROR 1305 (42000)" \
    "USE ${DATABASE}; START TRANSACTION; SAVEPOINT s; ANALYZE TABLE a; ROLLBACK TO s;"

cleanup

printf '%s\n' "mysql_baseline_table_maintenance_expectations: ok"
