#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_national_character_aliases_expectations_$$"
NATIONAL_WARNING="NATIONAL/NCHAR/NVARCHAR implies the character set UTF8MB3, which will be replaced by UTF8MB4 in a future release. Please consider using CHAR(x) CHARACTER SET UTF8MB4 in order to be unambiguous."

fail() {
    printf '%s\n' "mysql_baseline_national_character_aliases_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw --skip-column-names "$@"
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

expect_output_trim_trailing_tabs() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql "$sql" "$@" | sed 's/	$//')
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
        *) fail "$label: expected error $code/$state containing [$message], got [$output]" ;;
    esac
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
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE};" >/dev/null

create_sql="CREATE TABLE aliases (
    a NCHAR,
    b NCHAR(2) NOT NULL DEFAULT 'x',
    c NATIONAL CHAR(3),
    d NATIONAL CHARACTER(4),
    e NVARCHAR(5) DEFAULT 'yz',
    f NATIONAL VARCHAR(6),
    g NCHAR VARCHAR(7),
    h NCHAR VARYING(8),
    i NATIONAL CHAR VARYING(9),
    j NATIONAL CHARACTER VARYING(10)
)"

expected_create_warnings="Warning	3720	${NATIONAL_WARNING}
Warning	3720	${NATIONAL_WARNING}
Warning	3720	${NATIONAL_WARNING}
Warning	3720	${NATIONAL_WARNING}
Warning	3720	${NATIONAL_WARNING}
Warning	3720	${NATIONAL_WARNING}
Warning	3720	${NATIONAL_WARNING}
Warning	3720	${NATIONAL_WARNING}
Warning	3720	${NATIONAL_WARNING}
Warning	3720	${NATIONAL_WARNING}
10	-1"
create_warning_sql="${create_sql}; SHOW WARNINGS; SELECT @@warning_count, ROW_COUNT();"
expect_output \
    "create table national warnings and status" \
    "$expected_create_warnings" \
    "$create_warning_sql" \
    "$DATABASE"

