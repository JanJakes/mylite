#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_set_type_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_set_type_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
}

run_mysql_type_info() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --default-character-set=utf8mb4 --column-type-info -vvv "$@"
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

expect_contains() {
    label=$1
    needle=$2
    haystack=$3

    case "$haystack" in
        *"$needle"*) ;;
        *) fail "$label: expected output to contain [$needle]" ;;
    esac
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
        fail "$label: expected MySQL to accept behavior, got [$output]"
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

case "$(run_mysql "SELECT @@sql_mode;")" in
    *STRICT_TRANS_TABLES*) ;;
    *) fail "expected strict default sql_mode" ;;
esac

cleanup
run_mysql \
    "CREATE DATABASE ${DATABASE} DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci;" \
    >/dev/null

show_columns_expected=$(printf '%b\n' \
    "id\tint\tNO\tPRI\tNULL\tauto_increment" \
    "flags\tset('a','b','c')\tNO\t\t\t" \
    "nullable_flags\tset('b','a')\tYES\t\tNULL\t" \
    "spaced\tset('x','y')\tYES\t\ty\t" \
    "numericish\tset('0','1','2')\tYES\t\t2\t"
)
expect_output \
    "set descriptors render normalized columns" \
    "$show_columns_expected" \
    "CREATE TABLE set_values ("\
"id INT NOT NULL AUTO_INCREMENT PRIMARY KEY, "\
"flags SET('a','b','c') NOT NULL DEFAULT '', "\
"nullable_flags SET('b','a') NULL, "\
"spaced SET('x ','y  ') DEFAULT 'y', "\
"numericish SET('0','1','2') DEFAULT '2'"\
"); "\
"SHOW COLUMNS FROM set_values;" \
    "$DATABASE"

