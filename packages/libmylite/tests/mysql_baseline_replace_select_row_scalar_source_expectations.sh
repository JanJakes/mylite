#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_replace_select_scalar_$$"
OTHER_DATABASE="${DATABASE}_other"

fail() {
    printf '%s\n' "mysql_baseline_replace_select_row_scalar_source_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names "$@"
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

expect_value() {
    label=$1
    expected=$2
    actual=$3

    if [ "$actual" != "$expected" ]; then
        fail "$label: expected [$expected], got [$actual]"
    fi
}

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE}; DROP DATABASE IF EXISTS ${OTHER_DATABASE};" \
        >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup

basic_status=$(
    run_mysql \
        "CREATE DATABASE ${DATABASE};
         USE ${DATABASE};
         CREATE TABLE no_key(id INT, v INT NOT NULL DEFAULT 7, req INT NOT NULL);
         REPLACE INTO no_key(id, req) SELECT 1, 10;
         SELECT ROW_COUNT(), @@warning_count, @@error_count;
         REPLACE INTO no_key(id, v, req) SELECT 1, 20, 30 FROM DUAL;
         SELECT ROW_COUNT(), @@warning_count, @@error_count;
         SELECT id, v, req FROM no_key ORDER BY req;"
)
expect_value \
    "no-source and dual no-key replace-select" \
    "1	0	0
1	0	0
1	7	10
1	20	30" \
    "$(printf '%s\n' "$basic_status" | tail -n 4)"

filter_status=$(
    run_mysql \
        "USE ${DATABASE};
         CREATE TABLE filter_target(id INT PRIMARY KEY, v INT NOT NULL);
         REPLACE INTO filter_target(id, v)
             SELECT 1, 10 FROM DUAL WHERE EXISTS(SELECT * FROM filter_target WHERE id = 99);
         SELECT ROW_COUNT(), @@warning_count, COUNT(*) FROM filter_target;
         REPLACE INTO filter_target(id, v)
             SELECT 1, 10 FROM DUAL WHERE NOT EXISTS(SELECT * FROM filter_target WHERE id = 99);
         SELECT ROW_COUNT(), @@warning_count, COUNT(*) FROM filter_target;
         REPLACE INTO filter_target(id, v)
             SELECT 2, 20 FROM DUAL WHERE EXISTS(SELECT * FROM filter_target WHERE id = 1);
         SELECT ROW_COUNT(), @@warning_count, COUNT(*) FROM filter_target;
         SELECT id, v FROM filter_target ORDER BY id;"
)
expect_value \
    "dual exists filters" \
    "0	0	0
1	0	1
1	0	2
1	10
2	20" \
    "$(printf '%s\n' "$filter_status" | tail -n 5)"

default_status=$(
    run_mysql \
        "USE ${DATABASE};
         CREATE TABLE required_target(id INT PRIMARY KEY, must INT NOT NULL);
         REPLACE INTO required_target(id)
             SELECT 10 FROM DUAL WHERE EXISTS(SELECT * FROM required_target WHERE id = 99);
         SELECT ROW_COUNT(), @@warning_count, COUNT(*) FROM required_target;"
)
expect_value \
    "zero-row source omits required target" \
    "0	0	0" \
    "$(printf '%s\n' "$default_status" | tail -n 1)"

expect_error \
    "one-row source omitted required target" \
    1364 \
    HY000 \
    "Field 'must' doesn't have a default value" \
    "USE ${DATABASE}; REPLACE INTO required_target(id) SELECT 11 FROM DUAL;"

expect_error \
    "selected null into not null" \
    1048 \
    23000 \
    "Column 'must' cannot be null" \
    "USE ${DATABASE}; REPLACE INTO required_target(id, must) SELECT 12, NULL FROM DUAL;"

expect_error \
    "wildcard dual replace source" \
    1096 \
    HY000 \
    "No tables used" \
    "USE ${DATABASE}; REPLACE INTO required_target SELECT * FROM DUAL;"

keyed_status=$(
    run_mysql \
        "USE ${DATABASE};
         CREATE TABLE pk(id INT PRIMARY KEY, v INT);
         INSERT INTO pk VALUES (1, 10);
         REPLACE INTO pk SELECT 2, 20;
         SELECT ROW_COUNT(), @@warning_count, COUNT(*), GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id)
             FROM pk;
         REPLACE INTO pk SELECT 1, 30 FROM DUAL;
         SELECT ROW_COUNT(), @@warning_count, COUNT(*), GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id)
             FROM pk;
         REPLACE INTO pk SELECT 1, 30 FROM DUAL;
         SELECT ROW_COUNT(), @@warning_count, COUNT(*), GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id)
             FROM pk;"
)
expect_value \
    "primary-key row-scalar replacement" \
    "1	0	2	1:10,2:20