show_create_expected=$(cat <<\EXPECTED
aliases	CREATE TABLE `aliases` (
  `a` char(1) CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci DEFAULT NULL,
  `b` char(2) CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci NOT NULL DEFAULT 'x',
  `c` char(3) CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci DEFAULT NULL,
  `d` char(4) CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci DEFAULT NULL,
  `e` varchar(5) CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci DEFAULT 'yz',
  `f` varchar(6) CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci DEFAULT NULL,
  `g` varchar(7) CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci DEFAULT NULL,
  `h` varchar(8) CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci DEFAULT NULL,
  `i` varchar(9) CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci DEFAULT NULL,
  `j` varchar(10) CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "show create expands national aliases" \
    "$show_create_expected" \
    "SHOW CREATE TABLE aliases;" \
    "$DATABASE"

show_full_expected=$(cat <<\EXPECTED
a	char(1)	utf8mb3_general_ci	YES		NULL		select,insert,update,references
b	char(2)	utf8mb3_general_ci	NO		x		select,insert,update,references
c	char(3)	utf8mb3_general_ci	YES		NULL		select,insert,update,references
d	char(4)	utf8mb3_general_ci	YES		NULL		select,insert,update,references
e	varchar(5)	utf8mb3_general_ci	YES		yz		select,insert,update,references
f	varchar(6)	utf8mb3_general_ci	YES		NULL		select,insert,update,references
g	varchar(7)	utf8mb3_general_ci	YES		NULL		select,insert,update,references
h	varchar(8)	utf8mb3_general_ci	YES		NULL		select,insert,update,references
i	varchar(9)	utf8mb3_general_ci	YES		NULL		select,insert,update,references
j	varchar(10)	utf8mb3_general_ci	YES		NULL		select,insert,update,references
EXPECTED
)
expect_output_trim_trailing_tabs \
    "show full columns expands national aliases" \
    "$show_full_expected" \
    "SHOW FULL COLUMNS FROM aliases;" \
    "$DATABASE"

information_schema_expected=$(cat <<\EXPECTED
a	char	char(1)	1	3	utf8mb3	utf8mb3_general_ci	YES	NULL
b	char	char(2)	2	6	utf8mb3	utf8mb3_general_ci	NO	x
c	char	char(3)	3	9	utf8mb3	utf8mb3_general_ci	YES	NULL
d	char	char(4)	4	12	utf8mb3	utf8mb3_general_ci	YES	NULL
e	varchar	varchar(5)	5	15	utf8mb3	utf8mb3_general_ci	YES	yz
f	varchar	varchar(6)	6	18	utf8mb3	utf8mb3_general_ci	YES	NULL
g	varchar	varchar(7)	7	21	utf8mb3	utf8mb3_general_ci	YES	NULL
h	varchar	varchar(8)	8	24	utf8mb3	utf8mb3_general_ci	YES	NULL
i	varchar	varchar(9)	9	27	utf8mb3	utf8mb3_general_ci	YES	NULL
j	varchar	varchar(10)	10	30	utf8mb3	utf8mb3_general_ci	YES	NULL
EXPECTED
)
information_schema_sql="SELECT COLUMN_NAME,DATA_TYPE,COLUMN_TYPE,CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH,CHARACTER_SET_NAME,COLLATION_NAME,IS_NULLABLE,COLUMN_DEFAULT FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'aliases' ORDER BY ORDINAL_POSITION;"
expect_output \
    "information schema expands national aliases" \
    "$information_schema_expected" \
    "$information_schema_sql" \
    "$DATABASE"

readback_expected=$(cat <<\EXPECTED
a	1	xy	2	preserve  	10
EXPECTED
)
readback_sql="INSERT INTO aliases (a, b, e, j) VALUES ('a ', 'xy ', 'xy ', 'preserve  '); SELECT a, LENGTH(a), b, LENGTH(b), j, LENGTH(j) FROM aliases;"
expect_output \
    "national aliases use expanded char varchar DML semantics" \
    "$readback_expected" \
    "$readback_sql" \
    "$DATABASE"

expect_output \
    "create like national alias warnings and status" \
    "0	-1" \
    "CREATE TABLE clone LIKE aliases; SHOW WARNINGS; SELECT @@warning_count, ROW_COUNT();" \
    "$DATABASE"
clone_show_create_expected=$(printf '%s\n' "$show_create_expected" \
    | sed 's/^aliases/clone/; s/`aliases`/`clone`/')
expect_output \
    "create like national show create" \
    "$clone_show_create_expected" \
    "SHOW CREATE TABLE clone;" \
    "$DATABASE"

expect_output \
    "alter add national warning and status" \
    "Warning	3720	${NATIONAL_WARNING}
1	-1" \
    "ALTER TABLE aliases ADD COLUMN k NATIONAL CHARACTER(11) DEFAULT 'pq'; SHOW WARNINGS; SELECT @@warning_count, ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "alter modify national warning and status" \
    "Warning	3720	${NATIONAL_WARNING}
1	-1" \
    "ALTER TABLE aliases MODIFY COLUMN e NCHAR VARCHAR(12); SHOW WARNINGS; SELECT @@warning_count, ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "alter change national warning and status" \
    "Warning	3720	${NATIONAL_WARNING}
1	-1" \
    "ALTER TABLE aliases CHANGE COLUMN f ff NATIONAL CHARACTER VARYING(13); SHOW WARNINGS; SELECT @@warning_count, ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "altered national metadata" \
    "e	varchar	varchar(12)	12	36	utf8mb3	utf8mb3_general_ci
ff	varchar	varchar(13)	13	39	utf8mb3	utf8mb3_general_ci
k	char	char(11)	11	33	utf8mb3	utf8mb3_general_ci" \
    "SELECT COLUMN_NAME,DATA_TYPE,COLUMN_TYPE,CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH,CHARACTER_SET_NAME,COLLATION_NAME FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'aliases' AND COLUMN_NAME IN ('e', 'ff', 'k') ORDER BY COLUMN_NAME;" \
    "$DATABASE"

expect_error \
    "nvarchar requires length" \
    1064 \
    42000 \
    "SQL syntax" \
    "CREATE TABLE bad_nvarchar (v NVARCHAR);" \
    "$DATABASE"

expect_error \
    "national varchar requires length" \
    1064 \
    42000 \
    "SQL syntax" \
    "CREATE TABLE bad_national_varchar (v NATIONAL VARCHAR);" \
    "$DATABASE"

expect_error \
    "nchar empty length syntax" \
    1064 \
    42000 \
    "SQL syntax" \
    "CREATE TABLE bad_empty (v NCHAR());" \
    "$DATABASE"

expect_error \
    "nchar signed length syntax" \
    1064 \
    42000 \
    "SQL syntax" \
    "CREATE TABLE bad_negative (v NCHAR(-1));" \
    "$DATABASE"

expect_error \
    "nchar length cap" \
    1074 \
    42000 \
    "Column length too big" \
    "CREATE TABLE bad_nchar_length (v NCHAR(256));" \
    "$DATABASE"

run_mysql "DROP DATABASE ${DATABASE};" >/dev/null
trap - EXIT

printf '%s\n' "mysql_baseline_national_character_aliases_expectations: ok"
