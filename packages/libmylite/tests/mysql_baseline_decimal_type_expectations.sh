#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_decimal_type_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_decimal_type_expectations: $1" >&2
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
    "decimal unsigned declaration warning" \
    "Warning	1681	UNSIGNED for decimal and floating point data types is deprecated and support for it will be removed in a future release." \
    "CREATE TABLE decs ("\
"id INT NOT NULL, d DECIMAL, d0 DECIMAL(0), d00 DECIMAL(0,0), m DECIMAL(5), "\
"s DECIMAL(5,2), n NUMERIC(4,1), a DEC(3,1), f FIXED(3,1), "\
"u DECIMAL(5,2) UNSIGNED, nn DECIMAL(4,2) NOT NULL DEFAULT -1.20); "\
"SHOW WARNINGS;" \
    "$DATABASE"

show_columns_expected=$(cat <<\EXPECTED
id	int	NO		NULL	
d	decimal(10,0)	YES		NULL	
d0	decimal(10,0)	YES		NULL	
d00	decimal(10,0)	YES		NULL	
m	decimal(5,0)	YES		NULL	
s	decimal(5,2)	YES		NULL	
n	decimal(4,1)	YES		NULL	
a	decimal(3,1)	YES		NULL	
f	decimal(3,1)	YES		NULL	
u	decimal(5,2) unsigned	YES		NULL	
nn	decimal(4,2)	NO		-1.20	
EXPECTED
)
expect_output \
    "show columns renders decimal descriptors" \
    "$show_columns_expected" \
    "SHOW COLUMNS FROM decs;" \
    "$DATABASE"

