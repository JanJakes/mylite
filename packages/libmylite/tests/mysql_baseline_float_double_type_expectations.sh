#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_float_double_type_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_float_double_type_expectations: $1" >&2
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
    *STRICT_TRANS_TABLES*) ;;
    *) fail "expected strict default sql_mode" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

expect_output \
    "float double unsigned declaration warnings" \
    "Warning	1681	UNSIGNED for decimal and floating point data types is deprecated and support for it will be removed in a future release.
Warning	1681	UNSIGNED for decimal and floating point data types is deprecated and support for it will be removed in a future release." \
    "CREATE TABLE approx_types ("\
"id INT NOT NULL, f FLOAT, f0 FLOAT(0), f24 FLOAT(24), f25 FLOAT(25), f53 FLOAT(53), "\
"d DOUBLE, dp DOUBLE PRECISION, r REAL, f4 FLOAT4, f8 FLOAT8, "\
"fu FLOAT UNSIGNED, du DOUBLE UNSIGNED, nn FLOAT NOT NULL DEFAULT 1.25, "\
"dn DOUBLE NOT NULL DEFAULT -2.25); SHOW WARNINGS;" \
    "$DATABASE"

show_columns_expected=$(cat <<\EXPECTED
id	int	NO		NULL	
f	float	YES		NULL	
f0	float	YES		NULL	
f24	float	YES		NULL	
f25	double	YES		NULL	
f53	double	YES		NULL	
d	double	YES		NULL	
dp	double	YES		NULL	
r	double	YES		NULL	
f4	float	YES		NULL	
f8	double	YES		NULL	
fu	float unsigned	YES		NULL	
du	double unsigned	YES		NULL	
nn	float	NO		1.25	
dn	double	NO		-2.25	
EXPECTED
)
expect_output \
    "show columns renders approximate descriptors" \
    "$show_columns_expected" \
    "SHOW COLUMNS FROM approx_types;" \
    "$DATABASE"

