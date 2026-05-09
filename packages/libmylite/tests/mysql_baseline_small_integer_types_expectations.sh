#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_small_integer_types_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_small_integer_types_expectations: $1" >&2
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

small_types_expected=$(cat <<'EXPECTED'
2	0
-128	255	-32768	65535	-8388608	16777215
127	0	32767	0	8388607	0
EXPECTED
)
expect_output \
    "boundary values for small integer families" \
    "$small_types_expected" \
    "CREATE TABLE ints (ti TINYINT, tiu TINYINT UNSIGNED, si SMALLINT, "\
"siu SMALLINT UNSIGNED, mi MEDIUMINT, miu MEDIUMINT UNSIGNED); "\
"INSERT INTO ints VALUES "\
"(-128,255,-32768,65535,-8388608,16777215), "\
"(127,0,32767,0,8388607,0); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT * FROM ints ORDER BY ti;" \
    "$DATABASE"

show_columns_expected=$(printf '%b' \
    'ti\ttinyint\tYES\t\tNULL\t\n'\
'tiu\ttinyint unsigned\tYES\t\tNULL\t\n'\
'si\tsmallint\tYES\t\tNULL\t\n'\
'siu\tsmallint unsigned\tYES\t\tNULL\t\n'\
'mi\tmediumint\tYES\t\tNULL\t\n'\
'miu\tmediumint unsigned\tYES\t\tNULL\t')
expect_output \
    "show columns renders small integer families" \
    "$show_columns_expected" \
    "SHOW COLUMNS FROM ints;" \
    "$DATABASE"