show_create_expected=$(cat <<\EXPECTED
decs	CREATE TABLE `decs` (
  `id` int NOT NULL,
  `d` decimal(10,0) DEFAULT NULL,
  `d0` decimal(10,0) DEFAULT NULL,
  `d00` decimal(10,0) DEFAULT NULL,
  `m` decimal(5,0) DEFAULT NULL,
  `s` decimal(5,2) DEFAULT NULL,
  `n` decimal(4,1) DEFAULT NULL,
  `a` decimal(3,1) DEFAULT NULL,
  `f` decimal(3,1) DEFAULT NULL,
  `u` decimal(5,2) unsigned DEFAULT NULL,
  `nn` decimal(4,2) NOT NULL DEFAULT '-1.20'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "show create renders decimal descriptors" \
    "$show_create_expected" \
    "SHOW CREATE TABLE decs;" \
    "$DATABASE"

information_schema_expected=$(cat <<\EXPECTED
d	decimal	decimal(10,0)	10	0	YES	NULL	NULL	NULL	NULL
d0	decimal	decimal(10,0)	10	0	YES	NULL	NULL	NULL	NULL
d00	decimal	decimal(10,0)	10	0	YES	NULL	NULL	NULL	NULL
m	decimal	decimal(5,0)	5	0	YES	NULL	NULL	NULL	NULL
s	decimal	decimal(5,2)	5	2	YES	NULL	NULL	NULL	NULL
n	decimal	decimal(4,1)	4	1	YES	NULL	NULL	NULL	NULL
a	decimal	decimal(3,1)	3	1	YES	NULL	NULL	NULL	NULL
f	decimal	decimal(3,1)	3	1	YES	NULL	NULL	NULL	NULL
u	decimal	decimal(5,2) unsigned	5	2	YES	NULL	NULL	NULL	NULL
nn	decimal	decimal(4,2)	4	2	NO	-1.20	NULL	NULL	NULL
EXPECTED
)
expect_output \
    "information schema renders decimal descriptors" \
    "$information_schema_expected" \
    "SELECT COLUMN_NAME, DATA_TYPE, COLUMN_TYPE, NUMERIC_PRECISION, NUMERIC_SCALE, "\
"IS_NULLABLE, COLUMN_DEFAULT, CHARACTER_MAXIMUM_LENGTH, CHARACTER_SET_NAME, COLLATION_NAME "\
"FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = '${DATABASE}' "\
"AND TABLE_NAME = 'decs' AND COLUMN_NAME <> 'id' ORDER BY ORDINAL_POSITION;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE values_t ("\
"id INT, d DECIMAL(5,2), z DECIMAL(5,0), u DECIMAL(3,1) UNSIGNED, "\
"nn DECIMAL(4,2) NOT NULL DEFAULT 1.00);" \
    "$DATABASE" >/dev/null

expect_output \
    "decimal exact values canonicalize without warnings" \
    "1	3.10	12	1.2	1.00" \
    "INSERT INTO values_t VALUES (1, +0003.1, 12, 1.2, DEFAULT); "\
"SHOW WARNINGS; SELECT id, d, z, u, nn FROM values_t WHERE id = 1;" \
    "$DATABASE"

rounding_expected=$(cat <<\EXPECTED
Note	1265	Data truncated for column 'd' at row 1
Note	1265	Data truncated for column 'z' at row 1
Note	1265	Data truncated for column 'u' at row 1
Note	1265	Data truncated for column 'nn' at row 1
2	1.23	13	1.3	1.24
EXPECTED
)
expect_output \
    "decimal rounding records truncation notes" \
    "$rounding_expected" \
    "INSERT INTO values_t VALUES (2, 1.234, 12.5, 1.25, 1.239); "\
"SHOW WARNINGS; SELECT id, d, z, u, nn FROM values_t WHERE id = 2;" \
    "$DATABASE"

expect_output \
    "decimal signed and unsigned boundaries" \
    "3	999.99	99999	99.9	-99.99" \
    "INSERT INTO values_t VALUES (3, 999.99, 99999, 99.9, -99.99); "\
"SELECT id, d, z, u, nn FROM values_t WHERE id = 3;" \
    "$DATABASE"

expect_output \
    "decimal dot forms and negative zero normalize" \
    "4	0.25	-1	0.0	0.00" \
    "INSERT INTO values_t VALUES (4, .25, -1., -0.00, -0.00); "\
"SELECT id, d, z, u, nn FROM values_t WHERE id = 4;" \
    "$DATABASE"

expect_error \
    "decimal rounded out of range fails" \
    1264 \
    "22003" \
    "Out of range value for column 'd' at row 1" \
    "INSERT INTO values_t VALUES (5, 999.995, 1, 1.1, 1.1);" \
    "$DATABASE"

expect_error \
    "decimal integer out of range fails" \
    1264 \
    "22003" \
    "Out of range value for column 'd' at row 1" \
    "INSERT INTO values_t VALUES (6, 1000.00, 1, 1.1, 1.1);" \
    "$DATABASE"

expect_error \
    "unsigned decimal rejects negative input" \
    1264 \
    "22003" \
    "Out of range value for column 'u' at row 1" \
    "INSERT INTO values_t VALUES (7, 1.00, 1, -1.0, 1.1);" \
    "$DATABASE"

expect_error \
    "decimal not null rejects null" \
    1048 \
    "23000" \
    "Column 'nn' cannot be null" \
    "INSERT INTO values_t VALUES (8, NULL, NULL, NULL, NULL);" \
    "$DATABASE"

ignore_expected=$(cat <<\EXPECTED
Warning	1264	Out of range value for column 'd' at row 1
Warning	1264	Out of range value for column 'z' at row 1
Warning	1264	Out of range value for column 'u' at row 1
Warning	1048	Column 'nn' cannot be null
9	999.99	99999	0.0	0.00
EXPECTED
)
expect_output \
    "decimal insert ignore clips and adjusts" \
    "$ignore_expected" \
    "INSERT IGNORE INTO values_t VALUES (9, 1000.00, 100000, -1.0, NULL); "\
"SHOW WARNINGS; SELECT id, d, z, u, nn FROM values_t WHERE id = 9;" \
    "$DATABASE"

update_expected=$(cat <<\EXPECTED
0	0
1	1.20
1	1
1	1.24
1	0
1	0.00
1	0
1	NULL
EXPECTED
)
expect_output \
    "decimal update uses canonical changed-row semantics" \
    "$update_expected" \
    "CREATE TABLE update_t (id INT, d DECIMAL(5,2) DEFAULT 0.00, "\
"nn DECIMAL(5,2) NOT NULL DEFAULT 1.00); "\
"INSERT INTO update_t VALUES (1, 1.20, 2.00); "\
"UPDATE update_t SET d = 1.200 WHERE id = 1; SELECT ROW_COUNT(), @@warning_count; "\
"SHOW WARNINGS; SELECT id, d FROM update_t WHERE id = 1; "\
"UPDATE update_t SET d = 1.235 WHERE id = 1; SELECT ROW_COUNT(), @@warning_count; "\
"SHOW WARNINGS; SELECT id, d FROM update_t WHERE id = 1; "\
"UPDATE update_t SET d = DEFAULT WHERE id = 1; SELECT ROW_COUNT(), @@warning_count; "\
"SELECT id, d FROM update_t WHERE id = 1; "\
"UPDATE update_t SET d = NULL WHERE id = 1; SELECT ROW_COUNT(), @@warning_count; "\
"SELECT id, d FROM update_t WHERE id = 1;" \
    "$DATABASE"

expect_error \
    "decimal update not null rejects null" \
    1048 \
    "23000" \
    "Column 'nn' cannot be null" \
    "UPDATE update_t SET nn = NULL WHERE id = 1;" \
    "$DATABASE"

alter_expected=$(cat <<\EXPECTED
1	0.00	-1.2
1	0.00
1	2.30
d	decimal(5,2)	YES		3.40	
2	3.40	-1.2	1.11	2.30
EXPECTED
)
expect_output \
    "decimal defaults and alter add behavior" \
    "$alter_expected" \
    "CREATE TABLE defaults_t ("\
"id INT, d DECIMAL(5,2) DEFAULT 0.00, n DECIMAL(4,1) NOT NULL DEFAULT -1.2); "\
"INSERT INTO defaults_t(id) VALUES (1); SELECT id, d, n FROM defaults_t WHERE id = 1; "\
"ALTER TABLE defaults_t ADD COLUMN added DECIMAL(4,2) NOT NULL; "\
"SELECT id, added FROM defaults_t WHERE id = 1; "\
"ALTER TABLE defaults_t ADD COLUMN added_default DECIMAL(4,2) NOT NULL DEFAULT 2.30; "\
"SELECT id, added_default FROM defaults_t WHERE id = 1; "\
"ALTER TABLE defaults_t ALTER COLUMN d SET DEFAULT 3.40; "\
"SHOW COLUMNS FROM defaults_t LIKE 'd'; "\
"INSERT INTO defaults_t(id, n, added, added_default) VALUES (2, DEFAULT, 1.11, DEFAULT); "\
"SELECT id, d, n, added, added_default FROM defaults_t WHERE id = 2;" \
    "$DATABASE"

expect_error \
    "decimal precision above maximum fails" \
    1426 \
    "42000" \
    "Too-big precision 66 specified for 'd'. Maximum is 65." \
    "CREATE TABLE bad_m66 (d DECIMAL(66,0));" \
    "$DATABASE"

expect_error \
    "decimal scale above maximum fails" \
    1425 \
    "42000" \
    "Too big scale 31 specified for column 'd'. Maximum is 30." \
    "CREATE TABLE bad_d31 (d DECIMAL(31,31));" \
    "$DATABASE"

expect_error \
    "decimal scale greater than precision fails" \
    1427 \
    "42000" \
    "For float(M,D), double(M,D) or decimal(M,D), M must be >= D (column 'd')." \
    "CREATE TABLE bad_d_gt_m (d DECIMAL(3,4));" \
    "$DATABASE"

expect_error \
    "decimal negative precision is syntax error" \
    1064 \
    "42000" \
    "near '-1,0))'" \
    "CREATE TABLE bad_neg (d DECIMAL(-1,0));" \
    "$DATABASE"

expect_error \
    "decimal empty precision list is syntax error" \
    1064 \
    "42000" \
    "near '))'" \
    "CREATE TABLE bad_empty (d DECIMAL());" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts zerofill deferred by MyLite" \
    "CREATE TABLE upstream_zerofill (d DECIMAL(5,2) ZEROFILL);" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts string and approximate decimal inputs deferred by MyLite" \
    "CREATE TABLE upstream_inputs (d DECIMAL(5,2)); "\
"INSERT INTO upstream_inputs VALUES ('001.20'), (1e2), (0x10), (b'101');" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts decimal ordering deferred by MyLite" \
    "CREATE TABLE upstream_order (id INT, d DECIMAL(5,2)); "\
"INSERT INTO upstream_order VALUES (1, 2.00), (2, 1.00); "\
"SELECT id FROM upstream_order ORDER BY d;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_decimal_type_expectations: ok"
