#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_update_index_hints_noop_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_update_index_hints_noop_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | "$MYSQL_BIN" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
    else
        printf '%s\n' "$sql" \
            | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
                --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
    fi
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
        *) fail "$label: expected error $code/$state containing [$message], got [$output]" ;;
    esac
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
"COLLATE utf8mb4_0900_ai_ci;" >/dev/null
run_mysql \
    "CREATE TABLE t ("\
"id INT PRIMARY KEY, n INT, other INT, ka INT, kb INT, "\
"KEY k_n (n), KEY k_other (other), KEY kind_a (ka), KEY kind_b (kb)"\
"); "\
"INSERT INTO t VALUES (1,10,100,1,10),(2,20,200,2,20),(3,20,300,3,30);" \
    "$DATABASE" >/dev/null

use_update_expected=$(cat <<EXPECTED
1	0
1	10	100
2	20	201
3	20	300
EXPECTED
)
expect_output \
    "USE INDEX update orders and limits normally" \
    "$use_update_expected" \
    "UPDATE t USE INDEX (k_n) SET other = 201 WHERE n = 20 ORDER BY id LIMIT 1; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT id, n, other FROM t ORDER BY id;" \
    "$DATABASE"

no_change_expected=$(cat <<EXPECTED
0	0
EXPECTED
)
expect_output \
    "USE KEY scoped no-change update accepted" \
    "$no_change_expected" \
    "UPDATE t USE KEY FOR GROUP BY (k_n) SET other = 100 WHERE id = 1; "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

force_primary_expected=$(cat <<EXPECTED
1	0
1	10	100
2	20	201
3	20	301
EXPECTED
)
expect_output \
    "FORCE KEY scoped primary update accepted" \
    "$force_primary_expected" \
    "UPDATE t FORCE KEY FOR ORDER BY (PRIMARY) SET other = 301 WHERE id = 3; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT id, n, other FROM t ORDER BY id;" \
    "$DATABASE"

ignore_and_empty_expected=$(cat <<EXPECTED
1	0
1	0
0	0
1	10	103
2	20	201
3	20	302
EXPECTED
)
expect_output \
    "IGNORE, empty USE, duplicate names, and prefix hints accepted" \
    "$ignore_and_empty_expected" \
    "UPDATE t IGNORE INDEX (k_other) SET other = 102 WHERE id = 1; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"UPDATE t USE INDEX () SET other = 103 WHERE id = 1; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"UPDATE t USE INDEX (k_n, k_n) SET other = 103 WHERE id = 1; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"UPDATE t FORCE INDEX (k_ot) SET other = 302 WHERE id = 3; "\
"SELECT id, n, other FROM t ORDER BY id;" \
    "$DATABASE"

schema_expected=$(cat <<EXPECTED
1	0
104
EXPECTED
)
expect_output \
    "schema-qualified target hints accepted" \
    "$schema_expected" \
    "UPDATE ${DATABASE}.t USE INDEX (PRIMARY) SET other = 104 WHERE id = 1; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT other FROM ${DATABASE}.t WHERE id = 1;" \
    "$DATABASE"

temporary_expected=$(cat <<EXPECTED
1	0
1	1	10
2	2	25
3	2	30
1	0
35
EXPECTED
)
expect_output \
    "temporary table target hints validate against temporary descriptors" \
    "$temporary_expected" \
    "CREATE TEMPORARY TABLE temp_hint ("\
"id INT PRIMARY KEY, n INT, other INT, KEY k_n (n), KEY k_other (other)); "\
"INSERT INTO temp_hint VALUES (1,1,10),(2,2,20),(3,2,30); "\
"UPDATE temp_hint USE INDEX (k_n) SET other = 25 WHERE n = 2 ORDER BY id LIMIT 1; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT id, n, other FROM temp_hint ORDER BY id; "\
"UPDATE temp_hint FORCE KEY FOR ORDER BY (PRIMARY) SET other = 35 WHERE id = 3; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT other FROM temp_hint WHERE id = 3;" \
    "$DATABASE"

expect_error \
    "missing index rejected" \
    1176 \
    42000 \
    "Key 'missing' doesn't exist in table 't'" \
    "UPDATE t USE INDEX (missing) SET other = 105 WHERE id = 1;" \
    "$DATABASE"

expect_error \
    "ambiguous index prefix rejected" \
    1176 \
    42000 \
    "Key 'kind' doesn't exist in table 't'" \
    "UPDATE t USE INDEX (kind) SET other = 105 WHERE id = 1;" \
    "$DATABASE"

expect_error \
    "USE and FORCE rejected together" \
    1221 \
    HY000 \
    "Incorrect usage of USE INDEX and FORCE INDEX" \
    "UPDATE t USE INDEX (k_n) FORCE INDEX (PRIMARY) SET other = 105 WHERE id = 1;" \
    "$DATABASE"

expect_error \
    "FORCE empty list rejected" \
    1064 \
    42000 \
    "syntax" \
    "UPDATE t FORCE INDEX () SET other = 105 WHERE id = 1;" \
    "$DATABASE"

expect_error \
    "IGNORE empty list rejected" \
    1064 \
    42000 \
    "syntax" \
    "UPDATE t IGNORE INDEX () SET other = 105 WHERE id = 1;" \
    "$DATABASE"
