#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_insert_select_dual_scalar_where_$$"

fail() {
    printf '%s\n' "mysql_baseline_insert_select_dual_scalar_where_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names "$@"
}

expect_value() {
    label=$1
    expected=$2
    actual=$3

    if [ "$actual" != "$expected" ]; then
        fail "$label: expected [$expected], got [$actual]"
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

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
    rm -f "/tmp/${DATABASE}_scalar.out"
}

trap cleanup EXIT

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup

run_mysql \
    "CREATE DATABASE ${DATABASE};
     USE ${DATABASE};
     CREATE TABLE dst(id INT NOT NULL, label VARCHAR(64)) ENGINE=InnoDB;
     CREATE TABLE required_target(id INT NOT NULL, must INT NOT NULL) ENGINE=InnoDB;
     CREATE TABLE replace_target(id INT PRIMARY KEY, label VARCHAR(64)) ENGINE=InnoDB;

     INSERT INTO dst(id, label) SELECT 1, 'literal-true' FROM DUAL WHERE 1;
     SELECT 'literal-true', ROW_COUNT(), @@warning_count, @@error_count;
     INSERT INTO dst(id, label) SELECT 2, 'literal-false' FROM DUAL WHERE 0;
     SELECT 'literal-false', ROW_COUNT(), @@warning_count, @@error_count;
     INSERT INTO dst(id, label) SELECT 3, 'null-filter' FROM DUAL WHERE NULL;
     SELECT 'null-filter', ROW_COUNT(), @@warning_count, @@error_count;
     INSERT INTO dst(id, label) SELECT 4, 'comparison' FROM DUAL WHERE 1 = 1;
     SELECT 'comparison', ROW_COUNT(), @@warning_count, @@error_count;
     INSERT INTO dst(id, label) SELECT 5, 'null-safe' FROM DUAL WHERE NULL <=> NULL;
     SELECT 'null-safe', ROW_COUNT(), @@warning_count, @@error_count;
     INSERT INTO dst(id, label) SELECT 6, 'logical' FROM DUAL WHERE NOT 0 AND (1 OR 0);
     SELECT 'logical', ROW_COUNT(), @@warning_count, @@error_count;
     INSERT INTO dst(id, label) SELECT 7, 'is-true' FROM DUAL WHERE 1 IS TRUE;
     SELECT 'is-true', ROW_COUNT(), @@warning_count, @@error_count;
     INSERT INTO dst(id, label) SELECT 8, 'is-false' FROM DUAL WHERE 0 IS FALSE;
     SELECT 'is-false', ROW_COUNT(), @@warning_count, @@error_count;
     INSERT INTO dst(id, label) SELECT 9, 'is-null' FROM DUAL WHERE NULL IS NULL;
     SELECT 'is-null', ROW_COUNT(), @@warning_count, @@error_count;
     INSERT INTO dst(id, label) SELECT 10, 'is-unknown' FROM DUAL WHERE NULL IS UNKNOWN;
     SELECT 'is-unknown', ROW_COUNT(), @@warning_count, @@error_count;
     INSERT INTO dst(id, label) SELECT 11, 'is-not-true' FROM DUAL WHERE 0 IS NOT TRUE;
     SELECT 'is-not-true', ROW_COUNT(), @@warning_count, @@error_count;
     INSERT INTO dst(id, label) SELECT 12, 'xor' FROM DUAL WHERE 1 XOR 0;
     SELECT 'xor', ROW_COUNT(), @@warning_count, @@error_count;
     INSERT INTO dst(id, label) SELECT 13, 'is-unknown-false' FROM DUAL WHERE 1 IS UNKNOWN;
     SELECT 'is-unknown-false', ROW_COUNT(), @@warning_count, @@error_count;
     INSERT INTO dst(id, label) SELECT 14, 'xor-nonzero-false' FROM DUAL WHERE 1 XOR 2;
     SELECT 'xor-nonzero-false', ROW_COUNT(), @@warning_count, @@error_count;
     INSERT INTO dst(id, label) SELECT 15, 'xor-nonzero-false-2' FROM DUAL WHERE 2 XOR 3;
     SELECT 'xor-nonzero-false-2', ROW_COUNT(), @@warning_count, @@error_count;

     INSERT INTO required_target(id) SELECT 20 FROM DUAL WHERE 0;
     SELECT 'omitted-false', ROW_COUNT(), @@warning_count, @@error_count;

     REPLACE INTO replace_target(id, label) SELECT 1, 'hidden' FROM DUAL WHERE 0;
     SELECT 'replace-false', ROW_COUNT(), @@warning_count, @@error_count;
     REPLACE INTO replace_target(id, label) SELECT 1, 'visible' FROM DUAL WHERE 1 = 1;
     SELECT 'replace-true', ROW_COUNT(), @@warning_count, @@error_count;

     SELECT 'select-true-count', COUNT(*) FROM (SELECT 100 AS value FROM DUAL WHERE 1 = 1) AS q;
     SELECT 'select-false-count', COUNT(*) FROM (SELECT 100 AS value FROM DUAL WHERE 0) AS q;
     SELECT 'select-null-count', COUNT(*) FROM (SELECT 100 AS value FROM DUAL WHERE NULL) AS q;
     SELECT 'select-row-count-status', ROW_COUNT(), @@warning_count, @@error_count;

     SELECT id, label FROM dst ORDER BY id;
     SELECT id, label FROM replace_target ORDER BY id;" \
    >"/tmp/${DATABASE}_scalar.out"

expect_value \
    "scalar DML and SELECT status" \
    "literal-true	1	0	0
literal-false	0	0	0
null-filter	0	0	0
comparison	1	0	0
null-safe	1	0	0
logical	1	0	0
is-true	1	0	0
is-false	1	0	0
is-null	1	0	0
is-unknown	1	0	0
is-not-true	1	0	0
xor	1	0	0
is-unknown-false	0	0	0
xor-nonzero-false	0	0	0
xor-nonzero-false-2	0	0	0
omitted-false	0	0	0
replace-false	0	0	0
replace-true	1	0	0
select-true-count	1
select-false-count	0
select-null-count	0
select-row-count-status	-1	0	0
1	literal-true
4	comparison
5	null-safe
6	logical
7	is-true
8	is-false
9	is-null
10	is-unknown
11	is-not-true
12	xor
1	visible" \
    "$(cat "/tmp/${DATABASE}_scalar.out")"

expect_error \
    "column count mismatch before false filter" \
    1136 \
    21S01 \
    "Column count doesn't match value count at row 1" \
    "USE ${DATABASE}; INSERT INTO required_target(id, must) SELECT 21 FROM DUAL WHERE 0;"

expect_error \
    "unknown select column before false filter" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'field list'" \
    "USE ${DATABASE}; INSERT INTO dst(id, label) SELECT 22, missing FROM DUAL WHERE 0;"

expect_error \
    "unknown where column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'where clause'" \
    "USE ${DATABASE}; INSERT INTO dst(id, label) SELECT 23, 'x' FROM DUAL WHERE missing = 1;"

expect_error \
    "omitted not-null no-default with produced row" \
    1364 \
    HY000 \
    "Field 'must' doesn't have a default value" \
    "USE ${DATABASE}; INSERT INTO required_target(id) SELECT 24 FROM DUAL WHERE 1;"

between_count=$(
    run_mysql \
        "USE ${DATABASE};
         SELECT COUNT(*) FROM (SELECT 1 FROM DUAL WHERE 2 BETWEEN 1 AND 3) AS q;"
)
expect_value "mysql accepts broader between predicate" "1" "$between_count"
