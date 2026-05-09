#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_explicit_default_null_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_explicit_default_null_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --batch --raw --skip-column-names "$@"
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

expect_upstream_accepts() {
    label=$1
    sql=$2
    shift 2

    set +e
    output=$(run_mysql "$sql" "$@" 2>&1)
    status_code=$?
    set -e

    if [ "$status_code" -ne 0 ]; then
        fail "$label: expected MySQL to accept deferred syntax, got [$output]"
    fi
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

create_expected=$(cat <<'EXPECTED'
defaults	CREATE TABLE `defaults` (
  `a` int DEFAULT NULL,
  `b` int DEFAULT NULL,
  `c` bigint unsigned DEFAULT NULL,
  `d` tinyint(1) DEFAULT NULL,
  `nn` int NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
N:N:N:N:7	0
EXPECTED
)
expect_output \
    "create table accepts explicit default null" \
    "$create_expected" \
    "CREATE TABLE defaults ("\
"a INT DEFAULT NULL, b INT NULL DEFAULT NULL, c BIGINT UNSIGNED DEFAULT NULL, "\
"d BOOL DEFAULT NULL, nn INT NOT NULL); "\
"SHOW CREATE TABLE defaults; "\
"INSERT INTO defaults (nn) VALUES (7); "\
"SELECT CONCAT(IFNULL(a, 'N'), ':', IFNULL(b, 'N'), ':', IFNULL(c, 'N'), ':', "\
"IFNULL(d, 'N'), ':', nn), @@warning_count FROM defaults;" \
    "$DATABASE"

expect_error \
    "not null default null is invalid in create" \
    1067 \
    42000 \
    "Invalid default value for 'bad'" \
    "CREATE TABLE create_bad (bad INT NOT NULL DEFAULT NULL);" \
    "$DATABASE"

expect_error \
    "not null default null is invalid before existing-table if-not-exists noop" \
    1067 \
    42000 \
    "Invalid default value for 'bad'" \
    "CREATE TABLE IF NOT EXISTS defaults (bad INT NOT NULL DEFAULT NULL);" \
    "$DATABASE"

expect_output \
    "existing-table if-not-exists still skips duplicate column names" \
    "0	1" \
    "CREATE TABLE IF NOT EXISTS defaults (a INT, a INT); SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

add_expected=$(cat <<'EXPECTED'
0	0
1:N,2:N
1	0
1:N,2:N,3:N
EXPECTED
)
expect_output \
    "add column default null backfills and later omits null" \
    "$add_expected" \
    "CREATE TABLE add_target (id INT NOT NULL); INSERT INTO add_target VALUES (1), (2); "\
"ALTER TABLE add_target ADD COLUMN added INT DEFAULT NULL; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', IFNULL(added, 'N')) ORDER BY id) FROM add_target; "\
"INSERT INTO add_target (id) VALUES (3); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', IFNULL(added, 'N')) ORDER BY id) FROM add_target;" \
    "$DATABASE"

expect_error \
    "not null default null is invalid in add" \
    1067 \
    42000 \
    "Invalid default value for 'bad_added'" \
    "ALTER TABLE add_target ADD COLUMN bad_added INT NOT NULL DEFAULT NULL;" \
    "$DATABASE"

modify_change_expected=$(cat <<'EXPECTED'
2	0
0	0
renamed	bigint	YES		NULL	
1:N,2:5
EXPECTED
)
expect_output \
    "modify and change accept default null" \
    "$modify_change_expected" \
    "CREATE TABLE mutate_target (id INT NOT NULL, value INT NULL); "\
"INSERT INTO mutate_target VALUES (1, NULL), (2, 5); "\
"ALTER TABLE mutate_target MODIFY value BIGINT DEFAULT NULL; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE mutate_target CHANGE value renamed BIGINT DEFAULT NULL; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW COLUMNS FROM mutate_target LIKE 'renamed'; "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', IFNULL(renamed, 'N')) ORDER BY id) FROM mutate_target;" \
    "$DATABASE"

expect_error \
    "not null default null is invalid in modify" \
    1067 \
    42000 \
    "Invalid default value for 'renamed'" \
    "ALTER TABLE mutate_target MODIFY renamed BIGINT NOT NULL DEFAULT NULL;" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts default before repeated nullability outside MyLite scope" \
    "CREATE TABLE mysql_wider_null_order (a INT DEFAULT NULL NULL);" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts non-null default outside MyLite scope" \
    "CREATE TABLE mysql_wider_non_null_default (a INT DEFAULT 0);" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts expression default outside MyLite scope" \
    "CREATE TABLE mysql_wider_expression_default (a INT DEFAULT (1 + 2));" \
    "$DATABASE"

printf '%s\n' "baseline-explicit-default-null MySQL 8.4.9 expectations verified"
