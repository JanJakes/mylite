#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_json_type_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_json_type_expectations: $1" >&2
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
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

show_columns_expected=$(cat <<\EXPECTED
id	int	YES		NULL	
j	json	YES		NULL	
nn	json	NO		NULL	
EXPECTED
)
expect_output \
    "show columns renders JSON descriptors" \
    "$show_columns_expected" \
    "CREATE TABLE jt (id INT, j JSON, nn JSON NOT NULL); SHOW COLUMNS FROM jt;" \
    "$DATABASE"

show_create_expected=$(cat <<\EXPECTED
jt	CREATE TABLE `jt` (
  `id` int DEFAULT NULL,
  `j` json DEFAULT NULL,
  `nn` json NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "show create renders JSON descriptors" \
    "$show_create_expected" \
    "SHOW CREATE TABLE jt;" \
    "$DATABASE"

information_schema_expected=$(cat <<\EXPECTED
id	int	int	NULL	NULL	NULL	NULL	YES	NULL
j	json	json	NULL	NULL	NULL	NULL	YES	NULL
nn	json	json	NULL	NULL	NULL	NULL	NO	NULL
EXPECTED
)
expect_output \
    "information schema renders JSON metadata" \
    "$information_schema_expected" \
    "SELECT COLUMN_NAME, DATA_TYPE, COLUMN_TYPE, CHARACTER_MAXIMUM_LENGTH, "\
"CHARACTER_OCTET_LENGTH, CHARACTER_SET_NAME, COLLATION_NAME, IS_NULLABLE, "\
"COLUMN_DEFAULT FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA='${DATABASE}' "\
"AND TABLE_NAME='jt' ORDER BY ORDINAL_POSITION;" \
    "$DATABASE"

json_values_expected=$(cat <<\EXPECTED
1	{"a": 1, "b": 2}	{"x": 2}	0	0
2	null	true	0	0
3	NULL	false	1	0
4	[1, 2, 3]	123	0	0
5	"hello"	"world"	0	0
0	-1
EXPECTED
)
expect_output \
    "insert and read canonical JSON values" \
    "$json_values_expected" \
    "INSERT INTO jt VALUES (1, '{\"b\":2,\"a\":1}', '{\"x\":1,\"x\":2}'); "\
"INSERT INTO jt VALUES (2, 'null', 'true'), (3, NULL, 'false'), "\
"(4, '[1,2,3]', '123'), (5, '\"hello\"', '\"world\"'); "\
"SELECT id, j, nn, j IS NULL, nn IS NULL FROM jt ORDER BY id; "\
"SELECT @@warning_count, ROW_COUNT();" \
    "$DATABASE"

update_expected=$(cat <<\EXPECTED
1	0
0	0
1	0
1	{"updated": 1}
2	null
3	NULL
4	[1, 2, 3]
5	"hello"
EXPECTED
)
expect_output \
    "update JSON affected rows use canonical values" \
    "$update_expected" \
    "UPDATE jt SET j='{\"updated\":1}' WHERE id=1; SELECT ROW_COUNT(), @@warning_count; "\
"UPDATE jt SET j='{\"updated\":1}' WHERE id=1; SELECT ROW_COUNT(), @@warning_count; "\
"UPDATE jt SET j=NULL WHERE id=1; SELECT ROW_COUNT(), @@warning_count; "\
"UPDATE jt SET j='{\"updated\":1}' WHERE id=1; "\
"SELECT id, j FROM jt ORDER BY id;" \
    "$DATABASE"

alter_add_expected=$(cat <<\EXPECTED
id	json	YES		NULL	
j	json	YES		NULL	
nn	json	NO		NULL	
1	NULL	null
EXPECTED
)
expect_output \
    "alter add JSON backfills nullable and not null columns" \
    "$alter_add_expected" \
    "CREATE TABLE alter_json (id JSON); INSERT INTO alter_json VALUES ('1'); "\
"ALTER TABLE alter_json ADD COLUMN j JSON; "\
"ALTER TABLE alter_json ADD COLUMN nn JSON NOT NULL; "\
"SHOW COLUMNS FROM alter_json; SELECT id, j, nn FROM alter_json;" \
    "$DATABASE"

ignore_expected=$(cat <<\EXPECTED
1	null	1	1
EXPECTED
)
expect_output \
    "insert ignore omitted JSON NOT NULL stores JSON null" \
    "$ignore_expected" \
    "CREATE TABLE ignore_json (id INT, j JSON NOT NULL); "\
"INSERT IGNORE INTO ignore_json(id) VALUES(1); "\
"SELECT id, j, @@warning_count, ROW_COUNT() FROM ignore_json;" \
    "$DATABASE"

predicate_expected=$(cat <<\EXPECTED
3
1
2
4
5
EXPECTED
)
expect_output \
    "JSON IS NULL predicates work" \
    "$predicate_expected" \
    "SELECT id FROM jt WHERE j IS NULL ORDER BY id; "\
"SELECT id FROM jt WHERE j IS NOT NULL ORDER BY id;" \
    "$DATABASE"

expect_error \
    "invalid JSON text" \
    "3140" \
    "22032" \
    "Invalid JSON text" \
    "INSERT INTO jt VALUES (6, 'bad', '{}');" \
    "$DATABASE"

expect_error \
    "invalid JSON fragment" \
    "3140" \
    "22032" \
    "Invalid JSON text" \
    "INSERT INTO jt VALUES (7, '[1,2,', '{}');" \
    "$DATABASE"

expect_error \
    "JSON NOT NULL rejects SQL NULL" \
    "1048" \
    "23000" \
    "Column 'nn' cannot be null" \
    "INSERT INTO jt VALUES (8, '{}', NULL);" \
    "$DATABASE"

expect_error \
    "JSON NOT NULL DEFAULT NULL is invalid" \
    "1067" \
    "42000" \
    "Invalid default value for 'j'" \
    "CREATE TABLE json_not_null_default (j JSON NOT NULL DEFAULT NULL);" \
    "$DATABASE"

expect_error \
    "JSON bare literal default is invalid" \
    "1101" \
    "42000" \
    "can't have a default value" \
    "CREATE TABLE json_bare_default (j JSON DEFAULT '{}');" \
    "$DATABASE"

expect_upstream_accepts \
    "JSON expression defaults are deferred by MyLite" \
    "CREATE TABLE json_expr_default (j JSON DEFAULT ('{}')); INSERT INTO json_expr_default VALUES();" \
    "$DATABASE"

expect_error \
    "direct primary key on JSON is rejected" \
    "3152" \
    "42000" \
    "supports indexing only via generated columns" \
    "CREATE TABLE json_pk (j JSON PRIMARY KEY);" \
    "$DATABASE"

expect_error \
    "direct secondary key on JSON is rejected" \
    "3152" \
    "42000" \
    "supports indexing only via generated columns" \
    "CREATE TABLE json_key (j JSON, KEY k(j));" \
    "$DATABASE"
