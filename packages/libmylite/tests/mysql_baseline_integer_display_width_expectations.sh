#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_integer_display_width_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_integer_display_width_expectations: $1" >&2
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

display_width_warnings() {
    count=$1
    index=0
    output=

    while [ "$index" -lt "$count" ]; do
        line='Warning	1681	Integer display width is deprecated and will be removed in a future release.'
        if [ -z "$output" ]; then
            output=$line
        else
            output="${output}
${line}"
        fi
        index=$((index + 1))
    done

    printf '%s' "$output"
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
    "create table emits one warning per integer display width" \
    "$(display_width_warnings 16)" \
    "CREATE TABLE widths ("\
"ti0 TINYINT(0), ti1 TINYINT(1), ti2 TINYINT(2), "\
"si SMALLINT(5), mi MEDIUMINT(9), i INT(11), ii INTEGER(10), "\
"bi BIGINT(20), iu INT(10) UNSIGNED, tis TINYINT(1) SIGNED, "\
"tiu TINYINT(1) UNSIGNED, i1 INT1(1), i2 INT2(5), i3 INT3(7), "\
"i4 INT4(9), i8 INT8(20)); SHOW WARNINGS;" \
    "$DATABASE"

show_columns_expected=$(printf '%b' \
    'ti0\ttinyint\tYES\t\tNULL\t\n'\
'ti1\ttinyint(1)\tYES\t\tNULL\t\n'\
'ti2\ttinyint\tYES\t\tNULL\t\n'\
'si\tsmallint\tYES\t\tNULL\t\n'\
'mi\tmediumint\tYES\t\tNULL\t\n'\
'i\tint\tYES\t\tNULL\t\n'\
'ii\tint\tYES\t\tNULL\t\n'\
'bi\tbigint\tYES\t\tNULL\t\n'\
'iu\tint unsigned\tYES\t\tNULL\t\n'\
'tis\ttinyint(1)\tYES\t\tNULL\t\n'\
'tiu\ttinyint unsigned\tYES\t\tNULL\t\n'\
'i1\ttinyint(1)\tYES\t\tNULL\t\n'\
'i2\tsmallint\tYES\t\tNULL\t\n'\
'i3\tmediumint\tYES\t\tNULL\t\n'\
'i4\tint\tYES\t\tNULL\t\n'\
'i8\tbigint\tYES\t\tNULL\t')
expect_output \
    "show columns normalizes display widths" \
    "$show_columns_expected" \
    "SHOW COLUMNS FROM widths;" \
    "$DATABASE"

show_create_expected=$(cat <<'EXPECTED'
widths	CREATE TABLE `widths` (
  `ti0` tinyint DEFAULT NULL,
  `ti1` tinyint(1) DEFAULT NULL,
  `ti2` tinyint DEFAULT NULL,
  `si` smallint DEFAULT NULL,
  `mi` mediumint DEFAULT NULL,
  `i` int DEFAULT NULL,
  `ii` int DEFAULT NULL,
  `bi` bigint DEFAULT NULL,
  `iu` int unsigned DEFAULT NULL,
  `tis` tinyint(1) DEFAULT NULL,
  `tiu` tinyint unsigned DEFAULT NULL,
  `i1` tinyint(1) DEFAULT NULL,
  `i2` smallint DEFAULT NULL,
  `i3` mediumint DEFAULT NULL,
  `i4` int DEFAULT NULL,
  `i8` bigint DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "show create normalizes display widths" \
    "$show_create_expected" \
    "SHOW CREATE TABLE widths;" \
    "$DATABASE"

values_expected=$(cat <<'EXPECTED'
1	0
-1	1	2	3	4	5	6	7	8	9	10	11	12	13	14	15
EXPECTED
)
expect_output \
    "display width does not affect stored values" \
    "$values_expected" \
    "INSERT INTO widths VALUES (-1,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15); "\
"SELECT ROW_COUNT(), @@warning_count; SELECT * FROM widths;" \
    "$DATABASE"

expect_error \
    "display width high out of range" \
    1439 \
    42000 \
    "Display width out of range for column 'c' (max = 255)" \
    "CREATE TABLE width_256 (c INT(256));" \
    "$DATABASE"

expect_error \
    "signed positive display width is syntax error" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "CREATE TABLE width_plus (c INT(+1));" \
    "$DATABASE"

expect_error \
    "signed negative display width is syntax error" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "CREATE TABLE width_minus (c INT(-1));" \
    "$DATABASE"

expect_error \
    "empty display width is syntax error" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "CREATE TABLE width_empty (c INT());" \
    "$DATABASE"

expect_error \
    "display width after unsigned is syntax error" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "CREATE TABLE width_after_unsigned (c INT UNSIGNED(1));" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE alter_widths (a TINYINT, b TINYINT(1), c INT); INSERT INTO alter_widths VALUES (1,1,1),(2,2,2);" \
    "$DATABASE" >/dev/null

expect_output \
    "alter modify tinyint width emits warning" \
    "$(display_width_warnings 1)" \
    "ALTER TABLE alter_widths MODIFY a TINYINT(1); SHOW WARNINGS;" \
    "$DATABASE"

alter_after_first_modify=$(printf '%b' \
    'a\ttinyint(1)\tYES\t\tNULL\t\n'\
'b\ttinyint(1)\tYES\t\tNULL\t\n'\
'c\tint\tYES\t\tNULL\t')
expect_output \
    "alter modify stores tinyint width metadata" \
    "$alter_after_first_modify" \
    "SHOW COLUMNS FROM alter_widths;" \
    "$DATABASE"

expect_output \
    "alter remove tinyint width emits no warning" \
    "" \
    "ALTER TABLE alter_widths MODIFY b TINYINT; SHOW WARNINGS;" \
    "$DATABASE"

expect_output \
    "alter change int width emits warning" \
    "$(display_width_warnings 1)" \
    "ALTER TABLE alter_widths CHANGE c c2 INT(1); SHOW WARNINGS;" \
    "$DATABASE"

alter_final_columns=$(printf '%b' \
    'a\ttinyint(1)\tYES\t\tNULL\t\n'\
'b\ttinyint\tYES\t\tNULL\t\n'\
'c2\tint\tYES\t\tNULL\t')
expect_output \
    "alter change normalizes non-tinyint width" \
    "$alter_final_columns" \
    "SHOW COLUMNS FROM alter_widths;" \
    "$DATABASE"

expect_output \
    "display width alter reports zero affected rows" \
    "0	1" \
    "CREATE TABLE rowcount_widths (a TINYINT); INSERT INTO rowcount_widths VALUES (1),(2); "\
"ALTER TABLE rowcount_widths MODIFY a TINYINT(1); SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts display width zerofill" \
    "CREATE TABLE width_zerofill (c INT(5) ZEROFILL);" \
    "$DATABASE"

printf '%s\n' "baseline-integer-display-width MySQL 8.4.9 expectations verified"

