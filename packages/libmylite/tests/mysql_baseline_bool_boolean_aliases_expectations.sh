#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_bool_boolean_aliases_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_bool_boolean_aliases_expectations: $1" >&2
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

case "$(run_mysql 'SELECT @@sql_mode;')" in
    *STRICT_TRANS_TABLES*) ;;
    *) fail "expected strict default sql_mode" ;;
esac

expect_output \
    "bool aliases emit no warnings" \
    "" \
    "CREATE TABLE bools (b BOOL, c BOOLEAN, nn BOOL NOT NULL); SHOW WARNINGS;" \
    "$DATABASE"

show_columns_expected=$(printf '%b' \
    'b\ttinyint(1)\tYES\t\tNULL\t\n'\
'c\ttinyint(1)\tYES\t\tNULL\t\n'\
'nn\ttinyint(1)\tNO\t\tNULL\t')
expect_output \
    "show columns normalizes bool aliases" \
    "$show_columns_expected" \
    "SHOW COLUMNS FROM bools;" \
    "$DATABASE"
expect_output \
    "describe normalizes bool aliases" \
    "$show_columns_expected" \
    "DESCRIBE bools;" \
    "$DATABASE"
expect_output \
    "explain table normalizes bool aliases" \
    "$show_columns_expected" \
    "EXPLAIN bools;" \
    "$DATABASE"

show_create_expected=$(cat <<'EXPECTED'
bools	CREATE TABLE `bools` (
  `b` tinyint(1) DEFAULT NULL,
  `c` tinyint(1) DEFAULT NULL,
  `nn` tinyint(1) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "show create normalizes bool aliases" \
    "$show_create_expected" \
    "SHOW CREATE TABLE bools;" \
    "$DATABASE"

dml_expected=$(cat <<'EXPECTED'
3	0
-1	0	1
1	2	1
127	NULL	1
EXPECTED
)
expect_output \
    "bool aliases store signed tinyint values" \
    "$dml_expected" \
    "INSERT INTO bools VALUES (-1,0,1),(1,2,1),(127,NULL,1); "\
"SELECT ROW_COUNT(), @@warning_count; SELECT b,c,nn FROM bools ORDER BY b;" \
    "$DATABASE"

update_expected=$(cat <<'EXPECTED'
1	0
-1	0
1	127
127	NULL
EXPECTED
)
expect_output \
    "bool aliases update through tinyint descriptor" \
    "$update_expected" \
    "UPDATE bools SET c = 127 WHERE b = 1; "\
"SELECT ROW_COUNT(), @@warning_count; SELECT b,c FROM bools ORDER BY b;" \
    "$DATABASE"

predicate_order_expected=$(cat <<'EXPECTED'
127
1
EXPECTED
)
expect_output \
    "bool aliases reuse predicate order and limit behavior" \
    "$predicate_order_expected" \
    "SELECT b FROM bools WHERE b IS NOT NULL ORDER BY b DESC LIMIT 2;" \
    "$DATABASE"

expect_output \
    "bool alias delete reports affected rows" \
    "1	0" \
    "DELETE FROM bools WHERE b = -1; SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_error \
    "bool alias high out of range" \
    1264 \
    22003 \
    "Out of range value for column 'b' at row 1" \
    "INSERT INTO bools VALUES (128,0,1);" \
    "$DATABASE"

expect_error \
    "bool alias low out of range" \
    1264 \
    22003 \
    "Out of range value for column 'b' at row 1" \
    "INSERT INTO bools VALUES (-129,0,1);" \
    "$DATABASE"

expect_error \
    "bool not null rejects null" \
    1048 \
    23000 \
    "Column 'nn' cannot be null" \
    "INSERT INTO bools VALUES (0,0,NULL);" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE alter_bools (a TINYINT(1)); INSERT INTO alter_bools VALUES (1),(2);" \
    "$DATABASE" >/dev/null

alter_modify_expected=$(printf '%b' \
    '0\t0\n'\
'a\ttinyint(1)\tYES\t\tNULL\t\n'\
'1\n'\
'2')
expect_output \
    "alter modify bool is metadata-equivalent" \
    "$alter_modify_expected" \
    "ALTER TABLE alter_bools MODIFY a BOOL; SELECT ROW_COUNT(), @@warning_count; "\
"SHOW COLUMNS FROM alter_bools; SELECT a FROM alter_bools ORDER BY a;" \
    "$DATABASE"

alter_change_expected=$(printf '%b' \
    '0\t0\n'\
'flag\ttinyint(1)\tNO\t\tNULL\t')
expect_output \
    "alter change boolean normalizes descriptor" \
    "$alter_change_expected" \
    "ALTER TABLE alter_bools CHANGE a flag BOOLEAN NOT NULL; "\
"SELECT ROW_COUNT(), @@warning_count; SHOW COLUMNS FROM alter_bools;" \
    "$DATABASE"

alter_add_expected=$(printf '%b' \
    '0\t0\n'\
'flag\ttinyint(1)\tNO\t\tNULL\t\n'\
'added\ttinyint(1)\tNO\t\tNULL\t\n'\
'1\t0\n'\
'2\t0')
expect_output \
    "alter add bool backfills not null rows" \
    "$alter_add_expected" \
    "ALTER TABLE alter_bools ADD added BOOL NOT NULL; "\
"SELECT ROW_COUNT(), @@warning_count; SHOW COLUMNS FROM alter_bools; "\
"SELECT flag,added FROM alter_bools ORDER BY flag;" \
    "$DATABASE"

expect_error \
    "bool explicit display width is syntax error" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "CREATE TABLE bad_width (b BOOL(1));" \
    "$DATABASE"

expect_error \
    "boolean explicit display width is syntax error" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "CREATE TABLE bad_boolean_width (b BOOLEAN(1));" \
    "$DATABASE"

expect_error \
    "bool unsigned is syntax error" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "CREATE TABLE bad_unsigned (b BOOL UNSIGNED);" \
    "$DATABASE"

expect_error \
    "bool signed is syntax error" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "CREATE TABLE bad_signed (b BOOL SIGNED);" \
    "$DATABASE"

expect_error \
    "bool zerofill is syntax error" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "CREATE TABLE bad_zerofill (b BOOL ZEROFILL);" \
    "$DATABASE"

expect_error \
    "bool combined attributes are syntax error" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "CREATE TABLE bad_combined (b BOOL SIGNED UNSIGNED);" \
    "$DATABASE"

printf '%s\n' "baseline-bool-boolean-aliases MySQL 8.4.9 expectations verified"
