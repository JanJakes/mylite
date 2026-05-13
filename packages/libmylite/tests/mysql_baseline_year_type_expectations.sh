#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_year_type_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_year_type_expectations: $1" >&2
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
    *) fail "expected strict default sql_mode with zero-temporal checks" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

show_columns_expected=$(printf '%b\n' \
    "id\tint\tNO\t\tNULL\t" \
    "y\tyear\tYES\t\tNULL\t" \
    "y4\tyear\tYES\t\tNULL\t" \
    "ynn\tyear\tNO\t\tNULL\t" \
    "yd\tyear\tYES\t\t1970\t" \
    "ys\tyear\tYES\t\t1970\t" \
    "yz\tyear\tYES\t\t0000\t" \
    "yzs\tyear\tYES\t\t2000\t" \
    "yt\tyear\tYES\t\t2001\t" \
    "yf\tyear\tYES\t\t0000\t")
metadata_expected=$(
    printf '%s\n' \
        "Warning	1287	'YEAR(4)' is deprecated and will be removed in a future release. Please use YEAR instead"
    printf '%s' "$show_columns_expected"
    printf '\n'
    cat <<\EXPECTED
years	CREATE TABLE `years` (
  `id` int NOT NULL,
  `y` year DEFAULT NULL,
  `y4` year DEFAULT NULL,
  `ynn` year NOT NULL,
  `yd` year DEFAULT '1970',
  `ys` year DEFAULT '1970',
  `yz` year DEFAULT '0000',
  `yzs` year DEFAULT '2000',
  `yt` year DEFAULT '2001',
  `yf` year DEFAULT '0000'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
y	year	year	YES	NULL	NULL	NULL	NULL	NULL
y4	year	year	YES	NULL	NULL	NULL	NULL	NULL
ynn	year	year	NO	NULL	NULL	NULL	NULL	NULL
yd	year	year	YES	1970	NULL	NULL	NULL	NULL
ys	year	year	YES	1970	NULL	NULL	NULL	NULL
yz	year	year	YES	0000	NULL	NULL	NULL	NULL
yzs	year	year	YES	2000	NULL	NULL	NULL	NULL
yt	year	year	YES	2001	NULL	NULL	NULL	NULL
yf	year	year	YES	0000	NULL	NULL	NULL	NULL
EXPECTED
)
expect_output \
    "year metadata renders descriptors and deprecation warning" \
    "$metadata_expected" \
    "CREATE TABLE years ("\
"id INT NOT NULL, y YEAR, y4 YEAR(4), ynn YEAR NOT NULL, "\
"yd YEAR DEFAULT 70, ys YEAR DEFAULT '70', yz YEAR DEFAULT 0, "\
"yzs YEAR DEFAULT '0', yt YEAR DEFAULT TRUE, yf YEAR DEFAULT FALSE); "\
"SHOW WARNINGS; SHOW COLUMNS FROM years; SHOW CREATE TABLE years; "\
"SELECT COLUMN_NAME, DATA_TYPE, COLUMN_TYPE, IS_NULLABLE, COLUMN_DEFAULT, "\
"DATETIME_PRECISION, CHARACTER_MAXIMUM_LENGTH, NUMERIC_PRECISION, NUMERIC_SCALE "\
"FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = '${DATABASE}' "\
"AND TABLE_NAME = 'years' AND COLUMN_NAME <> 'id' ORDER BY ORDINAL_POSITION;" \
    "$DATABASE"

dml_expected=$(cat <<\EXPECTED
1	0000	0	0000	0
2	2000	2000	2000	2000
3	2001	2001	2001	2001
4	2069	2069	2069	2069
5	1970	1970	1970	1970
6	1999	1999	1999	1999
7	1901	1901	1901	1901
8	2155	2155	2155	2155
9	NULL	NULL	1970	1970
10	2001	2001	0000	0
11	0000	0	2155	2155
0	0	2069	2069
1	0	1970	1970
1	0	NULL
eq70	5,9
eqs70	5,9
nullsafe	4,9
between	2,3,4,5,6,9
between70	1,5,7,9,10
EXPECTED
)
expect_output \
    "year dml conversion and comparison predicates" \
    "$dml_expected" \
    "CREATE TABLE dml_t (id INT NOT NULL, y YEAR, ynn YEAR NOT NULL DEFAULT 1970); "\
"INSERT INTO dml_t VALUES "\
"(1, 0, 0), (2, '0', '0'), (3, 1, 1), (4, '69', '69'), "\
"(5, 70, 70), (6, '99', '99'), (7, 1901, 1901), (8, 2155, 2155), "\
"(9, NULL, DEFAULT), (10, TRUE, FALSE), (11, '0000', '2155'); "\
"SHOW WARNINGS; "\
"SELECT id, IF(y IS NULL, 'NULL', y), IF(y IS NULL, 'NULL', y + 0), "\
"ynn, ynn + 0 FROM dml_t ORDER BY id; "\
"UPDATE dml_t SET y = '69' WHERE id = 4; "\
"SELECT ROW_COUNT(), @@warning_count, y, y + 0 FROM dml_t WHERE id = 4; "\
"UPDATE dml_t SET y = 70 WHERE id = 4; "\
"SELECT ROW_COUNT(), @@warning_count, y, y + 0 FROM dml_t WHERE id = 4; "\
"UPDATE dml_t SET y = NULL WHERE id = 4; "\
"SELECT ROW_COUNT(), @@warning_count, IF(y IS NULL, 'NULL', y) FROM dml_t WHERE id = 4; "\
"SELECT 'eq70', GROUP_CONCAT(id ORDER BY id) FROM dml_t WHERE ynn = 70; "\
"SELECT 'eqs70', GROUP_CONCAT(id ORDER BY id) FROM dml_t WHERE ynn = '70'; "\
"SELECT 'nullsafe', GROUP_CONCAT(id ORDER BY id) FROM dml_t WHERE y <=> NULL; "\
"SELECT 'between', GROUP_CONCAT(id ORDER BY id) FROM dml_t "\
"WHERE ynn BETWEEN 1970 AND 2069; "\
"SELECT 'between70', GROUP_CONCAT(id ORDER BY id) FROM dml_t "\
"WHERE ynn BETWEEN 0 AND 70;" \
    "$DATABASE"

replace_expected=$(cat <<\EXPECTED
2	0
1	0
1	0
1	1970	1970	1970	1970
2	NULL	NULL	0000	0
3	2069	2069	2001	2001
4	2001	2001	2001	2001
EXPECTED
)
expect_output \
    "year replace conversion and descriptor copies" \
    "$replace_expected" \
    "CREATE TABLE replace_t (id INT, y YEAR, ynn YEAR NOT NULL DEFAULT 1970); "\
"REPLACE INTO replace_t VALUES (1, '70', DEFAULT), (2, NULL, 0); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"REPLACE INTO replace_t SET id = 3, y = '69', ynn = TRUE; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"CREATE TABLE replace_src (id INT, y YEAR, ynn YEAR NOT NULL DEFAULT 1970); "\
"INSERT INTO replace_src VALUES (4, 1, 1); "\
"REPLACE INTO replace_t (id, y, ynn) SELECT id, y, ynn FROM replace_src; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT id, IF(y IS NULL, 'NULL', y), IF(y IS NULL, 'NULL', y + 0), "\
"ynn, ynn + 0 FROM replace_t ORDER BY id;" \
    "$DATABASE"

in_expected=$(cat <<\EXPECTED
eq70	3,4,5
eqs70	3,4,5
in70	3,4,5
in1970	3,4,5
ins70	3,4,5
in0	1
in2000	2,6
ins0	2,6
ins2000	2,6
in70_2000	2,6
in1970_2000	2,3,4,5,6
ins70_s2000	2,3,4,5,6
in70_s2000	2,6
in1970_s70	3,4,5
in_bool	1
EXPECTED
)
expect_output \
    "year in predicate list coercion" \
    "$in_expected" \
    "CREATE TABLE in_t (id INT, y YEAR NOT NULL); "\
"INSERT INTO in_t VALUES "\
"(1,0),(2,'0'),(3,70),(4,'70'),(5,1970),(6,2000),(7,'69'),(8,2069); "\
"SELECT 'eq70', GROUP_CONCAT(id ORDER BY id) FROM in_t WHERE y = 70; "\
"SELECT 'eqs70', GROUP_CONCAT(id ORDER BY id) FROM in_t WHERE y = '70'; "\
"SELECT 'in70', GROUP_CONCAT(id ORDER BY id) FROM in_t WHERE y IN (70); "\
"SELECT 'in1970', GROUP_CONCAT(id ORDER BY id) FROM in_t WHERE y IN (1970); "\
"SELECT 'ins70', GROUP_CONCAT(id ORDER BY id) FROM in_t WHERE y IN ('70'); "\
"SELECT 'in0', GROUP_CONCAT(id ORDER BY id) FROM in_t WHERE y IN (0); "\
"SELECT 'in2000', GROUP_CONCAT(id ORDER BY id) FROM in_t WHERE y IN (2000); "\
"SELECT 'ins0', GROUP_CONCAT(id ORDER BY id) FROM in_t WHERE y IN ('0'); "\
"SELECT 'ins2000', GROUP_CONCAT(id ORDER BY id) FROM in_t WHERE y IN ('2000'); "\
"SELECT 'in70_2000', GROUP_CONCAT(id ORDER BY id) FROM in_t WHERE y IN (70,2000); "\
"SELECT 'in1970_2000', GROUP_CONCAT(id ORDER BY id) FROM in_t WHERE y IN (1970,2000); "\
"SELECT 'ins70_s2000', GROUP_CONCAT(id ORDER BY id) FROM in_t WHERE y IN ('70','2000'); "\
"SELECT 'in70_s2000', GROUP_CONCAT(id ORDER BY id) FROM in_t WHERE y IN (70,'2000'); "\
"SELECT 'in1970_s70', GROUP_CONCAT(id ORDER BY id) FROM in_t WHERE y IN (1970,'70'); "\
"SELECT 'in_bool', GROUP_CONCAT(id ORDER BY id) FROM in_t WHERE y IN (TRUE,FALSE);" \
    "$DATABASE"

ordering_expected=$(cat <<\EXPECTED
9	NULL	NULL
12	NULL	NULL
1	0000	0
11	0000	0
7	1901	1901
5	1970	1970
6	1999	1999
2	2000	2000
3	2001	2001
10	2001	2001
4	2069	2069
8	2155	2155
8	2155	2155
4	2069	2069
3	2001	2001
10	2001	2001
2	2000	2000
6	1999	1999
5	1970	1970
7	1901	1901
1	0000	0
11	0000	0
9	NULL	NULL
12	NULL	NULL
EXPECTED
)
expect_output \
    "year ordering uses represented value and mysql null placement" \
    "$ordering_expected" \
    "CREATE TABLE order_t (id INT NOT NULL, y YEAR); "\
"INSERT INTO order_t VALUES "\
"(1, 0), (2, '0'), (3, 1), (4, '69'), (5, 70), (6, '99'), "\
"(7, 1901), (8, 2155), (9, NULL), (10, TRUE), (11, '0000'), (12, NULL); "\
"SELECT id, IF(y IS NULL, 'NULL', y), IF(y IS NULL, 'NULL', y + 0) "\
"FROM order_t ORDER BY y ASC, id ASC; "\
"SELECT id, IF(y IS NULL, 'NULL', y), IF(y IS NULL, 'NULL', y + 0) "\
"FROM order_t ORDER BY y DESC, id ASC;" \
    "$DATABASE"

ignore_expected=$(printf '%b\n' \
    "Warning\t1264\tOut of range value for column 'y' at row 1" \
    "Warning\t1366\tIncorrect integer value: 'abc' for column 'y' at row 2" \
    "Warning\t1048\tColumn 'y' cannot be null" \
    "1\t0000\t0" \
    "2\t0000\t0" \
    "3\t0000\t0" \
    "Warning\t1364\tField 'y' doesn't have a default value" \
    "4\t0000\t0")
expect_output \
    "year insert ignore adjusts invalid and missing values" \
    "$ignore_expected" \
    "CREATE TABLE ignore_t (id INT, y YEAR NOT NULL); "\
"INSERT IGNORE INTO ignore_t VALUES (1, 2156), (2, 'abc'), (3, NULL); "\
"SHOW WARNINGS; SELECT id, y, y + 0 FROM ignore_t ORDER BY id; "\
"INSERT IGNORE INTO ignore_t(id) VALUES (4); "\
"SHOW WARNINGS; SELECT id, y, y + 0 FROM ignore_t WHERE id = 4;" \
    "$DATABASE"

alter_expected=$(
    printf '%b\n' \
        "id\tint\tYES\t\tNULL\t" \
        "y\tyear\tNO\t\tNULL\t"
    cat <<\EXPECTED
1	0000	0
2	0000	0
1	NULL	NULL
2	NULL	NULL
EXPECTED
    printf '%b\n' \
        "id\tint\tYES\t\tNULL\t" \
        "y\tyear\tNO\t\t1970\t" \
        "yn\tyear\tYES\t\tNULL\t"
    cat <<\EXPECTED
3	1970	1970
EXPECTED
)
expect_output \
    "year alter add and set default use year descriptors" \
    "$alter_expected" \
    "CREATE TABLE alter_t (id INT); INSERT INTO alter_t VALUES (1), (2); "\
"ALTER TABLE alter_t ADD y YEAR NOT NULL; SHOW WARNINGS; SHOW COLUMNS FROM alter_t; "\
"SELECT id, y, y + 0 FROM alter_t ORDER BY id; "\
"ALTER TABLE alter_t ADD yn YEAR NULL; SHOW WARNINGS; "\
"SELECT id, IF(yn IS NULL, 'NULL', yn), yn + 0 FROM alter_t ORDER BY id; "\
"ALTER TABLE alter_t ALTER COLUMN y SET DEFAULT '70'; "\
"SHOW COLUMNS FROM alter_t; "\
"INSERT INTO alter_t(id) VALUES (3); "\
"SELECT id, y, y + 0 FROM alter_t WHERE id = 3;" \
    "$DATABASE"

index_expected=$(cat <<\EXPECTED
index_t	CREATE TABLE `index_t` (
  `id` int DEFAULT NULL,
  `y` year DEFAULT NULL,
  UNIQUE KEY `uy` (`id`,`y`),
  KEY `ky` (`y`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
3	NULL	NULL
4	0000	0
5	2000	2000
1	2001	2001
2	2001	2001
1
2
EXPECTED
)
expect_output \
    "year secondary indexes are accepted" \
    "$index_expected" \
    "CREATE TABLE index_t (id INT, y YEAR, KEY ky (y), UNIQUE KEY uy (id, y)); "\
"SHOW WARNINGS; SHOW CREATE TABLE index_t; "\
"INSERT INTO index_t VALUES (1, 2001), (2, '01'), (3, NULL), (4, 0), (5, '0'); "\
"SELECT id, IF(y IS NULL, 'NULL', y), y + 0 FROM index_t ORDER BY y, id; "\
"SELECT id FROM index_t WHERE y = 2001 ORDER BY id;" \
    "$DATABASE"

run_mysql "CREATE TABLE err_insert (y YEAR NOT NULL);" "$DATABASE" >/dev/null
expect_error \
    "year width zero fails" \
    1818 \
    "HY000" \
    "Invalid display width. Use YEAR instead." \
    "CREATE TABLE bad_width_zero (y YEAR(0));" \
    "$DATABASE"
expect_error \
    "year width two fails" \
    1818 \
    "HY000" \
    "Invalid display width. Use YEAR instead." \
    "CREATE TABLE bad_width_two (y YEAR(2));" \
    "$DATABASE"
expect_error \
    "year width five fails" \
    1818 \
    "HY000" \
    "Invalid display width. Use YEAR instead." \
    "CREATE TABLE bad_width_five (y YEAR(5));" \
    "$DATABASE"
expect_error \
    "year current timestamp default fails" \
    1067 \
    "42000" \
    "Invalid default value for 'y'" \
    "CREATE TABLE bad_current_default (y YEAR DEFAULT CURRENT_TIMESTAMP);" \
    "$DATABASE"
expect_error \
    "year bad string default fails" \
    1067 \
    "42000" \
    "Invalid default value for 'y'" \
    "CREATE TABLE bad_string_default (y YEAR DEFAULT 'abc');" \
    "$DATABASE"
expect_error \
    "year not null default null fails" \
    1067 \
    "42000" \
    "Invalid default value for 'y'" \
    "CREATE TABLE bad_null_default (y YEAR NOT NULL DEFAULT NULL);" \
    "$DATABASE"
expect_error \
    "year lower out of range insert fails" \
    1264 \
    "22003" \
    "Out of range value for column 'y' at row 1" \
    "INSERT INTO err_insert VALUES (1900);" \
    "$DATABASE"
expect_error \
    "year upper out of range insert fails" \
    1264 \
    "22003" \
    "Out of range value for column 'y' at row 1" \
    "INSERT INTO err_insert VALUES (2156);" \
    "$DATABASE"
expect_error \
    "year negative insert fails" \
    1264 \
    "22003" \
    "Out of range value for column 'y' at row 1" \
    "INSERT INTO err_insert VALUES (-1);" \
    "$DATABASE"
expect_error \
    "year bad string insert fails" \
    1366 \
    "HY000" \
    "Incorrect integer value: 'abc' for column 'y' at row 1" \
    "INSERT INTO err_insert VALUES ('abc');" \
    "$DATABASE"
expect_error \
    "year not null insert null fails" \
    1048 \
    "23000" \
    "Column 'y' cannot be null" \
    "INSERT INTO err_insert VALUES (NULL);" \
    "$DATABASE"
expect_error \
    "year not null omitted value fails" \
    1364 \
    "HY000" \
    "Field 'y' doesn't have a default value" \
    "INSERT INTO err_insert () VALUES ();" \
    "$DATABASE"

expect_output \
    "year expression default is accepted upstream but deferred in mylite" \
    "2001" \
    "CREATE TABLE expr_default (y YEAR DEFAULT (2000 + 1)); "\
"INSERT INTO expr_default () VALUES (); SELECT y + 0 FROM expr_default;" \
    "$DATABASE"

expect_upstream_accepts \
    "year numeric attributes are accepted upstream but deferred in mylite" \
    "CREATE TABLE attr_deferred (a YEAR UNSIGNED, b YEAR SIGNED, c YEAR ZEROFILL, "\
"d YEAR(4) UNSIGNED); SHOW WARNINGS;" \
    "$DATABASE"

expect_upstream_accepts \
    "relaxed year strings are accepted upstream but deferred in mylite" \
    "CREATE TABLE relaxed_deferred (y YEAR); "\
"INSERT INTO relaxed_deferred VALUES (' 70'), ('70 '), ('0070'), ('02155');" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_year_type_expectations: ok"