show_create_expected=$(cat <<\EXPECTED
approx_types	CREATE TABLE `approx_types` (
  `id` int NOT NULL,
  `f` float DEFAULT NULL,
  `f0` float DEFAULT NULL,
  `f24` float DEFAULT NULL,
  `f25` double DEFAULT NULL,
  `f53` double DEFAULT NULL,
  `d` double DEFAULT NULL,
  `dp` double DEFAULT NULL,
  `r` double DEFAULT NULL,
  `f4` float DEFAULT NULL,
  `f8` double DEFAULT NULL,
  `fu` float unsigned DEFAULT NULL,
  `du` double unsigned DEFAULT NULL,
  `nn` float NOT NULL DEFAULT '1.25',
  `dn` double NOT NULL DEFAULT '-2.25'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "show create renders approximate descriptors" \
    "$show_create_expected" \
    "SHOW CREATE TABLE approx_types;" \
    "$DATABASE"

information_schema_expected=$(cat <<\EXPECTED
f	float	float	12	NULL	YES	NULL	NULL	NULL	NULL
f0	float	float	12	NULL	YES	NULL	NULL	NULL	NULL
f24	float	float	12	NULL	YES	NULL	NULL	NULL	NULL
f25	double	double	22	NULL	YES	NULL	NULL	NULL	NULL
f53	double	double	22	NULL	YES	NULL	NULL	NULL	NULL
d	double	double	22	NULL	YES	NULL	NULL	NULL	NULL
dp	double	double	22	NULL	YES	NULL	NULL	NULL	NULL
r	double	double	22	NULL	YES	NULL	NULL	NULL	NULL
f4	float	float	12	NULL	YES	NULL	NULL	NULL	NULL
f8	double	double	22	NULL	YES	NULL	NULL	NULL	NULL
fu	float	float unsigned	12	NULL	YES	NULL	NULL	NULL	NULL
du	double	double unsigned	22	NULL	YES	NULL	NULL	NULL	NULL
nn	float	float	12	NULL	NO	1.25	NULL	NULL	NULL
dn	double	double	22	NULL	NO	-2.25	NULL	NULL	NULL
EXPECTED
)
expect_output \
    "information schema renders approximate descriptors" \
    "$information_schema_expected" \
    "SELECT COLUMN_NAME, DATA_TYPE, COLUMN_TYPE, NUMERIC_PRECISION, NUMERIC_SCALE, "\
"IS_NULLABLE, COLUMN_DEFAULT, CHARACTER_MAXIMUM_LENGTH, CHARACTER_SET_NAME, COLLATION_NAME "\
"FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = '${DATABASE}' "\
"AND TABLE_NAME = 'approx_types' AND COLUMN_NAME <> 'id' ORDER BY ORDINAL_POSITION;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE approx_values ("\
"id INT, f FLOAT, d DOUBLE, fu FLOAT UNSIGNED, du DOUBLE UNSIGNED, "\
"nn FLOAT NOT NULL DEFAULT 1.5, dn DOUBLE NOT NULL DEFAULT -2.5);" \
    "$DATABASE" >/dev/null

expect_output \
    "approximate literal values store without warnings" \
    "1	42	42	42	42	1.5	-2.5
2	3.14159	3.1415926535	1.25	2.5	3.40282e38	1.7976931348623157e308
3	0	0	0	0	1.5	-2.5
4	NULL	NULL	NULL	NULL	1.5	-2.5" \
    "INSERT INTO approx_values VALUES "\
"(1, 42, 42, 42, 42, DEFAULT, DEFAULT), "\
"(2, 3.1415926535, 3.1415926535, 1.25, 2.5, 3.402823466E+38, "\
"1.7976931348623157E+308), "\
"(3, +0, -0, FALSE, FALSE, DEFAULT, DEFAULT), "\
"(4, NULL, NULL, NULL, NULL, DEFAULT, DEFAULT); "\
"SHOW WARNINGS; SELECT id, f, d, fu, du, nn, dn FROM approx_values ORDER BY id;" \
    "$DATABASE"

expect_output \
    "approximate update reports changed rows" \
    "1	0
1	9.75	4.25" \
    "UPDATE approx_values SET f = 9.75 WHERE id = 1; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"UPDATE approx_values SET d = 4.25 WHERE id = 1; "\
"SELECT ROW_COUNT(), f, d FROM approx_values WHERE id = 1;" \
    "$DATABASE"

expect_output \
    "approximate underflow stores zero without warnings" \
    "5	0	0" \
    "INSERT INTO approx_values (id, f, d) VALUES (5, 1e-46, 1e-325); "\
"SHOW WARNINGS; SELECT id, f, d FROM approx_values WHERE id = 5;" \
    "$DATABASE"

expect_error \
    "float overflow fails" \
    1264 \
    "22003" \
    "Out of range value for column 'f' at row 1" \
    "INSERT INTO approx_values (id, f) VALUES (6, 3.5e38);" \
    "$DATABASE"

expect_error \
    "unsigned float rejects negative" \
    1264 \
    "22003" \
    "Out of range value for column 'fu' at row 1" \
    "INSERT INTO approx_values (id, fu) VALUES (7, -1.5);" \
    "$DATABASE"

expect_error \
    "not-null float rejects null" \
    1048 \
    "23000" \
    "Column 'nn' cannot be null" \
    "INSERT INTO approx_values (id, nn) VALUES (8, NULL);" \
    "$DATABASE"

expect_error \
    "float precision above double fails" \
    1063 \
    "42000" \
    "Incorrect column specifier for column 'x'" \
    "CREATE TABLE bad_precision (x FLOAT(54));" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts deferred scaled float" \
    "CREATE TABLE deferred_scaled_float (x FLOAT(7,4));" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts deferred zerofill float" \
    "CREATE TABLE deferred_zerofill_float (x FLOAT ZEROFILL);" \
    "$DATABASE"
