#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_long_character_binary_aliases_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_long_character_binary_aliases_expectations: $1" >&2
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
a	mediumtext	YES		NULL	
b	mediumtext	YES		NULL	
c	mediumblob	YES		NULL	
nn	mediumtext	NO		NULL	
EXPECTED
)
expect_output \
    "show columns renders normalized long aliases" \
    "$show_columns_expected" \
    "CREATE TABLE aliases (id INT, a LONG, b LONG VARCHAR, c LONG VARBINARY, nn LONG NOT NULL); "\
"SHOW COLUMNS FROM aliases;" \
    "$DATABASE"

show_create_expected=$(cat <<\EXPECTED
aliases	CREATE TABLE `aliases` (
  `id` int DEFAULT NULL,
  `a` mediumtext,
  `b` mediumtext,
  `c` mediumblob,
  `nn` mediumtext NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "show create renders normalized long aliases" \
    "$show_create_expected" \
    "SHOW CREATE TABLE aliases;" \
    "$DATABASE"

information_schema_expected=$(cat <<\EXPECTED
a	mediumtext	16777215	16777215	utf8mb4	utf8mb4_0900_ai_ci	mediumtext	YES
b	mediumtext	16777215	16777215	utf8mb4	utf8mb4_0900_ai_ci	mediumtext	YES
c	mediumblob	16777215	16777215	NULL	NULL	mediumblob	YES
nn	mediumtext	16777215	16777215	utf8mb4	utf8mb4_0900_ai_ci	mediumtext	NO
EXPECTED
)
expect_output \
    "information schema renders normalized long aliases" \
    "$information_schema_expected" \
    "SELECT COLUMN_NAME, DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, "\
"CHARACTER_SET_NAME, COLLATION_NAME, COLUMN_TYPE, IS_NULLABLE "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA='${DATABASE}' AND TABLE_NAME='aliases' AND COLUMN_NAME <> 'id' "\
"ORDER BY ORDINAL_POSITION;" \
    "$DATABASE"

insert_readback_expected=$(cat <<\EXPECTED
1	text	body	4142	[nn]
2	NULL	NULL	NULL	[]
EXPECTED
)
expect_output \
    "insert and read back normalized long alias values" \
    "$insert_readback_expected" \
    "INSERT INTO aliases VALUES (1, 'text', 'body', X'4142', 'nn'), "\
"(2, NULL, NULL, NULL, ''); "\
"SELECT id, IF(a IS NULL, 'NULL', a), IF(b IS NULL, 'NULL', b), "\
"IF(c IS NULL, 'NULL', HEX(c)), IF(nn IS NULL, 'NULL', CONCAT('[', nn, ']')) "\
"FROM aliases ORDER BY id;" \
    "$DATABASE"

update_expected=$(cat <<\EXPECTED
1	0	[updated]	43
0	0
EXPECTED
)
expect_output \
    "update normalized long alias values" \
    "$update_expected" \
    "UPDATE aliases SET b = 'updated', c = X'43' WHERE id = 1; "\
"SELECT ROW_COUNT(), @@warning_count, CONCAT('[', b, ']'), HEX(c) FROM aliases WHERE id = 1; "\
"UPDATE aliases SET b = 'updated', c = X'43' WHERE id = 1; "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

show_columns_after_add_expected=$(cat <<\EXPECTED
added	mediumtext	NO		NULL	
added_blob	mediumblob	YES		NULL	
EXPECTED
)
expect_output \
    "alter add column normalizes long aliases" \
    "$show_columns_after_add_expected" \
    "ALTER TABLE aliases ADD COLUMN added LONG VARCHAR NOT NULL; "\
"ALTER TABLE aliases ADD COLUMN added_blob LONG VARBINARY; "\
"SHOW COLUMNS FROM aliases WHERE Field IN ('added', 'added_blob');" \
    "$DATABASE"

clone_expected=$(cat <<\EXPECTED
like_aliases	CREATE TABLE `like_aliases` (
  `id` int DEFAULT NULL,
  `a` mediumtext,
  `b` mediumtext,
  `c` mediumblob,
  `nn` mediumtext NOT NULL,
  `added` mediumtext NOT NULL,
  `added_blob` mediumblob
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
ctas_aliases	CREATE TABLE `ctas_aliases` (
  `a` mediumtext,
  `b` mediumtext,
  `c` mediumblob
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "clone and ctas preserve normalized long aliases" \
    "$clone_expected" \
    "CREATE TABLE like_aliases LIKE aliases; "\
"CREATE TABLE ctas_aliases AS SELECT a, b, c FROM aliases; "\
"SHOW CREATE TABLE like_aliases; SHOW CREATE TABLE ctas_aliases;" \
    "$DATABASE"

attrs_expected=$(cat <<\EXPECTED
attrs	CREATE TABLE `attrs` (
  `a` mediumtext CHARACTER SET utf8mb4 COLLATE utf8mb4_bin,
  `b` mediumtext CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci,
  `c` mediumblob NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "long text aliases accept text charset attributes" \
    "$attrs_expected" \
    "CREATE TABLE attrs (a LONG CHARACTER SET utf8mb4 COLLATE utf8mb4_bin, "\
"b LONG VARCHAR CHARACTER SET utf8mb4, c LONG VARBINARY NOT NULL); "\
"SHOW CREATE TABLE attrs;" \
    "$DATABASE"

expect_error \
    "long varchar length is syntax error" \
    1064 \
    42000 \
    "near '(10))'" \
    "CREATE TABLE bad_long_varchar_len (a LONG VARCHAR(10));" \
    "$DATABASE"

expect_error \
    "long varbinary length is syntax error" \
    1064 \
    42000 \
    "near '(10))'" \
    "CREATE TABLE bad_long_varbinary_len (a LONG VARBINARY(10));" \
    "$DATABASE"

expect_error \
    "long text is syntax error" \
    1064 \
    42000 \
    "near 'TEXT)'" \
    "CREATE TABLE bad_long_text (a LONG TEXT);" \
    "$DATABASE"

expect_error \
    "literal long text default is rejected" \
    1101 \
    42000 \
    "can't have a default value" \
    "CREATE TABLE bad_long_default_literal (a LONG DEFAULT 'abc');" \
    "$DATABASE"

expect_error \
    "literal long varbinary default is rejected" \
    1101 \
    42000 \
    "can't have a default value" \
    "CREATE TABLE bad_long_varbinary_default_literal (a LONG VARBINARY DEFAULT X'41');" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts long binary collation alias" \
    "CREATE TABLE long_binary (a LONG BINARY); SHOW CREATE TABLE long_binary;" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts deferred long expression default" \
    "CREATE TABLE deferred_long_default_expr (a LONG DEFAULT ('abc'));" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts deferred long varbinary expression default" \
    "CREATE TABLE deferred_long_varbinary_default_expr (a LONG VARBINARY DEFAULT (X'41'));" \
    "$DATABASE"