show_create_expected=$(cat <<'EXPECTED'
ints	CREATE TABLE `ints` (
  `ti` tinyint DEFAULT NULL,
  `tiu` tinyint unsigned DEFAULT NULL,
  `si` smallint DEFAULT NULL,
  `siu` smallint unsigned DEFAULT NULL,
  `mi` mediumint DEFAULT NULL,
  `miu` mediumint unsigned DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "show create renders small integer families" \
    "$show_create_expected" \
    "SHOW CREATE TABLE ints;" \
    "$DATABASE"

expect_error \
    "tinyint signed high out of range" \
    1264 \
    22003 \
    "Out of range value for column 'ti' at row 1" \
    "INSERT INTO ints (ti) VALUES (128);" \
    "$DATABASE"

expect_error \
    "tinyint signed low out of range" \
    1264 \
    22003 \
    "Out of range value for column 'ti' at row 1" \
    "INSERT INTO ints (ti) VALUES (-129);" \
    "$DATABASE"

expect_error \
    "tinyint unsigned negative out of range" \
    1264 \
    22003 \
    "Out of range value for column 'tiu' at row 1" \
    "INSERT INTO ints (tiu) VALUES (-1);" \
    "$DATABASE"

expect_error \
    "tinyint unsigned high out of range" \
    1264 \
    22003 \
    "Out of range value for column 'tiu' at row 1" \
    "INSERT INTO ints (tiu) VALUES (256);" \
    "$DATABASE"

expect_error \
    "smallint signed high out of range" \
    1264 \
    22003 \
    "Out of range value for column 'si' at row 1" \
    "INSERT INTO ints (si) VALUES (32768);" \
    "$DATABASE"

expect_error \
    "smallint unsigned high out of range" \
    1264 \
    22003 \
    "Out of range value for column 'siu' at row 1" \
    "INSERT INTO ints (siu) VALUES (65536);" \
    "$DATABASE"

expect_error \
    "mediumint signed high out of range" \
    1264 \
    22003 \
    "Out of range value for column 'mi' at row 1" \
    "INSERT INTO ints (mi) VALUES (8388608);" \
    "$DATABASE"

expect_error \
    "mediumint unsigned high out of range" \
    1264 \
    22003 \
    "Out of range value for column 'miu' at row 1" \
    "INSERT INTO ints (miu) VALUES (16777216);" \
    "$DATABASE"

update_expected=$(cat <<'EXPECTED'
1	0	127
EXPECTED
)
expect_output \
    "update assignment changes small integer row" \
    "$update_expected" \
    "UPDATE ints SET ti = 127 WHERE ti = -128; "\
"SELECT ROW_COUNT(), @@warning_count, ti FROM ints WHERE tiu = 255;" \
    "$DATABASE"

expect_error \
    "update assignment out of range" \
    1264 \
    22003 \
    "Out of range value for column 'ti' at row 1" \
    "UPDATE ints SET ti = 128;" \
    "$DATABASE"

predicate_order_expected=$(cat <<'EXPECTED'
4,2
4,2
4,2
EXPECTED
)
expect_output \
    "predicate and order over small integer families" \
    "$predicate_order_expected" \
    "CREATE TABLE pred_order (ti TINYINT, tiu TINYINT UNSIGNED, si SMALLINT, "\
"siu SMALLINT UNSIGNED, mi MEDIUMINT, miu MEDIUMINT UNSIGNED); "\
"INSERT INTO pred_order VALUES (1,2,1,2,1,2),(3,4,3,4,3,4); "\
"SELECT GROUP_CONCAT(tiu ORDER BY ti DESC) FROM pred_order WHERE ti >= 0; "\
"SELECT GROUP_CONCAT(siu ORDER BY si DESC) FROM pred_order WHERE si >= 0; "\
"SELECT GROUP_CONCAT(miu ORDER BY mi DESC) FROM pred_order WHERE mi >= 0;" \
    "$DATABASE"

expect_output \
    "mysql accepts comparison literal outside assignment range" \
    "0" \
    "SELECT COUNT(*) FROM ints WHERE ti = 128;" \
    "$DATABASE"

insert_set_expected=$(cat <<'EXPECTED'
1	0	-1	65535
EXPECTED
)
expect_output \
    "insert set with small integer families" \
    "$insert_set_expected" \
    "CREATE TABLE set_form (ti TINYINT, siu SMALLINT UNSIGNED); "\
"INSERT INTO set_form SET ti = -1, siu = 65535; "\
"SELECT ROW_COUNT(), @@warning_count, ti, siu FROM set_form;" \
    "$DATABASE"

modify_expected=$(printf '%b' '2\t0\nc\ttinyint\tNO\t\tNULL\t')
expect_output \
    "modify to tinyint validates existing rows" \
    "$modify_expected" \
    "CREATE TABLE alter_ranges (c INT NOT NULL); "\
"INSERT INTO alter_ranges VALUES (127),(-128); "\
"ALTER TABLE alter_ranges MODIFY c TINYINT NOT NULL; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW COLUMNS FROM alter_ranges;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE alter_bad (c INT NOT NULL); INSERT INTO alter_bad VALUES (128);" \
    "$DATABASE" >/dev/null
expect_error \
    "modify to tinyint rejects existing out-of-range row" \
    1264 \
    22003 \
    "Out of range value for column 'c' at row 1" \
    "ALTER TABLE alter_bad MODIFY c TINYINT NOT NULL;" \
    "$DATABASE"

change_expected=$(printf '%b' \
    '2\t0\n'\
'changed\tmediumint unsigned\tYES\t\tNULL\t\n'\
'1,16777215')
expect_output \
    "change to mediumint unsigned preserves rows" \
    "$change_expected" \
    "CREATE TABLE change_ranges (c INT UNSIGNED); "\
"INSERT INTO change_ranges VALUES (1),(16777215); "\
"ALTER TABLE change_ranges CHANGE c changed MEDIUMINT UNSIGNED; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW COLUMNS FROM change_ranges; "\
"SELECT GROUP_CONCAT(changed ORDER BY changed) FROM change_ranges;" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts signed integer attribute" \
    "CREATE TABLE signed_attr (c TINYINT SIGNED);" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts display width" \
    "CREATE TABLE display_width (c SMALLINT(3));" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts zerofill" \
    "CREATE TABLE zerofill_attr (c MEDIUMINT ZEROFILL);" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts bool boolean aliases" \
    "CREATE TABLE bool_aliases (b BOOL, c BOOLEAN);" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts vendor integer aliases" \
    "CREATE TABLE vendor_aliases (i1 INT1, i2 INT2, i3 INT3, i4 INT4, i8 INT8);" \
    "$DATABASE"

printf '%s\n' "baseline-small-integer-types MySQL 8.4.9 expectations verified"
