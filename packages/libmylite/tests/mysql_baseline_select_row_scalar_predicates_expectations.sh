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
tableless-if-truth	1
tableless-if-false	<empty>
tableless-coalesce-truth	1
tableless-ifnull-comparison	1
tableless-nullif-is-null	1
tableless-isnull-truth	1
tableless-case-truth	1
tableless-scalar-between	1
tableless-scalar-in	1
tableless-coalesce-in	1
dual-greatest-in	1
tableless-deprecated-and	1
tableless-deprecated-or	1
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
string-in	1
numeric-in	1,2,3
not-in	2
function-list-in	1,2,3
mixed-list-in	1,2,3
numeric-order	3,2,1
json-order	2,1,3
temporal-order	2,1,3
hex-order	3,1,2
json-in	1
temporal-in	1
digest-in	1
compression-in	1
coalesce-truth	1
coalesce-not-truth	2,3
if-truth	1
isnull-truth	3
nullif-truth	<empty>
greatest-truth	1
abs-truth	1
datediff-truth	2
json-unquote-truth	1,2
lower-truth	<empty>
hex-truth	1,2
md5-truth	1,2
uncompressed-length-truth	1,2
and-truth	<empty>
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
"SELECT 'tableless-if-truth', IFNULL(GROUP_CONCAT(x), '<empty>') "\
"FROM (SELECT 1 AS x WHERE IF(1, TRUE, FALSE)) AS q; "\
"SELECT 'tableless-if-false', IFNULL(GROUP_CONCAT(x), '<empty>') "\
"FROM (SELECT 1 AS x WHERE IF(0, TRUE, FALSE)) AS q; "\
"SELECT 'tableless-coalesce-truth', IFNULL(GROUP_CONCAT(x), '<empty>') "\
"FROM (SELECT 1 AS x WHERE COALESCE(NULL, 1)) AS q; "\
"SELECT 'tableless-ifnull-comparison', IFNULL(GROUP_CONCAT(x), '<empty>') "\
"FROM (SELECT 1 AS x WHERE IFNULL(NULL, 3) = 3) AS q; "\
"SELECT 'tableless-nullif-is-null', IFNULL(GROUP_CONCAT(x), '<empty>') "\
"FROM (SELECT 1 AS x WHERE NULLIF(1, 1) IS NULL) AS q; "\
"SELECT 'tableless-isnull-truth', IFNULL(GROUP_CONCAT(x), '<empty>') "\
"FROM (SELECT 1 AS x WHERE ISNULL(NULL)) AS q; "\
"SELECT 'tableless-case-truth', IFNULL(GROUP_CONCAT(x), '<empty>') "\
"FROM (SELECT 1 AS x WHERE CASE WHEN 1 THEN TRUE ELSE FALSE END) AS q; "\
"SELECT 'tableless-scalar-between', IFNULL(GROUP_CONCAT(x), '<empty>') "\
"FROM (SELECT 1 AS x WHERE 2 BETWEEN 1 AND 3) AS q; "\
"SELECT 'tableless-scalar-in', IFNULL(GROUP_CONCAT(x), '<empty>') "\
"FROM (SELECT 1 AS x WHERE 1 IN (1, 2)) AS q; "\
"SELECT 'tableless-coalesce-in', IFNULL(GROUP_CONCAT(x), '<empty>') "\
"FROM (SELECT 1 AS x WHERE COALESCE(NULL, 2) IN (1, 2)) AS q; "\
"SELECT 'dual-greatest-in', IFNULL(GROUP_CONCAT(x), '<empty>') "\
"FROM (SELECT 1 AS x FROM DUAL WHERE GREATEST(1, 2) IN (1, 2)) AS q; "\
"SELECT 'tableless-deprecated-and', IFNULL(GROUP_CONCAT(x), '<empty>') "\
"FROM (SELECT 1 AS x WHERE COALESCE(NULL, 1) && NOT IF(0, TRUE, FALSE)) AS q; "\
"SELECT 'tableless-deprecated-or', IFNULL(GROUP_CONCAT(x), '<empty>') "\
"FROM (SELECT 1 AS x WHERE IF(0, TRUE, FALSE) || TRUE) AS q; "\
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
"COALESCE(v, 'fallback') AND CONCAT(IFNULL(v, 'fallback'), 'z'); "\
"SELECT 'string-in', IFNULL(GROUP_CONCAT(id ORDER BY id), '') "\
"FROM expr_pred WHERE LOWER(v) IN ('ALPHA', 'gamma'); "\
"SELECT 'numeric-in', IFNULL(GROUP_CONCAT(id ORDER BY id), '') "\
"FROM expr_pred WHERE COALESCE(i, 0) IN (0, 10); "\
"SELECT 'not-in', IFNULL(GROUP_CONCAT(id ORDER BY id), '') "\
"FROM expr_pred WHERE HEX(b) NOT IN ('4142', 'FFFF'); "\
"SELECT 'function-list-in', IFNULL(GROUP_CONCAT(id ORDER BY id), '') "\
"FROM expr_pred WHERE IFNULL(v, 'fallback') IN "\
"(LOWER(v), COALESCE(v, 'fallback')); "\
"SELECT 'mixed-list-in', IFNULL(GROUP_CONCAT(id ORDER BY id), '') "\
"FROM expr_pred WHERE IFNULL(v, 'fallback') IN "\
"('fallback', COALESCE(v, 'fallback')); "\
"SELECT 'numeric-order', GROUP_CONCAT(id ORDER BY COALESCE(i, -1), id) "\
"FROM expr_pred; "\
"SELECT 'json-order', GROUP_CONCAT(id ORDER BY "\
"JSON_UNQUOTE(JSON_EXTRACT(js, '$.a')) DESC, id) FROM expr_pred; "\
"SELECT 'temporal-order', GROUP_CONCAT(id ORDER BY "\
"DATEDIFF(dt, '2024-01-01') DESC, id) FROM expr_pred; "\
"SELECT 'hex-order', GROUP_CONCAT(id ORDER BY HEX(b), id) "\
"FROM expr_pred; "\
"SELECT 'json-in', IFNULL(GROUP_CONCAT(id ORDER BY id), '') "\
"FROM expr_pred WHERE JSON_UNQUOTE(JSON_EXTRACT(js, '$.a')) IN ('1', '3'); "\
"SELECT 'temporal-in', IFNULL(GROUP_CONCAT(id ORDER BY id), '') "\
"FROM expr_pred WHERE DATEDIFF(dt, '2024-01-01') IN (1, 3); "\
"SELECT 'digest-in', IFNULL(GROUP_CONCAT(id ORDER BY id), '') "\
"FROM expr_pred WHERE MD5(v) IN (MD5('Alpha'), MD5('missing')); "\
"SELECT 'compression-in', IFNULL(GROUP_CONCAT(id ORDER BY id), '') "\
"FROM expr_pred WHERE UNCOMPRESSED_LENGTH(COMPRESS(v)) IN (0, 5); "\
"SELECT 'coalesce-truth', IFNULL(GROUP_CONCAT(id ORDER BY id), '') "\
"FROM expr_pred WHERE COALESCE(i, 0); "\
"SELECT 'coalesce-not-truth', IFNULL(GROUP_CONCAT(id ORDER BY id), '') "\
"FROM expr_pred WHERE NOT COALESCE(i, 0); "\
"SELECT 'if-truth', IFNULL(GROUP_CONCAT(id ORDER BY id), '') "\
"FROM expr_pred WHERE IF(i, i, 0); "\
"SELECT 'isnull-truth', IFNULL(GROUP_CONCAT(id ORDER BY id), '') "\
"FROM expr_pred WHERE ISNULL(i); "\
"SELECT 'nullif-truth', IFNULL(GROUP_CONCAT(id ORDER BY id), '<empty>') "\
"FROM expr_pred WHERE NULLIF(i, 10); "\
"SELECT 'greatest-truth', IFNULL(GROUP_CONCAT(id ORDER BY id), '') "\
"FROM expr_pred WHERE GREATEST(i, 0); "\
"SELECT 'abs-truth', IFNULL(GROUP_CONCAT(id ORDER BY id), '') "\
"FROM expr_pred WHERE ABS(i); "\
"SELECT 'datediff-truth', IFNULL(GROUP_CONCAT(id ORDER BY id), '') "\
"FROM expr_pred WHERE DATEDIFF(dt, '2024-01-02'); "\
"SELECT 'json-unquote-truth', IFNULL(GROUP_CONCAT(id ORDER BY id), '') "\
"FROM expr_pred WHERE JSON_UNQUOTE(JSON_EXTRACT(js, '$.a')); "\
"SELECT 'lower-truth', IFNULL(GROUP_CONCAT(id ORDER BY id), '<empty>') "\
"FROM expr_pred WHERE LOWER(v); "\
"SELECT 'hex-truth', IFNULL(GROUP_CONCAT(id ORDER BY id), '') "\
"FROM expr_pred WHERE HEX(b); "\
"SELECT 'md5-truth', IFNULL(GROUP_CONCAT(id ORDER BY id), '') "\
"FROM expr_pred WHERE MD5(v); "\
"SELECT 'uncompressed-length-truth', IFNULL(GROUP_CONCAT(id ORDER BY id), '') "\
"FROM expr_pred WHERE UNCOMPRESSED_LENGTH(COMPRESS(v)); "\
"SELECT 'and-truth', IFNULL(GROUP_CONCAT(id ORDER BY id), '<empty>') "\
"FROM expr_pred WHERE COALESCE(i, 0) AND IFNULL(v, '0');" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_select_row_scalar_predicates_expectations: ok"
