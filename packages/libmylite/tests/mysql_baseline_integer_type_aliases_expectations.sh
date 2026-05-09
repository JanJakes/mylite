#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_integer_type_aliases_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_integer_type_aliases_expectations: $1" >&2
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

alias_values_expected=$(cat <<'EXPECTED'
2	0
-128	-32768	-8388608	-2147483648	-9223372036854775808	0	0	0	0	0
127	32767	8388607	2147483647	9223372036854775807	255	65535	16777215	4294967295	9223372036854775807
EXPECTED
)
expect_output \
    "integer aliases accept normalized values" \
    "$alias_values_expected" \
    "CREATE TABLE aliases (i1 INT1, i2 INT2, i3 INT3, i4 INT4, i8 INT8, "\
"i1u INT1 UNSIGNED, i2u INT2 UNSIGNED, i3u INT3 UNSIGNED, "\
"i4u INT4 UNSIGNED, i8u INT8 UNSIGNED); "\
"INSERT INTO aliases VALUES "\
"(-128,-32768,-8388608,-2147483648,-9223372036854775808,0,0,0,0,0), "\
"(127,32767,8388607,2147483647,9223372036854775807,255,65535,16777215,4294967295,9223372036854775807); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT * FROM aliases ORDER BY i1;" \
    "$DATABASE"

show_columns_expected=$(printf '%b' \
    'i1\ttinyint\tYES\t\tNULL\t\n'\
'i2\tsmallint\tYES\t\tNULL\t\n'\
'i3\tmediumint\tYES\t\tNULL\t\n'\
'i4\tint\tYES\t\tNULL\t\n'\
'i8\tbigint\tYES\t\tNULL\t\n'\
'i1u\ttinyint unsigned\tYES\t\tNULL\t\n'\
'i2u\tsmallint unsigned\tYES\t\tNULL\t\n'\
'i3u\tmediumint unsigned\tYES\t\tNULL\t\n'\
'i4u\tint unsigned\tYES\t\tNULL\t\n'\
'i8u\tbigint unsigned\tYES\t\tNULL\t')
expect_output \
    "show columns normalizes integer aliases" \
    "$show_columns_expected" \
    "SHOW COLUMNS FROM aliases;" \
    "$DATABASE"

