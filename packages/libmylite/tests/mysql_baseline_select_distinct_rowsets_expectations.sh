#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_select_distinct_rowsets_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_select_distinct_rowsets_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --default-character-set=utf8mb4 --batch --raw --skip-column-names "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --default-character-set=utf8mb4 --batch --raw "$@"
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

expect_error_contains() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    set +e
    output=$(run_mysql "$sql" "$@" 2>&1)
    status_code=$?
    set -e

    if [ "$status_code" -eq 0 ]; then
        fail "$label: expected MySQL to reject statement, got success [$output]"
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

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE};" >/dev/null

run_mysql \
    "CREATE TABLE t ("\
"a INT NULL, b INT NULL, s VARCHAR(16) NULL, txt TEXT NULL, y YEAR NULL, "\
"d DATE NULL, tm TIME NULL, dt DATETIME NULL); "\
"INSERT INTO t VALUES "\
"(1,10,'Alpha','Text',2020,'2020-01-01','01:02:03','2020-01-01 01:02:03'),"\
"(1,10,'alpha','text',2020,'2020-01-01','01:02:03','2020-01-01 01:02:03'),"\
"(1,11,'Beta','Text',2021,'2020-01-02','-01:02:03','2020-01-02 01:02:03'),"\
"(NULL,11,NULL,NULL,NULL,NULL,NULL,NULL),"\
"(NULL,11,NULL,NULL,NULL,NULL,NULL,NULL);" \
    "$DATABASE" >/dev/null

numeric_expected=$(cat <<EXPECTED
NULL	11
1	10
1	11
EXPECTED
)
expect_output \
    "multi-column integer distinct rowsets" \
    "$numeric_expected" \
    "SELECT DISTINCT a, b FROM t ORDER BY a, b;" \
    "$DATABASE"

strings_expected=$(cat <<EXPECTED
NULL
Alpha
Beta
EXPECTED
)
expect_output \
    "ASCII string distinct uses default case-insensitive collation" \
    "$strings_expected" \
    "SELECT DISTINCT s FROM t ORDER BY s;" \
    "$DATABASE"

string_rows_expected=$(cat <<EXPECTED
NULL	NULL
Alpha	Text
Beta	Text
EXPECTED
)
expect_output \
    "multi-column string distinct rowsets" \
    "$string_rows_expected" \
    "SELECT DISTINCT s, txt FROM t ORDER BY s, txt;" \
    "$DATABASE"

temporal_expected=$(cat <<EXPECTED
NULL	NULL	NULL	NULL
2020	2020-01-01	01:02:03	2020-01-01 01:02:03
2021	2020-01-02	-01:02:03	2020-01-02 01:02:03
EXPECTED
)
expect_output \
    "year and temporal distinct rowsets" \
    "$temporal_expected" \
    "SELECT DISTINCT y, d, tm, dt FROM t ORDER BY y, d, tm, dt;" \
    "$DATABASE"

alias_expected=$(cat <<EXPECTED
1	10
NULL	11
1	11
EXPECTED
)
expect_output \
    "selected aliases can order distinct rowsets" \
    "$alias_expected" \
    "SELECT DISTINCT a AS x, b AS y FROM t ORDER BY y, x;" \
    "$DATABASE"

distinctrow_expected=$(cat <<EXPECTED
NULL	11
1	10
1	11
EXPECTED
)
expect_output \
    "DISTINCTROW synonym" \
    "$distinctrow_expected" \
    "SELECT DISTINCTROW a, b FROM t ORDER BY a, b LIMIT 10;" \
    "$DATABASE"

limit_expected=$(cat <<EXPECTED
1	10
1	11
EXPECTED
)
expect_output \
    "distinct limit offset applies to unique rows" \
    "$limit_expected" \
    "SELECT DISTINCT a, b FROM t ORDER BY a, b LIMIT 2 OFFSET 1;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE visible_dupes (a INT, b INT, s VARCHAR(16)); "\
"INSERT INTO visible_dupes VALUES "\
"(1,10,'Alpha'),(1,10,'alpha'),(1,11,'Alpha'),(NULL,11,NULL),(NULL,11,NULL);" \
    "$DATABASE" >/dev/null

wildcard_expected=$(cat <<EXPECTED
NULL	11	NULL
1	10	Alpha
1	11	Alpha
EXPECTED
)
expect_output \
    "wildcard distinct visible rows" \
    "$wildcard_expected" \
    "SELECT DISTINCT * FROM visible_dupes ORDER BY a, b, s;" \
    "$DATABASE"

qualified_wildcard_expected=$(cat <<EXPECTED
NULL	11	NULL
1	10	Alpha
1	11	Alpha
EXPECTED
)
expect_output \
    "qualified wildcard distinct visible rows" \
    "$qualified_wildcard_expected" \
    "SELECT DISTINCT vd.* FROM visible_dupes AS vd ORDER BY a, b, s;" \
    "$DATABASE"

labels_expected=$(cat <<EXPECTED
x	y
NULL	11
1	10
1	11
EXPECTED
)
expect_output_with_headers \
    "distinct result labels" \
    "$labels_expected" \
    "SELECT DISTINCT a AS x, b AS y FROM t ORDER BY x, y;" \
    "$DATABASE"

status_expected=$(cat <<EXPECTED
NULL	11
1	10
1	11
-1	0
EXPECTED
)
expect_output \
    "distinct row count and warning count" \
    "$status_expected" \
    "SELECT DISTINCT a, b FROM t ORDER BY a, b; SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_upstream_accepts \
    "MySQL accepts scalar distinct, deferred in MyLite" \
    "SELECT DISTINCT 1;" \
    "$DATABASE"

expect_error_contains \
    "MySQL rejects default-mode distinct order by non-selected descriptor column" \
    "ERROR 3065 (HY000)" \
    "SET SESSION sql_mode = 'ONLY_FULL_GROUP_BY'; SELECT DISTINCT a FROM t ORDER BY b;" \
    "$DATABASE"

non_selected_order_expected=$(cat <<EXPECTED
1
NULL
EXPECTED
)
expect_output \
    "loose mode distinct accepts non-selected descriptor order column" \
    "$non_selected_order_expected" \
    "SET SESSION sql_mode = ''; SELECT DISTINCT a FROM t ORDER BY b;" \
    "$DATABASE"

joined_distinct_expected=$(cat <<EXPECTED
1	1
1	2
EXPECTED
)
expect_output \
    "joined distinct rowsets" \
    "$joined_distinct_expected" \
    "SET SESSION sql_mode = ''; CREATE TABLE j (a INT, c INT); INSERT INTO j VALUES (1,1), (1,2); "\
"SELECT DISTINCT t.a, j.c FROM t JOIN j ON t.a = j.a ORDER BY t.a, j.c;" \
    "$DATABASE"

joined_non_selected_order_expected=$(cat <<EXPECTED
10
11
EXPECTED
)
expect_output \
    "joined distinct accepts non-selected descriptor order column" \
    "$joined_non_selected_order_expected" \
    "SET SESSION sql_mode = ''; SELECT DISTINCT t.b FROM t JOIN j ON t.a = j.a ORDER BY t.s, t.b;" \
    "$DATABASE"