2	0	2	1:30,2:20
1	0	2	1:30,2:20" \
    "$(printf '%s\n' "$keyed_status" | tail -n 3)"

multi_unique_status=$(
    run_mysql \
        "USE ${DATABASE};
         CREATE TABLE multi_unique(a INT UNIQUE, b INT UNIQUE, v INT);
         INSERT INTO multi_unique VALUES (1, 10, 100), (2, 20, 200);
         REPLACE INTO multi_unique SELECT 1, 20, 300 FROM DUAL;
         SELECT ROW_COUNT(), @@warning_count, COUNT(*),
             GROUP_CONCAT(CONCAT(a, ':', b, ':', v) ORDER BY a)
         FROM multi_unique;"
)
expect_value \
    "multiple unique conflicts" \
    "3	0	1	1:20:300" \
    "$(printf '%s\n' "$multi_unique_status" | tail -n 1)"

nullable_unique_status=$(
    run_mysql \
        "USE ${DATABASE};
         CREATE TABLE nullable_unique(a INT UNIQUE, v INT);
         INSERT INTO nullable_unique VALUES (NULL, 10);
         REPLACE INTO nullable_unique SELECT NULL, 20 FROM DUAL;
         SELECT ROW_COUNT(), @@warning_count, COUNT(*),
             GROUP_CONCAT(COALESCE(CONCAT(a, ':', v), CONCAT('NULL:', v)) ORDER BY v)
         FROM nullable_unique;"
)
expect_value \
    "nullable unique does not conflict" \
    "1	0	2	NULL:10,NULL:20" \
    "$(printf '%s\n' "$nullable_unique_status" | tail -n 1)"

auto_increment_status=$(
    run_mysql \
        "USE ${DATABASE};
         CREATE TABLE auto_inc(id INT AUTO_INCREMENT PRIMARY KEY, v VARCHAR(10) UNIQUE);
         REPLACE INTO auto_inc(v) SELECT CONCAT('a') FROM DUAL;
         SELECT ROW_COUNT(), @@warning_count, LAST_INSERT_ID();
         REPLACE INTO auto_inc(id, v) SELECT NULL, 'a' FROM DUAL;
         SELECT ROW_COUNT(), @@warning_count, LAST_INSERT_ID();
         SELECT id, v FROM auto_inc;"
)
expect_value \
    "auto-increment row-scalar replacement" \
    "1	0	1
2	0	2
2	a" \
    "$(printf '%s\n' "$auto_increment_status" | tail -n 3)"

modifier_status=$(
    run_mysql \
        "USE ${DATABASE};
         CREATE TABLE modifiers(id INT PRIMARY KEY, v INT);
         REPLACE LOW_PRIORITY INTO modifiers SELECT 1, 10;
         SELECT ROW_COUNT(), @@warning_count, @@error_count,
             GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id)
         FROM modifiers;
         REPLACE DELAYED INTO modifiers SELECT 2, 20 FROM DUAL;
         SELECT ROW_COUNT(), @@warning_count, @@error_count,
             GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id)
         FROM modifiers;"
)
expect_value \
    "row-scalar replace-select modifiers" \
    "1	0	0	1:10
1	1	0	1:10,2:20" \
    "$(printf '%s\n' "$modifier_status" | tail -n 2)"

qualified_status=$(
    run_mysql \
        "CREATE DATABASE ${OTHER_DATABASE};
         CREATE TABLE ${OTHER_DATABASE}.qualified_dst(id INT PRIMARY KEY, v INT);
         REPLACE INTO ${OTHER_DATABASE}.qualified_dst(id, v) SELECT 9, 90;
         SELECT ROW_COUNT(), @@warning_count, id, v FROM ${OTHER_DATABASE}.qualified_dst;"
)
expect_value \
    "schema-qualified target without selected database" \
    "1	0	9	90" \
    "$(printf '%s\n' "$qualified_status" | tail -n 1)"

expect_error \
    "missing default schema for target" \
    1046 \
    3D000 \
    "No database selected" \
    "REPLACE INTO missing_default(id) SELECT 1;"

expect_error \
    "unknown target schema" \
    1049 \
    42000 \
    "Unknown database 'nosuch_${DATABASE}'" \
    "REPLACE INTO nosuch_${DATABASE}.dst(id) SELECT 1;"

printf '%s\n' "mysql_baseline_replace_select_row_scalar_source_expectations: ok"