show_create_expected=$(cat <<'EXPECTED'
aliases	CREATE TABLE `aliases` (
  `i1` tinyint DEFAULT NULL,
  `i2` smallint DEFAULT NULL,
  `i3` mediumint DEFAULT NULL,
  `i4` int DEFAULT NULL,
  `i8` bigint DEFAULT NULL,
  `i1u` tinyint unsigned DEFAULT NULL,
  `i2u` smallint unsigned DEFAULT NULL,
  `i3u` mediumint unsigned DEFAULT NULL,
  `i4u` int unsigned DEFAULT NULL,
  `i8u` bigint unsigned DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "show create normalizes integer aliases" \
    "$show_create_expected" \
    "SHOW CREATE TABLE aliases;" \
    "$DATABASE"

signed_alias_expected=$(cat <<'EXPECTED'
c1	tinyint	YES		NULL	
c2	smallint	YES		NULL	
c3	mediumint	YES		NULL	
c4	int	YES		NULL	
c8	bigint	YES		NULL	
EXPECTED
)
expect_output \
    "signed aliases normalize to signed families" \
    "$signed_alias_expected" \
    "CREATE TABLE signed_aliases (c1 INT1 SIGNED, c2 INT2 SIGNED, "\
"c3 INT3 SIGNED, c4 INT4 SIGNED, c8 INT8 SIGNED); "\
"SHOW COLUMNS FROM signed_aliases;" \
    "$DATABASE"

update_expected=$(cat <<'EXPECTED'
1	0	-127
EXPECTED
)
expect_output \
    "update over alias column uses normalized range" \
    "$update_expected" \
    "UPDATE aliases SET i1 = -127 WHERE i1 = -128; "\
"SELECT ROW_COUNT(), @@warning_count, i1 FROM aliases WHERE i2 = -32768;" \
    "$DATABASE"

predicate_order_expected=$(cat <<'EXPECTED'
4,2
4,2
4,2
EXPECTED
)
expect_output \
    "predicates and order use normalized alias descriptors" \
    "$predicate_order_expected" \
    "CREATE TABLE pred_order (i1 INT1, i2 INT2, i3 INT3, payload INT4); "\
"INSERT INTO pred_order VALUES (1,1,1,2),(3,3,3,4); "\
"SELECT GROUP_CONCAT(payload ORDER BY i1 DESC) FROM pred_order WHERE i1 >= 0; "\
"SELECT GROUP_CONCAT(payload ORDER BY i2 DESC) FROM pred_order WHERE i2 >= 0; "\
"SELECT GROUP_CONCAT(payload ORDER BY i3 DESC) FROM pred_order WHERE i3 >= 0;" \
    "$DATABASE"

insert_set_expected=$(cat <<'EXPECTED'
1	0	-1	-2
EXPECTED
)
expect_output \
    "insert set accepts alias columns" \
    "$insert_set_expected" \
    "CREATE TABLE set_form (i1 INT1, i2 INT2); "\
"INSERT INTO set_form SET i1 = -1, i2 = -2; "\
"SELECT ROW_COUNT(), @@warning_count, i1, i2 FROM set_form;" \
    "$DATABASE"

expect_error \
    "int1 alias high out of range" \
    1264 \
    22003 \
    "Out of range value for column 'i1' at row 1" \
    "INSERT INTO aliases (i1) VALUES (128);" \
    "$DATABASE"

expect_error \
    "int2 alias low out of range" \
    1264 \
    22003 \
    "Out of range value for column 'i2' at row 1" \
    "INSERT INTO aliases (i2) VALUES (-32769);" \
    "$DATABASE"

expect_error \
    "int3 alias high out of range" \
    1264 \
    22003 \
    "Out of range value for column 'i3' at row 1" \
    "INSERT INTO aliases (i3) VALUES (8388608);" \
    "$DATABASE"

expect_error \
    "int4 alias high out of range" \
    1264 \
    22003 \
    "Out of range value for column 'i4' at row 1" \
    "INSERT INTO aliases (i4) VALUES (2147483648);" \
    "$DATABASE"

expect_error \
    "int8 alias high out of range" \
    1264 \
    22003 \
    "Out of range value for column 'i8' at row 1" \
    "INSERT INTO aliases (i8) VALUES (9223372036854775808);" \
    "$DATABASE"

alter_expected=$(printf '%b' \
    '0\t0\n'\
'i1\ttinyint\tYES\t\tNULL\t\n'\
'i2\tint\tYES\t\tNULL\t\n'\
'changed\tmediumint\tYES\t\tNULL\t\n'\
'i4\tint\tYES\t\tNULL\t\n'\
'i8\tbigint\tYES\t\tNULL\t\n'\
'i1u\ttinyint unsigned\tYES\t\tNULL\t\n'\
'i2u\tsmallint unsigned\tYES\t\tNULL\t\n'\
'i3u\tmediumint unsigned\tYES\t\tNULL\t\n'\
'i4u\tint unsigned\tYES\t\tNULL\t\n'\
'i8u\tbigint unsigned\tYES\t\tNULL\t\n'\
'added\ttinyint\tNO\t\tNULL\t')
expect_output \
    "alter add modify change accepts integer aliases" \
    "$alter_expected" \
    "ALTER TABLE aliases ADD added INT1 NOT NULL; "\
"ALTER TABLE aliases MODIFY i2 INT4; "\
"ALTER TABLE aliases CHANGE i3 changed INT3; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW COLUMNS FROM aliases;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE alter_bad (c INT4 NOT NULL); INSERT INTO alter_bad VALUES (128);" \
    "$DATABASE" >/dev/null
expect_error \
    "modify alias validates existing rows" \
    1264 \
    22003 \
    "Out of range value for column 'c' at row 1" \
    "ALTER TABLE alter_bad MODIFY c INT1 NOT NULL;" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts int1 display width" \
    "CREATE TABLE alias_width (c INT1(1));" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts int2 zerofill" \
    "CREATE TABLE alias_zerofill (c INT2 ZEROFILL);" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts bool boolean aliases" \
    "CREATE TABLE bool_aliases (b BOOL, c BOOLEAN);" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts serial alias" \
    "CREATE TABLE serial_alias (s SERIAL);" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts signed unsigned alias attribute list" \
    "CREATE TABLE signed_unsigned_alias_attr (c INT1 SIGNED UNSIGNED);" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts unsigned signed alias attribute list" \
    "CREATE TABLE unsigned_signed_alias_attr (c INT1 UNSIGNED SIGNED);" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts repeated alias attribute" \
    "CREATE TABLE repeated_alias_attr (c INT1 UNSIGNED UNSIGNED);" \
    "$DATABASE"

printf '%s\n' "baseline-integer-type-aliases MySQL 8.4.9 expectations verified"
