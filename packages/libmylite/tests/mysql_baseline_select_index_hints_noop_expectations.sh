#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_select_index_hints_noop_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_select_index_hints_noop_expectations: $1" >&2
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
    "CREATE TABLE t (id INT PRIMARY KEY, a INT, b INT, KEY k_a (a), KEY k_b (b), KEY zed (a)); "\
"INSERT INTO t VALUES (1,10,100),(2,20,200),(3,10,300); "\
"CREATE TABLE r (id INT PRIMARY KEY, a INT, c INT, KEY r_a (a)); "\
"INSERT INTO r VALUES (7,10,700),(8,10,800),(9,30,900);" \
    "$DATABASE" >/dev/null

use_index_expected=$(cat <<EXPECTED
1
3
-1	0
EXPECTED
)
expect_output \
    "USE INDEX filters normally" \
    "$use_index_expected" \
    "SELECT id FROM t USE INDEX (k_a) WHERE a = 10 ORDER BY id; "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_output \
    "unambiguous index prefix accepted" \
    "$use_index_expected" \
    "SELECT id FROM t USE INDEX (ze) WHERE a = 10 ORDER BY id; "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_output \
    "USE INDEX empty list accepted" \
    "$use_index_expected" \
    "SELECT id FROM t USE INDEX () WHERE a = 10 ORDER BY id; "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

primary_expected=$(cat <<EXPECTED
1
-1	0
EXPECTED
)
expect_output \
    "PRIMARY prefix accepted" \
    "$primary_expected" \
    "SELECT id FROM t USE KEY (PRI) WHERE id = 1; "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_output \
    "PRIMARY hint accepted" \
    "$primary_expected" \
    "SELECT id FROM t USE KEY (PRIMARY) WHERE id = 1; "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

force_order_expected=$(cat <<EXPECTED
1
2
-1	0
EXPECTED
)
expect_output \
    "FORCE KEY FOR ORDER BY accepted" \
    "$force_order_expected" \
    "SELECT id FROM t FORCE KEY FOR ORDER BY (k_b) ORDER BY b LIMIT 2; "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_output \
    "IGNORE INDEX FOR JOIN accepts duplicates" \
    "$use_index_expected" \
    "SELECT id FROM t IGNORE INDEX FOR JOIN (k_a, k_a, PRIMARY) "\
"WHERE a = 10 ORDER BY id; SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

alias_expected=$(cat <<EXPECTED
1
3
-1	0
EXPECTED
)
expect_output \
    "alias before hint accepted" \
    "$alias_expected" \
    "SELECT x.id FROM t AS x USE INDEX (k_a) WHERE x.a = 10 ORDER BY x.id; "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

group_expected=$(cat <<EXPECTED
10	2
20	1
-1	0
EXPECTED
)
expect_output \
    "USE INDEX FOR GROUP BY accepted" \
    "$group_expected" \
    "SELECT a, COUNT(*) FROM t USE INDEX FOR GROUP BY (k_a) GROUP BY a ORDER BY a; "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

join_expected=$(cat <<EXPECTED
1	7
1	8
3	7
3	8
-1	0
EXPECTED
)
expect_output \
    "join source hints accepted independently" \
    "$join_expected" \
    "SELECT x.id, y.id FROM t AS x USE INDEX (k_a) "\
"JOIN r AS y FORCE KEY FOR JOIN (r_a) ON x.a = y.a "\
"WHERE x.a = 10 ORDER BY x.id, y.id; SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_error \
    "unknown hint name" \
    1176 \
    "42000" \
    "Key 'missing' doesn't exist in table 't'" \
    "SELECT id FROM t USE INDEX (missing) WHERE a = 10;" \
    "$DATABASE"

expect_error \
    "ambiguous hint prefix rejected" \
    1176 \
    "42000" \
    "Key 'k' doesn't exist in table 't'" \
    "SELECT id FROM t USE INDEX (k) WHERE a = 10;" \
    "$DATABASE"

expect_error \
    "use and force conflict" \
    1221 \
    "HY000" \
    "Incorrect usage of USE INDEX and FORCE INDEX" \
    "SELECT id FROM t USE INDEX FOR JOIN (k_a) FORCE INDEX FOR ORDER BY (k_b) "\
"WHERE a = 10 ORDER BY b;" \
    "$DATABASE"

expect_error \
    "force empty syntax" \
    1064 \
    "42000" \
    "near ') WHERE a = 10'" \
    "SELECT id FROM t FORCE INDEX () WHERE a = 10;" \
    "$DATABASE"

expect_error \
    "ignore empty syntax" \
    1064 \
    "42000" \
    "near ') WHERE a = 10'" \
    "SELECT id FROM t IGNORE INDEX () WHERE a = 10;" \
    "$DATABASE"

expect_error \
    "delete index hint rejected" \
    1064 \
    "42000" \
    "near 'USE INDEX(k_a) WHERE a = 999'" \
    "DELETE FROM t USE INDEX(k_a) WHERE a = 999;" \
    "$DATABASE"