metadata_expected=$(cat <<\EXPECTED
flags	set	set('a','b','c')	5	20	NO		utf8mb4	utf8mb4_0900_ai_ci
nullable_flags	set	set('b','a')	3	12	YES	NULL	utf8mb4	utf8mb4_0900_ai_ci
numericish	set	set('0','1','2')	5	20	YES	2	utf8mb4	utf8mb4_0900_ai_ci
spaced	set	set('x','y')	3	12	YES	y	utf8mb4	utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "set information schema metadata" \
    "$metadata_expected" \
    "SELECT COLUMN_NAME, DATA_TYPE, COLUMN_TYPE, CHARACTER_MAXIMUM_LENGTH, "\
"CHARACTER_OCTET_LENGTH, IS_NULLABLE, COLUMN_DEFAULT, CHARACTER_SET_NAME, COLLATION_NAME "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'set_values' "\
"AND COLUMN_NAME <> 'id' ORDER BY COLUMN_NAME;" \
    "$DATABASE"

type_info_output=$(run_mysql_type_info "SELECT flags, nullable_flags FROM set_values;" "$DATABASE")
expect_contains "set c api type" "Type:       STRING" "$type_info_output"
expect_contains "set c api not-null flag" "Flags:      NOT_NULL SET " "$type_info_output"
expect_contains "set c api nullable flag" "Flags:      SET " "$type_info_output"

dml_expected=$(cat <<\EXPECTED
1	a	1	a	2	2	4
2	a,b	3	b	1	1	2
3	a	1	NULL	NULL	0,1	3
4		0	NULL	NULL	0	1
5	a,b,c	7	b,a	3		0
6		0		0		0
EXPECTED
)
expect_output \
    "set insert conversion and omitted defaults" \
    "$dml_expected" \
    "INSERT INTO set_values (flags, nullable_flags, numericish) VALUES "\
"('a','a','2'), ('B,A','B',2), (1,NULL,'3'), (DEFAULT, DEFAULT, '0'), "\
"('c,a,b,b', 'a,b,a', 0), ('', '', ''); "\
"SELECT id, flags, flags + 0, nullable_flags, nullable_flags + 0, "\
"numericish, numericish + 0 FROM set_values ORDER BY id;" \
    "$DATABASE"

predicate_expected=$(cat <<\EXPECTED
case display	2
case reversed	NULL
case number	2
quoted numeric predicate	NULL
null safe	3,4
not equal	1,5,6
EXPECTED
)
expect_output \
    "set predicate conversion" \
    "$predicate_expected" \
    "SELECT 'case display', GROUP_CONCAT(id ORDER BY id) FROM set_values WHERE flags = 'A,B'; "\
"SELECT 'case reversed', GROUP_CONCAT(id ORDER BY id) FROM set_values WHERE flags = 'b,a'; "\
"SELECT 'case number', GROUP_CONCAT(id ORDER BY id) FROM set_values WHERE flags = 3; "\
"SELECT 'quoted numeric predicate', GROUP_CONCAT(id ORDER BY id) FROM set_values "\
"WHERE flags = '3'; "\
"SELECT 'null safe', GROUP_CONCAT(id ORDER BY id) FROM set_values "\
"WHERE nullable_flags <=> NULL; "\
"SELECT 'not equal', GROUP_CONCAT(id ORDER BY id) FROM set_values WHERE nullable_flags <> 'b';" \
    "$DATABASE"

update_expected=$(cat <<\EXPECTED
2	0
1	a,c	5
2	a,b	3
3	a,c	5
4		0
5	a,b,c	7
6		0
1	0
2	a,c	5
EXPECTED
)
expect_output \
    "set update conversion and affected rows" \
    "$update_expected" \
    "UPDATE set_values SET flags = 'c,a' WHERE flags = 1; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT id, flags, flags + 0 FROM set_values ORDER BY id; "\
"UPDATE set_values SET flags = 5 WHERE id = 2; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT id, flags, flags + 0 FROM set_values WHERE id = 2;" \
    "$DATABASE"

default_expected=$(cat <<\EXPECTED
1	0	NULL	NULL	a,b	3		0
2	0	NULL	NULL	a,b	3		0
EXPECTED
)
expect_output \
    "set explicit defaults and no-default behavior" \
    "$default_expected" \
    "CREATE TABLE set_defaults ("\
"id INT, "\
"nn SET('a','b') NOT NULL DEFAULT '', "\
"n SET('a','b') NULL, "\
"explicit SET('a','b') DEFAULT 'b,a', "\
"empty_default SET('a','b') DEFAULT ''"\
"); "\
"INSERT INTO set_defaults (id) VALUES (1); "\
"INSERT INTO set_defaults (id, nn, n, explicit, empty_default) "\
"VALUES (2, DEFAULT, DEFAULT, DEFAULT, DEFAULT); "\
"SELECT id, nn + 0, n, n + 0, explicit, explicit + 0, empty_default, "\
"empty_default + 0 FROM set_defaults ORDER BY id;" \
    "$DATABASE"

expect_error \
    "not-null set with no explicit default" \
    1364 \
    HY000 \
    "Field 'nn' doesn't have a default value" \
    "CREATE TABLE set_no_default (id INT, nn SET('a','b') NOT NULL); "\
"INSERT INTO set_no_default (id) VALUES (1);" \
    "$DATABASE"

alter_default_expected=$(cat <<\EXPECTED
0	0	0
1	a	1
2	a,b	3
3	NULL	NULL
EXPECTED
)
expect_output \
    "set alter column default behavior" \
    "$alter_default_expected" \
    "CREATE TABLE set_alter_default (id INT, v SET('a','b') NULL DEFAULT 'a'); "\
"INSERT INTO set_alter_default (id) VALUES (1); "\
"ALTER TABLE set_alter_default ALTER COLUMN v SET DEFAULT 'b,a'; "\
"SELECT @@warning_count, @@error_count, ROW_COUNT(); "\
"INSERT INTO set_alter_default (id) VALUES (2); "\
"ALTER TABLE set_alter_default ALTER COLUMN v SET DEFAULT NULL; "\
"INSERT INTO set_alter_default (id) VALUES (3); "\
"SELECT id, v, v + 0 FROM set_alter_default ORDER BY id;" \
    "$DATABASE"

drop_default_expected=$(cat <<\EXPECTED
0	0	0
EXPECTED
)
expect_output \
    "set alter column drop default status" \
    "$drop_default_expected" \
    "CREATE TABLE set_drop_default (id INT, v SET('a','b') NULL DEFAULT 'a'); "\
"ALTER TABLE set_drop_default ALTER COLUMN v DROP DEFAULT; "\
"SELECT @@warning_count, @@error_count, ROW_COUNT();" \
    "$DATABASE"

expect_error \
    "set dropped default omitted insert" \
    1364 \
    HY000 \
    "Field 'v' doesn't have a default value" \
    "INSERT INTO set_drop_default (id) VALUES (1);" \
    "$DATABASE"

expect_error \
    "set alter column numeric default" \
    1067 \
    42000 \
    "Invalid default value for 'v'" \
    "ALTER TABLE set_alter_default ALTER COLUMN v SET DEFAULT 2;" \
    "$DATABASE"

expect_error \
    "duplicate set member" \
    1291 \
    HY000 \
    "Column 'v' has duplicated value 'a' in SET" \
    "CREATE TABLE duplicate_set (v SET('a ', 'A'));" \
    "$DATABASE"

expect_error \
    "comma-containing set member" \
    1367 \
    22007 \
    "Illegal set 'a,b' value found during parsing" \
    "CREATE TABLE comma_set (v SET('a,b','c'));" \
    "$DATABASE"

expect_error \
    "numeric set default" \
    1067 \
    42000 \
    "Invalid default value for 'v'" \
    "CREATE TABLE numeric_default (v SET('a','b') DEFAULT 3);" \
    "$DATABASE"

expect_error \
    "invalid set default" \
    1067 \
    42000 \
    "Invalid default value for 'v'" \
    "CREATE TABLE bad_default (v SET('a','b') DEFAULT 'c');" \
    "$DATABASE"

expect_error \
    "invalid set member assignment" \
    1265 \
    01000 \
    "Data truncated for column 'v' at row 1" \
    "CREATE TABLE bad_assignment (v SET('a','b') NOT NULL DEFAULT ''); "\
"INSERT INTO bad_assignment VALUES ('a,c');" \
    "$DATABASE"

expect_error \
    "out-of-range set bitmap assignment" \
    1265 \
    01000 \
    "Data truncated for column 'v' at row 1" \
    "CREATE TABLE bad_bitmap (v SET('a','b') NOT NULL DEFAULT ''); "\
"INSERT INTO bad_bitmap VALUES (4);" \
    "$DATABASE"

expect_error \
    "negative set bitmap assignment" \
    1265 \
    01000 \
    "Data truncated for column 'v' at row 1" \
    "CREATE TABLE bad_negative (v SET('a','b') NOT NULL DEFAULT ''); "\
"INSERT INTO bad_negative VALUES (-1);" \
    "$DATABASE"

expect_error \
    "set null into not null" \
    1048 \
    23000 \
    "Column 'v' cannot be null" \
    "CREATE TABLE bad_null (v SET('a','b') NOT NULL DEFAULT ''); "\
"INSERT INTO bad_null VALUES (NULL);" \
    "$DATABASE"

empty_member_expected=$(cat <<\EXPECTED
	0
	1
a	2
a	3
EXPECTED
)
expect_output \
    "upstream empty-string set members are deferred by MyLite" \
    "$empty_member_expected" \
    "CREATE TABLE empty_member_upstream (v SET('', 'a')); "\
"INSERT INTO empty_member_upstream VALUES (0),(1),(2),(3); "\
"SELECT v, v + 0 FROM empty_member_upstream ORDER BY v + 0;" \
    "$DATABASE"

order_expected=$(cat <<\EXPECTED
	0
a	1
b	2
a,b	3
a,b	3
b	2
a	1
	0
EXPECTED
)
expect_output \
    "upstream set numeric ordering is deferred by MyLite" \
    "$order_expected" \
    "CREATE TABLE ordered_set (v SET('a','b')); "\
"INSERT INTO ordered_set VALUES (''),('b'),('a,b'),('a'); "\
"SELECT v, v + 0 FROM ordered_set ORDER BY v; "\
"SELECT v, v + 0 FROM ordered_set ORDER BY v DESC;" \
    "$DATABASE"

expect_upstream_accepts \
    "upstream accepts full set primary keys deferred by MyLite" \
    "CREATE TABLE set_pk (v SET('a','b') NOT NULL, PRIMARY KEY(v));" \
    "$DATABASE"

expect_error \
    "upstream rejects set prefix keys" \
    1089 \
    HY000 \
    "Incorrect prefix key" \
    "CREATE TABLE set_prefix_key (v SET('a','b'), KEY k(v(1)));" \
    "$DATABASE"
