#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_integer_signed_attribute_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_integer_signed_attribute_expectations: $1" >&2
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

case "$(run_mysql 'SELECT @@sql_mode;')" in
    *STRICT_TRANS_TABLES*) ;;
    *) fail "expected strict default sql_mode" ;;
esac

signed_values_expected=$(cat <<'EXPECTED'
2	0
-128	-32768	-8388608	-2147483648	-2147483648	-9223372036854775808
127	32767	8388607	2147483647	2147483647	9223372036854775807
EXPECTED
)
expect_output \
    "signed attribute accepts signed integer family values" \
    "$signed_values_expected" \
    "CREATE TABLE ints (ti TINYINT SIGNED, si SMALLINT SIGNED, "\
"mi MEDIUMINT SIGNED, i INT SIGNED, ii INTEGER SIGNED, b BIGINT SIGNED); "\
"INSERT INTO ints VALUES "\
"(-128,-32768,-8388608,-2147483648,-2147483648,-9223372036854775808), "\
"(127,32767,8388607,2147483647,2147483647,9223372036854775807); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT * FROM ints ORDER BY ti;" \
    "$DATABASE"

show_columns_expected=$(printf '%b' \
    'ti\ttinyint\tYES\t\tNULL\t\n'\
'si\tsmallint\tYES\t\tNULL\t\n'\
'mi\tmediumint\tYES\t\tNULL\t\n'\
'i\tint\tYES\t\tNULL\t\n'\
'ii\tint\tYES\t\tNULL\t\n'\
'b\tbigint\tYES\t\tNULL\t')
expect_output \
    "show columns normalizes signed attribute away" \
    "$show_columns_expected" \
    "SHOW COLUMNS FROM ints;" \
    "$DATABASE"

show_create_expected=$(cat <<'EXPECTED'
ints	CREATE TABLE `ints` (
  `ti` tinyint DEFAULT NULL,
  `si` smallint DEFAULT NULL,
  `mi` mediumint DEFAULT NULL,
  `i` int DEFAULT NULL,
  `ii` int DEFAULT NULL,
  `b` bigint DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "show create normalizes signed attribute away" \
    "$show_create_expected" \
    "SHOW CREATE TABLE ints;" \
    "$DATABASE"

update_expected=$(cat <<'EXPECTED'
1	0	-127
EXPECTED
)
expect_output \
    "update over signed attribute column uses signed range" \
    "$update_expected" \
    "UPDATE ints SET ti = -127 WHERE ti = -128; "\
"SELECT ROW_COUNT(), @@warning_count, ti FROM ints WHERE si = -32768;" \
    "$DATABASE"

predicate_order_expected=$(cat <<'EXPECTED'
4,2
4,2
4,2
EXPECTED
)
expect_output \
    "predicates and order use normalized signed descriptors" \
    "$predicate_order_expected" \
    "CREATE TABLE pred_order (ti TINYINT SIGNED, si SMALLINT SIGNED, "\
"mi MEDIUMINT SIGNED, payload INT SIGNED); "\
"INSERT INTO pred_order VALUES (1,1,1,2),(3,3,3,4); "\
"SELECT GROUP_CONCAT(payload ORDER BY ti DESC) FROM pred_order WHERE ti >= 0; "\
"SELECT GROUP_CONCAT(payload ORDER BY si DESC) FROM pred_order WHERE si >= 0; "\
"SELECT GROUP_CONCAT(payload ORDER BY mi DESC) FROM pred_order WHERE mi >= 0;" \
    "$DATABASE"

insert_set_expected=$(cat <<'EXPECTED'
1	0	-1	-2
EXPECTED
)
expect_output \
    "insert set accepts signed attribute columns" \
    "$insert_set_expected" \
    "CREATE TABLE set_form (ti TINYINT SIGNED, si SMALLINT SIGNED); "\
"INSERT INTO set_form SET ti = -1, si = -2; "\
"SELECT ROW_COUNT(), @@warning_count, ti, si FROM set_form;" \
    "$DATABASE"

expect_error \
    "tinyint signed attribute high out of range" \
    1264 \
    22003 \
    "Out of range value for column 'ti' at row 1" \
    "INSERT INTO ints (ti) VALUES (128);" \
    "$DATABASE"

expect_error \
    "smallint signed attribute low out of range" \
    1264 \
    22003 \
    "Out of range value for column 'si' at row 1" \
    "INSERT INTO ints (si) VALUES (-32769);" \
    "$DATABASE"

expect_error \
    "mediumint signed attribute high out of range" \
    1264 \
    22003 \
    "Out of range value for column 'mi' at row 1" \
    "INSERT INTO ints (mi) VALUES (8388608);" \
    "$DATABASE"

expect_error \
    "int signed attribute high out of range" \
    1264 \
    22003 \
    "Out of range value for column 'i' at row 1" \
    "INSERT INTO ints (i) VALUES (2147483648);" \
    "$DATABASE"

expect_error \
    "bigint signed attribute high out of range" \
    1264 \
    22003 \
    "Out of range value for column 'b' at row 1" \
    "INSERT INTO ints (b) VALUES (9223372036854775808);" \
    "$DATABASE"

alter_expected=$(printf '%b' \
    '0\t0\n'\
'ti\ttinyint\tYES\t\tNULL\t\n'\
'si\tint\tYES\t\tNULL\t\n'\
'changed\tmediumint\tYES\t\tNULL\t\n'\
'i\tint\tYES\t\tNULL\t\n'\
'ii\tint\tYES\t\tNULL\t\n'\
'b\tbigint\tYES\t\tNULL\t\n'\
'added\ttinyint\tNO\t\tNULL\t')
expect_output \
    "alter add modify change accepts signed attribute" \
    "$alter_expected" \
    "ALTER TABLE ints ADD added TINYINT SIGNED NOT NULL; "\
"ALTER TABLE ints MODIFY si INT SIGNED; "\
"ALTER TABLE ints CHANGE mi changed MEDIUMINT SIGNED; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW COLUMNS FROM ints;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE alter_bad (c INT SIGNED NOT NULL); INSERT INTO alter_bad VALUES (128);" \
    "$DATABASE" >/dev/null
expect_error \
    "modify signed attribute validates existing rows" \
    1264 \
    22003 \
    "Out of range value for column 'c' at row 1" \
    "ALTER TABLE alter_bad MODIFY c TINYINT SIGNED NOT NULL;" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts signed then unsigned attribute combination" \
    "CREATE TABLE both_attr_1 (c INT SIGNED UNSIGNED);" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts unsigned then signed attribute combination" \
    "CREATE TABLE both_attr_2 (c INT UNSIGNED SIGNED);" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts repeated signed attribute" \
    "CREATE TABLE repeated_signed (c INT SIGNED SIGNED);" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts signed display width" \
    "CREATE TABLE display_width_signed (c TINYINT(1) SIGNED);" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts zerofill signed combination" \
    "CREATE TABLE zerofill_signed (c INT ZEROFILL SIGNED);" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts bool boolean aliases" \
    "CREATE TABLE bool_aliases (b BOOL, c BOOLEAN);" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts vendor integer aliases" \
    "CREATE TABLE vendor_aliases (i1 INT1, i2 INT2, i3 INT3, i4 INT4, i8 INT8);" \
    "$DATABASE"

printf '%s\n' "baseline-integer-signed-attribute MySQL 8.4.9 expectations verified"
