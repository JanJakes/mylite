#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_find_in_set_function_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_find_in_set_function_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --default-character-set=utf8mb4 "$@"
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

expect_output_with_headers() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql_with_headers "$sql" "$@")
    if [ "$output" != "$expected" ]; then
        fail "$label: expected [$expected], got [$output]"
    fi
}

expect_error() {
    label=$1
    code=$2
    state=$3
    message=$4
    sql=$5
    shift 5

    set +e
    output=$(run_mysql "$sql" "$@" 2>&1)
    status_code=$?
    set -e

    if [ "$status_code" -eq 0 ]; then
        fail "$label: expected error $code/$state, command succeeded with [$output]"
    fi

    case "$output" in
        *"ERROR $code ($state)"*"$message"*) ;;
        *) fail "$label: expected error $code/$state containing [$message], got [$output]" ;;
    esac
}

expect_upstream_accepts() {
    label=$1
    sql=$2
    shift 2

    set +e
    output=$(run_mysql "$sql" "$@" 2>&1)
    status_code=$?
    set -e

    if [ "$status_code" -ne 0 ]; then
        fail "$label: expected MySQL to accept deferred behavior, got [$output]"
    fi
}

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE} DEFAULT CHARACTER SET utf8mb4 "\
"COLLATE utf8mb4_0900_ai_ci; USE ${DATABASE}; SET NAMES utf8mb4;" >/dev/null

scalar_expected=$(cat <<\EXPECTED
2	0	NULL	NULL	NULL	0	0	2	1	2	0	0	1	0	2	2	2	2	1	0
-1	0
EXPECTED
)
expect_output \
    "scalar FIND_IN_SET values" \
    "$scalar_expected" \
    "DO 0; SELECT FIND_IN_SET('b','a,b,c'), FIND_IN_SET('x','a,b,c'), "\
"FIND_IN_SET(NULL,'a,b'), FIND_IN_SET('a',NULL), FIND_IN_SET(NULL,NULL), "\
"FIND_IN_SET('',''), FIND_IN_SET('','a'), FIND_IN_SET('','a,'), "\
"FIND_IN_SET('',',a'), FIND_IN_SET('','a,,b'), FIND_IN_SET('a',''), "\
"FIND_IN_SET('a,b','a,b'), "\
"FIND_IN_SET('abc','ABC,def'), FIND_IN_SET('b','a, b,c'), "\
"FIND_IN_SET(' b','a, b,c'), FIND_IN_SET('b','a,b,b'), "\
"FIND_IN_SET(2,'1,2,3'), FIND_IN_SET(TRUE,'0,1,2'), "\
"FIND_IN_SET(FALSE,'0,1'), @@warning_count; SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

labels_expected=$(cat <<\EXPECTED
FIND_IN_SET ('b','a,b')	fis
2	2
EXPECTED
)
expect_output_with_headers \
    "FIND_IN_SET labels and whitespace" \
    "$labels_expected" \
    "SELECT FIND_IN_SET ('b','a,b'), FIND_IN_SET('green','red,green') AS fis FROM DUAL;" \
    "$DATABASE"

table_expected=$(cat <<\EXPECTED
1	1	1	2	0	1	1	1	1	1	1	3
2	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
EXPECTED
)
expect_output \
    "table-backed FIND_IN_SET values" \
    "$table_expected" \
    "CREATE TABLE t(id INT, i INT, d DECIMAL(6,2), v VARCHAR(20), tx TEXT, y YEAR, "\
"da DATE, ti TIME, dt DATETIME, ts TIMESTAMP, e ENUM('alpha','beta'), "\
"s SET('red','green','blue')); INSERT INTO t VALUES "\
"(1,123,-12.30,'red,green','alpha beta',2024,'2024-01-02','03:04:05', "\
"'2024-01-02 03:04:05','2024-01-02 03:04:06','alpha','red,blue'), "\
"(2,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL); "\
"SELECT id, FIND_IN_SET('123', i), FIND_IN_SET('-12.30', d), "\
"FIND_IN_SET('green', v), FIND_IN_SET('beta', tx), FIND_IN_SET('2024', y), "\
"FIND_IN_SET('2024-01-02', da), FIND_IN_SET('03:04:05', ti), "\
"FIND_IN_SET('2024-01-02 03:04:05', dt), "\
"FIND_IN_SET('2024-01-02 03:04:06', ts), FIND_IN_SET('alpha', e), "\
"FIND_IN_SET('blue', s) FROM t ORDER BY id;" \
    "$DATABASE"

predicate_expected=$(cat <<\EXPECTED
1
1
2
3
1
4
1
2
3
EXPECTED
)
expect_output \
    "FIND_IN_SET predicates" \
    "$predicate_expected" \
    "CREATE TABLE p(id INT, tags VARCHAR(64)); "\
"INSERT INTO p VALUES (1,'red,green'),(2,'blue'),(3,''),(4,NULL); "\
"SELECT id FROM p WHERE FIND_IN_SET('red', tags) ORDER BY id; "\
"SELECT id FROM p WHERE FIND_IN_SET('red', tags) > 0 ORDER BY id; "\
"SELECT id FROM p WHERE FIND_IN_SET('red', tags) = 0 ORDER BY id; "\
"SELECT id FROM p WHERE FIND_IN_SET('red', tags) <> 0 ORDER BY id; "\
"SELECT id FROM p WHERE FIND_IN_SET('red', tags) IS NULL ORDER BY id; "\
"SELECT id FROM p WHERE FIND_IN_SET('red', tags) IS NOT NULL ORDER BY id;" \
    "$DATABASE"

dml_expected=$(cat <<\EXPECTED
2	0
1	hit
2	old
3	old
4	old
5	hit
2	0
1	hit
3	old
4	old
EXPECTED
)
expect_output \
    "FIND_IN_SET DML predicates" \
    "$dml_expected" \
    "CREATE TABLE dml(id INT, tags VARCHAR(64), note VARCHAR(20)); "\
"INSERT INTO dml VALUES (1,'red,green','old'),(2,'blue','old'),(3,'','old'),"\
"(4,NULL,'old'),(5,'Red,blue','old'); "\
"UPDATE dml SET note = 'hit' WHERE FIND_IN_SET('red', tags); "\
"SELECT ROW_COUNT(), @@warning_count; SELECT id,note FROM dml ORDER BY id; "\
"DELETE FROM dml WHERE FIND_IN_SET('blue', tags) > 0; "\
"SELECT ROW_COUNT(), @@warning_count; SELECT id,note FROM dml ORDER BY id;" \
    "$DATABASE"

expect_error \
    "FIND_IN_SET zero arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count in the call to native function 'FIND_IN_SET'" \
    "SELECT FIND_IN_SET();" \
    "$DATABASE"

expect_error \
    "FIND_IN_SET one argument" \
    1582 \
    "42000" \
    "Incorrect parameter count in the call to native function 'FIND_IN_SET'" \
    "SELECT FIND_IN_SET('a');" \
    "$DATABASE"

expect_error \
    "FIND_IN_SET three arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count in the call to native function 'FIND_IN_SET'" \
    "SELECT FIND_IN_SET('a','a','extra');" \
    "$DATABASE"

expect_upstream_accepts \
    "non-ASCII collation accepted by MySQL but deferred by MyLite" \
    "SELECT FIND_IN_SET('e','é');" \
    "$DATABASE"

expect_upstream_accepts \
    "binary matching accepted by MySQL but deferred by MyLite" \
    "SELECT FIND_IN_SET(_binary 'A', _binary 'a,A');" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_find_in_set_function_expectations: ok"
