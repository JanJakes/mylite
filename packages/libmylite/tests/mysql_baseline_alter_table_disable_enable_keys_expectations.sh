#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_alter_table_disable_enable_keys_$$"

fail() {
    printf '%s\n' "mysql_baseline_alter_table_disable_enable_keys_expectations: $1" >&2
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
        *)
            fail "$label: expected error $code/$state containing [$message], got [$output]"
            ;;
    esac
}

expect_contains_output() {
    label=$1
    first=$2
    second=$3
    sql=$4
    shift 4

    output=$(run_mysql "$sql" "$@")
    case "$output" in
        *"$first"*"$second"*) ;;
        *)
            fail "$label: expected output containing [$first] and [$second], got [$output]"
            ;;
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
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

expect_output \
    "disable keys note" \
    "Note	1031	Table storage engine for 't' doesn't have this option" \
    "CREATE TABLE t (id INT, v INT, KEY k_v (v)) ENGINE=InnoDB; "\
"ALTER TABLE t DISABLE KEYS; SHOW WARNINGS;" \
    "$DATABASE"

expect_output \
    "disable keys result state and rows" \
    "0	1	0
1:10
2:20" \
    "INSERT INTO t VALUES (1, 10), (2, 20); "\
"ALTER TABLE t DISABLE KEYS; SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT CONCAT(id, ':', v) FROM t ORDER BY id;" \
    "$DATABASE"

expect_output \
    "enable keys note and result state" \
    "Note	1031	Table storage engine for 't' doesn't have this option
0	1	0" \
    "ALTER TABLE t ENABLE KEYS; SHOW WARNINGS; "\
"ALTER TABLE t ENABLE KEYS; SELECT ROW_COUNT(), @@warning_count, @@error_count;" \
    "$DATABASE"

expect_contains_output \
    "persistent copy note uses internal table name" \
    "Note	1031	Table storage engine for '#sql-" \
    "' doesn't have this option" \
    "ALTER TABLE t ENABLE KEYS, ALGORITHM=COPY; SHOW WARNINGS;" \
    "$DATABASE"

expect_contains_output \
    "temporary table note" \
    "Note	1031	Table storage engine for '#sql-" \
    "' doesn't have this option" \
    "CREATE TEMPORARY TABLE tmp_t (id INT, KEY k_id (id)) ENGINE=InnoDB; "\
"ALTER TABLE tmp_t DISABLE KEYS; SHOW WARNINGS;" \
    "$DATABASE"

expect_output \
    "schema-qualified target without selected default" \
    "0	1	0" \
    "CREATE TABLE ${DATABASE}.qualified_t (id INT, KEY k_id (id)) ENGINE=InnoDB; "\
"ALTER TABLE ${DATABASE}.qualified_t ENABLE KEYS; "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count;"

expect_output \
    "accepted option tails" \
    "0	1	0
2	1	0
2	1	0" \
    "ALTER TABLE t DISABLE KEYS, ALGORITHM=INSTANT; "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"ALTER TABLE t ENABLE KEYS, ALGORITHM=COPY, LOCK=EXCLUSIVE; "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"ALTER TABLE t DISABLE KEYS, ALGORITHM=COPY, LOCK=SHARED; "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count;" \
    "$DATABASE"

expect_error \
    "missing default database" \
    1046 \
    3D000 \
    "No database selected" \
    "ALTER TABLE t DISABLE KEYS;"

expect_error \
    "missing default database with incompatible lock" \
    1046 \
    3D000 \
    "No database selected" \
    "ALTER TABLE t DISABLE KEYS, LOCK=NONE;"

expect_error \
    "missing default database with instant explicit lock" \
    1046 \
    3D000 \
    "No database selected" \
    "ALTER TABLE t DISABLE KEYS, ALGORITHM=INSTANT, LOCK=EXCLUSIVE;"

expect_error \
    "unknown explicit schema" \
    1049 \
    42000 \
    "Unknown database 'missing_schema_${DATABASE}'" \
    "ALTER TABLE missing_schema_${DATABASE}.t DISABLE KEYS;"

expect_error \
    "unknown explicit schema with incompatible lock" \
    1049 \
    42000 \
    "Unknown database 'missing_schema_${DATABASE}'" \
    "ALTER TABLE missing_schema_${DATABASE}.t DISABLE KEYS, LOCK=SHARED;"

expect_error \
    "unknown table" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing_t' doesn't exist" \
    "ALTER TABLE missing_t ENABLE KEYS;" \
    "$DATABASE"

expect_error \
    "unknown table with incompatible lock" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing_t' doesn't exist" \
    "ALTER TABLE missing_t ENABLE KEYS, LOCK=NONE;" \
    "$DATABASE"

expect_error \
    "unknown table with instant explicit lock" \
    1221 \
    HY000 \
    "Incorrect usage of ALGORITHM=INSTANT and LOCK=NONE/SHARED/EXCLUSIVE" \
    "ALTER TABLE missing_t ENABLE KEYS, ALGORITHM=INSTANT, LOCK=EXCLUSIVE;" \
    "$DATABASE"

expect_error \
    "information schema write target" \
    1044 \
    42000 \
    "Access denied for user" \
    "ALTER TABLE information_schema.tables DISABLE KEYS;"

expect_error \
    "information schema write target with incompatible lock" \
    1044 \
    42000 \
    "Access denied for user" \
    "ALTER TABLE information_schema.tables DISABLE KEYS, LOCK=NONE;"

expect_error \
    "information schema write target with instant explicit lock" \
    1044 \
    42000 \
    "Access denied for user" \
    "ALTER TABLE information_schema.tables DISABLE KEYS, ALGORITHM=INSTANT, LOCK=EXCLUSIVE;"

expect_error \
    "lock none rejected" \
    1845 \
    0A000 \
    "LOCK=NONE/SHARED is not supported for this operation. Try LOCK=EXCLUSIVE." \
    "ALTER TABLE t DISABLE KEYS, LOCK=NONE;" \
    "$DATABASE"

expect_error \
    "lock shared rejected without copy" \
    1845 \
    0A000 \
    "LOCK=NONE/SHARED is not supported for this operation. Try LOCK=EXCLUSIVE." \
    "ALTER TABLE t ENABLE KEYS, LOCK=SHARED;" \
    "$DATABASE"

expect_error \
    "copy lock none rejected" \
    1846 \
    0A000 \
    "COPY algorithm requires a lock" \
    "ALTER TABLE t DISABLE KEYS, ALGORITHM=COPY, LOCK=NONE;" \
    "$DATABASE"

expect_error \
    "instant algorithm explicit lock rejected" \
    1221 \
    HY000 \
    "Incorrect usage of ALGORITHM=INSTANT and LOCK=NONE/SHARED/EXCLUSIVE" \
    "ALTER TABLE t ENABLE KEYS, ALGORITHM=INSTANT, LOCK=EXCLUSIVE;" \
    "$DATABASE"

expect_error \
    "single-action grammar requires KEYS" \
    1064 \
    42000 \
    "You have an error" \
    "ALTER TABLE t DISABLE KEY;" \
    "$DATABASE"

expect_upstream_accepts \
    "multi-action disable enable is deferred by MyLite" \
    "ALTER TABLE t DISABLE KEYS, ENABLE KEYS;" \
    "$DATABASE"
