#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_time_type_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_time_type_expectations: $1" >&2
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

case "$(run_mysql "SELECT @@sql_mode;")" in
    *STRICT_TRANS_TABLES*NO_ZERO_IN_DATE*NO_ZERO_DATE*) ;;
    *) fail "expected strict default sql_mode with zero-date checks" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

run_mysql \
    "CREATE TABLE times ("\
"id INT, t TIME, nn TIME NOT NULL DEFAULT '01:02:03');" \
    "$DATABASE" >/dev/null

show_columns_expected=$(printf '%s\n%s\n%s' \
    'id	int	YES		NULL	' \
    't	time	YES		NULL	' \
    'nn	time	NO		01:02:03	')
expect_output \
    "show columns renders time descriptors" \
    "$show_columns_expected" \
    "SHOW COLUMNS FROM times;" \
    "$DATABASE"

show_create_expected=$(cat <<\EXPECTED
times	CREATE TABLE `times` (
  `id` int DEFAULT NULL,
  `t` time DEFAULT NULL,
  `nn` time NOT NULL DEFAULT '01:02:03'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "show create renders time descriptors" \
    "$show_create_expected" \
    "SHOW CREATE TABLE times;" \
    "$DATABASE"

information_schema_expected=$(cat <<\EXPECTED
t	time	time	YES	NULL	0	NULL	NULL	NULL
nn	time	time	NO	01:02:03	0	NULL	NULL	NULL
EXPECTED
)
expect_output \
    "information schema renders time descriptors" \
    "$information_schema_expected" \
    "SELECT COLUMN_NAME, DATA_TYPE, COLUMN_TYPE, IS_NULLABLE, COLUMN_DEFAULT, "\
"DATETIME_PRECISION, CHARACTER_MAXIMUM_LENGTH, NUMERIC_PRECISION, NUMERIC_SCALE "\
"FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = '${DATABASE}' "\
"AND TABLE_NAME = 'times' AND COLUMN_NAME <> 'id' ORDER BY ORDINAL_POSITION;" \
    "$DATABASE"

expect_output \
    "canonical time values and boundaries store without warnings" \
    "1	-838:59:59	838:59:59
2	-00:00:01	00:00:00
3	00:00:00	01:02:03
4	24:00:00	100:00:00
5	NULL	01:02:03" \
    "INSERT INTO times VALUES "\
"(1, '-838:59:59', '838:59:59'), "\
"(2, '-00:00:01', '00:00:00'), "\
"(3, '00:00:00', DEFAULT), "\
"(4, '24:00:00', '100:00:00'), "\
"(5, NULL, DEFAULT); "\
"SHOW WARNINGS; SELECT id, t, nn FROM times ORDER BY id;" \
    "$DATABASE"

expect_error \
    "positive out-of-range time fails" \
    1292 \
    "22007" \
    "Incorrect time value: '839:00:00' for column 't' at row 1" \
    "INSERT INTO times VALUES (6, '839:00:00', '00:00:00');" \
    "$DATABASE"

expect_error \
    "negative out-of-range time fails" \
    1292 \
    "22007" \
    "Incorrect time value: '-839:00:00' for column 't' at row 1" \
    "INSERT INTO times VALUES (7, '-839:00:00', '00:00:00');" \
    "$DATABASE"

expect_error \
    "invalid time fails" \
    1292 \
    "22007" \
    "Incorrect time value: '12:60:00' for column 't' at row 1" \
    "INSERT INTO times VALUES (8, '12:60:00', '00:00:00');" \
    "$DATABASE"

expect_error \
    "time not null rejects null" \
    1048 \
    "23000" \
    "Column 'nn' cannot be null" \
    "INSERT INTO times VALUES (9, '00:00:00', NULL);" \
    "$DATABASE"

ignore_expected=$(cat <<\EXPECTED
Warning	1264	Out of range value for column 't' at row 1
Warning	1048	Column 'nn' cannot be null
Warning	1264	Out of range value for column 't' at row 2
Warning	1265	Data truncated for column 'nn' at row 2
Warning	1264	Out of range value for column 't' at row 3
10	838:59:59	00:00:00
11	-838:59:59	00:00:00
12	00:00:00	00:00:00
EXPECTED
)
expect_output \
    "time insert ignore adjusts invalid and null values" \
    "$ignore_expected" \
    "INSERT IGNORE INTO times VALUES "\
"(10, '839:00:00', NULL), "\
"(11, '-839:00:00', 'bad'), "\
"(12, '12:60:00', '00:00:00'); "\
"SHOW WARNINGS; SELECT id, t, nn FROM times WHERE id IN (10, 11, 12) ORDER BY id;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE required_t (id INT, t TIME NOT NULL); "\
"INSERT IGNORE INTO required_t (id) VALUES (1); "\
"INSERT IGNORE INTO required_t VALUES (2, DEFAULT);" \
    "$DATABASE" >/dev/null
expect_output \
    "time insert ignore adjusts missing defaults" \
    "1	00:00:00
2	00:00:00" \
    "SELECT id, t FROM required_t ORDER BY id;" \
    "$DATABASE"

update_expected=$(cat <<\EXPECTED
0	0
1	0
1	02:03:04	-00:00:01
1	0
1	0
1	NULL	-00:00:01
EXPECTED
)
expect_output \
    "time update uses canonical changed-row semantics" \
    "$update_expected" \
    "CREATE TABLE update_t (id INT, t TIME DEFAULT '01:02:03', "\
"nn TIME NOT NULL DEFAULT '-00:00:01'); "\
"INSERT INTO update_t VALUES (1, '01:02:03', '-00:00:01'); "\
"UPDATE update_t SET t = '01:02:03' WHERE id = 1; SELECT ROW_COUNT(), @@warning_count; "\
"UPDATE update_t SET t = '02:03:04' WHERE id = 1; SELECT ROW_COUNT(), @@warning_count; "\
"SELECT id, t, nn FROM update_t WHERE id = 1; "\
"UPDATE update_t SET t = DEFAULT WHERE id = 1; SELECT ROW_COUNT(), @@warning_count; "\
"UPDATE update_t SET t = NULL WHERE id = 1; SELECT ROW_COUNT(), @@warning_count; "\
"SELECT id, t, nn FROM update_t WHERE id = 1;" \
    "$DATABASE"

expect_output \
    "time predicates and ordering use time value ordering" \
    "5	-838:59:59
2	-00:00:01
3	00:00:00
4	24:00:00
1	838:59:59
1	838:59:59
4	24:00:00
3	00:00:00
2	-00:00:01
5	-838:59:59
2
5
2
3
4" \
    "CREATE TABLE order_t (id INT, t TIME); "\
"INSERT INTO order_t VALUES "\
"(1, '838:59:59'), (2, '-00:00:01'), (3, '00:00:00'), "\
"(4, '24:00:00'), (5, '-838:59:59'); "\
"SELECT id, t FROM order_t WHERE t IS NOT NULL ORDER BY t ASC, id; "\
"SELECT id, t FROM order_t WHERE t IS NOT NULL ORDER BY t DESC, id; "\
"SELECT id FROM order_t WHERE t < '00:00:00' ORDER BY id; "\
"SELECT id FROM order_t WHERE t BETWEEN '-00:00:01' AND '24:00:00' ORDER BY id; "\
"SELECT id FROM order_t WHERE t IN (NULL, '838:59:59', '24:00:00') ORDER BY id;" \
    "$DATABASE"

expect_output \
    "time alter add column backfills zero for not null no default" \
    "$(printf '%s\n%s\n%s\n%s\n%s' \
        '1	NULL	02:03:04	00:00:00' \
        'id	int	YES		NULL	' \
        'n	time	YES		NULL	' \
        'nn	time	NO		02:03:04	' \
        'bad	time	NO		NULL	')" \
    "CREATE TABLE alter_t (id INT); INSERT INTO alter_t VALUES (1); "\
"ALTER TABLE alter_t ADD COLUMN n TIME; "\
"ALTER TABLE alter_t ADD COLUMN nn TIME NOT NULL DEFAULT '02:03:04'; "\
"ALTER TABLE alter_t ADD COLUMN bad TIME NOT NULL; "\
"SELECT id, n, nn, bad FROM alter_t; SHOW COLUMNS FROM alter_t;" \
    "$DATABASE"

expect_output \
    "time secondary indexes are accepted and exposed" \
    "k_t	t	1	1
u_t	t	0	1" \
    "CREATE TABLE indexed (id INT, t TIME, UNIQUE KEY u_t (t), KEY k_t (t)); "\
"SELECT INDEX_NAME, COLUMN_NAME, NON_UNIQUE, SEQ_IN_INDEX "\
"FROM INFORMATION_SCHEMA.STATISTICS WHERE TABLE_SCHEMA = '${DATABASE}' "\
"AND TABLE_NAME = 'indexed' ORDER BY INDEX_NAME, SEQ_IN_INDEX;" \
    "$DATABASE"

expect_error \
    "invalid time default fails" \
    1067 \
    "42000" \
    "Invalid default value for 't'" \
    "CREATE TABLE bad_default (t TIME DEFAULT '839:00:00');" \
    "$DATABASE"

expect_error \
    "too-large fractional precision fails upstream" \
    1426 \
    "42000" \
    "Too-big precision 7 specified for 't'. Maximum is 6." \
    "CREATE TABLE bad_fsp (t TIME(7));" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts fractional precision deferred by MyLite" \
    "CREATE TABLE upstream_fsp (t TIME(3)); DROP TABLE upstream_fsp;" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts relaxed time strings deferred by MyLite" \
    "CREATE TABLE upstream_relaxed (t TIME); "\
"INSERT INTO upstream_relaxed VALUES ('1:2:3'), ('-0:0:1'), ('012:00:00'); "\
"DROP TABLE upstream_relaxed;" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts standard time literal deferred by MyLite" \
    "CREATE TABLE upstream_literal (t TIME); "\
"INSERT INTO upstream_literal VALUES (TIME '01:02:03'); "\
"DROP TABLE upstream_literal;" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts numeric time input deferred by MyLite" \
    "CREATE TABLE upstream_numeric (t TIME); "\
"INSERT INTO upstream_numeric VALUES (101112); "\
"DROP TABLE upstream_numeric;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_time_type_expectations: ok"
