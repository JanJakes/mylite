#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_order_by_field_function_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_order_by_field_function_expectations: $1" >&2
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
"COLLATE utf8mb4_0900_ai_ci;" >/dev/null
run_mysql \
    "CREATE TABLE t(id INT, name VARCHAR(32), n INT); "\
"INSERT INTO t VALUES "\
"(1,'User 0000018',18), "\
"(2,'User 0000019',19), "\
"(3,'User 0000020',20), "\
"(4,'Other',21), "\
"(5,NULL,NULL), "\
"(6,'One',1);" \
    "$DATABASE" >/dev/null

ascending_expected=$(cat <<EXPECTED
2
1
3
EXPECTED
)
expect_output \
    "string FIELD default ascending" \
    "$ascending_expected" \
    "SELECT id FROM t WHERE id IN (1,2,3) "\
"ORDER BY FIELD(name,'User 0000019','User 0000018','User 0000020');" \
    "$DATABASE"

desc_expected=$(cat <<EXPECTED
3
1
2
EXPECTED
)
expect_output \
    "string FIELD descending" \
    "$desc_expected" \
    "SELECT id FROM t WHERE id IN (1,2,3) "\
"ORDER BY FIELD(name,'User 0000019','User 0000018','User 0000020') DESC;" \
    "$DATABASE"

case_expected=$(cat <<EXPECTED
3
1
2
EXPECTED
)
expect_output \
    "case-insensitive FIELD ordering" \
    "$case_expected" \
    "SELECT id FROM t WHERE id IN (1,2,3) "\
"ORDER BY FIELD(name,'user 0000019','USER 0000018','user 0000020') DESC;" \
    "$DATABASE"

integer_expected=$(cat <<EXPECTED
6
4
3
2
EXPECTED
)
expect_output \
    "integer FIELD ordering" \
    "$integer_expected" \
    "SELECT id FROM t WHERE id IN (2,3,4,6) ORDER BY FIELD(n,TRUE,21,20,19);" \
    "$DATABASE"

where_limit_expected=$(cat <<EXPECTED
3
4
EXPECTED
)
expect_output \
    "WHERE before FIELD order and LIMIT after" \
    "$where_limit_expected" \
    "SELECT id FROM t WHERE id BETWEEN 2 AND 4 "\
"ORDER BY FIELD(name,'User 0000020','Other','User 0000019') LIMIT 2;" \
    "$DATABASE"

qualified_expected=$(cat <<EXPECTED
2
1
3
2
1
3
EXPECTED
)
expect_output \
    "qualified FIELD search column" \
    "$qualified_expected" \
    "SELECT id FROM ${DATABASE}.t AS o WHERE id IN (1,2,3) "\
"ORDER BY FIELD(o.name,'User 0000019','User 0000018','User 0000020'); "\
"SELECT id FROM ${DATABASE}.t WHERE id IN (1,2,3) "\
"ORDER BY FIELD(${DATABASE}.t.name,'User 0000019','User 0000018','User 0000020') LIMIT 3;" \
    "$DATABASE"

null_nomatch_expected=$(cat <<EXPECTED
5	0
4	1
3	2
EXPECTED
)
expect_output \
    "NULL and unmatched FIELD ranks" \
    "$null_nomatch_expected" \
    "SELECT id, FIELD(name,'Other','User 0000020') AS field_rank FROM t "\
"WHERE id IN (3,4,5) ORDER BY FIELD(name,'Other','User 0000020') ASC;" \
    "$DATABASE"

expect_error \
    "unknown FIELD order column" \
    1054 \
    "42S22" \
    "Unknown column 'missing' in 'order clause'" \
    "SELECT id FROM t ORDER BY FIELD(missing,'x');" \
    "$DATABASE"

expect_upstream_accepts \
    "multiple order keys accepted by MySQL but deferred by MyLite" \
    "SELECT id FROM t ORDER BY FIELD(name,'Other'), id;" \
    "$DATABASE"

expect_upstream_accepts \
    "join FIELD order accepted by MySQL but deferred by MyLite" \
    "SELECT l.id FROM t AS l JOIN t AS r ON l.id = r.id "\
"ORDER BY FIELD(l.name,'User 0000019') LIMIT 1;" \
    "$DATABASE"

expect_upstream_accepts \
    "DISTINCT FIELD order accepted by MySQL but deferred by MyLite" \
    "SELECT DISTINCT name FROM t ORDER BY FIELD(name,'User 0000019');" \
    "$DATABASE"

expect_upstream_accepts \
    "TABLE FIELD order accepted by MySQL but deferred by MyLite" \
    "TABLE t ORDER BY FIELD(name,'User 0000019');" \
    "$DATABASE"

expect_upstream_accepts \
    "mixed-domain FIELD order accepted by MySQL but deferred by MyLite" \
    "SELECT id FROM t ORDER BY FIELD(name,0,'Other') LIMIT 1; SHOW WARNINGS;" \
    "$DATABASE"

expect_upstream_accepts \
    "nested function FIELD order accepted by MySQL but deferred by MyLite" \
    "SELECT id FROM t ORDER BY FIELD(CONCAT(name,''),'Other') LIMIT 1;" \
    "$DATABASE"

expect_upstream_accepts \
    "single-table UPDATE FIELD order accepted by MySQL but deferred by MyLite" \
    "UPDATE t SET n = n ORDER BY FIELD(name,'User 0000019') LIMIT 1;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_order_by_field_function_expectations: ok"
