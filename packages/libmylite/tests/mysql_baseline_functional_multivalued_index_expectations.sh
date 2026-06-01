#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_functional_multivalued_index_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_functional_multivalued_index_expectations: $1" >&2
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

expect_contains() {
    label=$1
    needle=$2
    sql=$3
    shift 3

    output=$(run_mysql "$sql" "$@")
    case "$output" in
        *"$needle"*) ;;
        *) fail "$label: expected output containing [$needle], got [$output]" ;;
    esac
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

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

expect_contains \
    "functional index show create" \
    'KEY `idx_expr` (((`a` + `b`)))' \
    "USE ${DATABASE};
     CREATE TABLE t_func (
       a INT,
       b INT,
       KEY idx_expr ((a + b)),
       KEY idx_mix (a, (a - b) DESC)
     );
     SHOW CREATE TABLE t_func;"

expect_output \
    "functional index statistics" \
    "idx_expr	1	NULL	(\`a\` + \`b\`)	A
idx_mix	1	a	NULL	A
idx_mix	2	NULL	(\`a\` - \`b\`)	D" \
    "USE ${DATABASE};
     SELECT INDEX_NAME, SEQ_IN_INDEX, COLUMN_NAME, EXPRESSION, COLLATION
       FROM INFORMATION_SCHEMA.STATISTICS
      WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 't_func'
      ORDER BY INDEX_NAME, SEQ_IN_INDEX;"

expect_contains \
    "multi-valued index show create" \
    'KEY `mv` ((cast(json_extract(`j`,_latin1'\''$.ids'\'') as unsigned array)))' \
    "USE ${DATABASE};
     CREATE TABLE t_mv (
       id BIGINT NOT NULL AUTO_INCREMENT PRIMARY KEY,
       j JSON,
       KEY mv ((CAST(j->'$.ids' AS UNSIGNED ARRAY)))
     );
     SHOW CREATE TABLE t_mv;"

expect_output \
    "multi-valued index statistics" \
    "mv	1	NULL	cast(json_extract(\`j\`,_latin1\\'$.ids\\') as unsigned array)
PRIMARY	1	id	NULL" \
    "USE ${DATABASE};
     SELECT INDEX_NAME, SEQ_IN_INDEX, COLUMN_NAME, EXPRESSION
       FROM INFORMATION_SCHEMA.STATISTICS
      WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 't_mv'
      ORDER BY INDEX_NAME, SEQ_IN_INDEX;"

expect_error \
    "functional index on bare column" \
    3762 \
    HY000 \
    "Functional index on a column is not supported" \
    "USE ${DATABASE}; CREATE TABLE t_bad_column (a INT, KEY k ((a)));"

expect_error \
    "functional primary key" \
    3756 \
    HY000 \
    "primary key cannot be a functional index" \
    "USE ${DATABASE}; CREATE TABLE t_bad_primary (a INT, PRIMARY KEY ((a + 1)));"

expect_error \
    "multi-valued explicit order" \
    1221 \
    HY000 \
    "Incorrect usage of multi-valued index and explicit index order" \
    "USE ${DATABASE}; CREATE TABLE t_bad_order (
       j JSON,
       KEY mv ((CAST(j->'$.ids' AS UNSIGNED ARRAY)) DESC)
     );"

expect_error \
    "two multi-valued parts" \
    1235 \
    42000 \
    "more than one multi-valued key part per index" \
    "USE ${DATABASE}; CREATE TABLE t_bad_two (
       j JSON,
       KEY mv ((CAST(j->'$.a' AS UNSIGNED ARRAY)), (CAST(j->'$.b' AS UNSIGNED ARRAY)))
     );"

printf '%s\n' "mysql_baseline_functional_multivalued_index_expectations: ok"
