#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_select_row_scalar_predicates_$$"

fail() {
    printf '%s\n' "mysql_baseline_select_row_scalar_predicates_expectations: $1" >&2
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

predicate_expected=$(cat <<'EXPECTED'
hex-eq	1
lower-eq	1
upper-rhs	2
coalesce-between	1,2,3
greatest-eq	1
least-not-between	1
nullif-is-null	1,3
ifnull-is-not-null	1,2,3
timestamp-between	1
datediff-eq	1
json-extract-eq	2
column-rhs	1
function-rhs	1,2
extrema-rhs	1
control-rhs	1,2,3
if-column-rhs	1
ifnull-column-rhs	1
coalesce-column-rhs	1
nullif-column-rhs	1
isnull-column-rhs	1
numeric-rhs	1,2
concat-ws-rhs	1,2,3
between-function-bounds	1,2
between-string-function-bounds	1,2,3
EXPECTED
)
expect_output \
    "select row-scalar predicates" \
    "$predicate_expected" \
    "CREATE TABLE expr_pred("\
"id INT PRIMARY KEY, "\
"i INT, "\
"v VARCHAR(64), "\
"b VARBINARY(16), "\
"js JSON, "\
"dt DATETIME, "\
"tm TIME) ENGINE=InnoDB; "\
"INSERT INTO expr_pred(id, i, v, b, js, dt, tm) VALUES "\
"(1, 10, 'Alpha', UNHEX('4142'), JSON_OBJECT('a', 1), "\
"'2024-01-02 03:04:05', '00:00:59'), "\
"(2, 0, 'beta', UNHEX('4344'), JSON_OBJECT('a', 2), "\
"'2024-01-03 04:05:06', '00:01:01'), "\
"(3, NULL, NULL, NULL, NULL, NULL, NULL); "\
"SELECT 'hex-eq', IFNULL(GROUP_CONCAT(id ORDER BY id), '') "\
"FROM expr_pred WHERE HEX(b) = '4142'; "\
"SELECT 'lower-eq', IFNULL(GROUP_CONCAT(id ORDER BY id), '') "\
"FROM expr_pred WHERE LOWER(v) = 'alpha'; "\
"SELECT 'upper-rhs', IFNULL(GROUP_CONCAT(id ORDER BY id), '') "\
"FROM expr_pred WHERE UPPER(v) = UPPER('beta'); "\
"SELECT 'coalesce-between', IFNULL(GROUP_CONCAT(id ORDER BY id), '') "\
"FROM expr_pred WHERE COALESCE(i, 0) BETWEEN 0 AND 10; "\
"SELECT 'greatest-eq', IFNULL(GROUP_CONCAT(id ORDER BY id), '') "\
"FROM expr_pred WHERE GREATEST(i, 5) = 10; "\
"SELECT 'least-not-between', IFNULL(GROUP_CONCAT(id ORDER BY id), '') "\
"FROM expr_pred WHERE LEAST(i, 5) NOT BETWEEN 0 AND 4; "\
"SELECT 'nullif-is-null', IFNULL(GROUP_CONCAT(id ORDER BY id), '') "\
"FROM expr_pred WHERE NULLIF(v, 'Alpha') IS NULL; "\
"SELECT 'ifnull-is-not-null', IFNULL(GROUP_CONCAT(id ORDER BY id), '') "\
"FROM expr_pred WHERE IFNULL(v, 'fallback') IS NOT NULL; "\
"SELECT 'timestamp-between', IFNULL(GROUP_CONCAT(id ORDER BY id), '') "\
"FROM expr_pred WHERE TIMESTAMP(dt) BETWEEN "\
"'2024-01-02 00:00:00' AND '2024-01-02 23:59:59'; "\
"SELECT 'datediff-eq', IFNULL(GROUP_CONCAT(id ORDER BY id), '') "\
"FROM expr_pred WHERE DATEDIFF(dt, '2024-01-01') = 1; "\
"SELECT 'json-extract-eq', IFNULL(GROUP_CONCAT(id ORDER BY id), '') "\
"FROM expr_pred WHERE JSON_UNQUOTE(JSON_EXTRACT(js, '$.a')) = '2'; "\
"SELECT 'column-rhs', IFNULL(GROUP_CONCAT(id ORDER BY id), '') "\
"FROM expr_pred WHERE GREATEST(i, 5) = i; "\
"SELECT 'function-rhs', IFNULL(GROUP_CONCAT(id ORDER BY id), '') "\
"FROM expr_pred WHERE CONCAT(v, ':', i) = "\
"CONCAT(v, ':', GREATEST(i, 0)); "\
"SELECT 'extrema-rhs', IFNULL(GROUP_CONCAT(id ORDER BY id), '') "\
"FROM expr_pred WHERE GREATEST(i, 5) = LEAST(i, 10); "\
"SELECT 'control-rhs', IFNULL(GROUP_CONCAT(id ORDER BY id), '') "\
"FROM expr_pred WHERE IFNULL(v, 'fallback') = COALESCE(v, 'fallback'); "\
"SELECT 'if-column-rhs', IFNULL(GROUP_CONCAT(id ORDER BY id), '') "\
"FROM expr_pred WHERE id = IF(1, 1, 0); "\
"SELECT 'ifnull-column-rhs', IFNULL(GROUP_CONCAT(id ORDER BY id), '') "\
"FROM expr_pred WHERE id = IFNULL(1, 0); "\
"SELECT 'coalesce-column-rhs', IFNULL(GROUP_CONCAT(id ORDER BY id), '') "\
"FROM expr_pred WHERE id = COALESCE(1, 0); "\
"SELECT 'nullif-column-rhs', IFNULL(GROUP_CONCAT(id ORDER BY id), '') "\
"FROM expr_pred WHERE id = NULLIF(1, 0); "\
"SELECT 'isnull-column-rhs', IFNULL(GROUP_CONCAT(id ORDER BY id), '') "\
"FROM expr_pred WHERE id = ISNULL(NULL); "\
"SELECT 'numeric-rhs', IFNULL(GROUP_CONCAT(id ORDER BY id), '') "\
"FROM expr_pred WHERE ABS(i) = GREATEST(i, 0); "\
"SELECT 'concat-ws-rhs', IFNULL(GROUP_CONCAT(id ORDER BY id), '') "\
"FROM expr_pred WHERE CONCAT_WS('-', v, i) = "\
"CONCAT_WS('-', v, GREATEST(i, 0)); "\
"SELECT 'between-function-bounds', IFNULL(GROUP_CONCAT(id ORDER BY id), '') "\
"FROM expr_pred WHERE GREATEST(i, 5) BETWEEN LEAST(i, 5) "\
"AND GREATEST(i, 10); "\
"SELECT 'between-string-function-bounds', IFNULL(GROUP_CONCAT(id ORDER BY id), '') "\
"FROM expr_pred WHERE IFNULL(v, 'fallback') BETWEEN "\
"COALESCE(v, 'fallback') AND CONCAT(IFNULL(v, 'fallback'), 'z');" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_select_row_scalar_predicates_expectations: ok"
