#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_generated_column_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_generated_column_lifecycle_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_BIN" ]; then
        if [ -n "$MYSQL_SOCKET" ]; then
            printf '%s\n' "$sql" \
                | "$MYSQL_BIN" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                    --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
        else
            printf '%s\n' "$sql" \
                | "$MYSQL_BIN" --protocol=TCP -h127.0.0.1 -uroot \
                    --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
        fi
    else
        printf '%s\n' "$sql" \
            | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
                --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
    fi
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
        *) fail "$label: expected error $code/$state containing [$message], got [$output]" ;;
    esac
}

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT HUP INT TERM

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE} DEFAULT CHARACTER SET utf8mb4 "\
"COLLATE utf8mb4_0900_ai_ci;" >/dev/null

generated_values_expected=$(cat <<\EXPECTED
4	5	3	8	-4	4	NULL	0	2
NULL	NULL	NULL	NULL	NULL	NULL	NULL	0	2
EXPECTED
)
expect_output \
    "generated values and warning count" \
    "$generated_values_expected" \
    "CREATE TABLE exprs ("\
"a INT, "\
"b INT AS (a + 1), "\
"c INT GENERATED ALWAYS AS (a - 1) VIRTUAL, "\
"d INT GENERATED ALWAYS AS (a * 2) STORED, "\
"e INT AS (-a), "\
"f INT AS (+a), "\
"n INT AS (NULL)); "\
"INSERT INTO exprs(a) VALUES (4), (NULL); "\
"SELECT a,b,c,d,e,f,n,@@warning_count,ROW_COUNT() FROM exprs ORDER BY a IS NULL, a;" \
    "$DATABASE"

show_columns_expected=$(printf '%s\n' \
    $'a\tint\tYES\t\tNULL\t' \
    $'b\tint\tYES\t\tNULL\tVIRTUAL GENERATED' \
    $'c\tint\tYES\t\tNULL\tVIRTUAL GENERATED' \
    $'d\tint\tYES\t\tNULL\tSTORED GENERATED' \
    $'e\tint\tYES\t\tNULL\tVIRTUAL GENERATED' \
    $'f\tint\tYES\t\tNULL\tVIRTUAL GENERATED' \
    $'n\tint\tYES\t\tNULL\tVIRTUAL GENERATED')
expect_output \
    "SHOW COLUMNS generated extras" \
    "$show_columns_expected" \
    "SHOW COLUMNS FROM exprs;" \
    "$DATABASE"

show_create_expected=$(cat <<\EXPECTED
exprs	CREATE TABLE `exprs` (
  `a` int DEFAULT NULL,
  `b` int GENERATED ALWAYS AS ((`a` + 1)) VIRTUAL,
  `c` int GENERATED ALWAYS AS ((`a` - 1)) VIRTUAL,
  `d` int GENERATED ALWAYS AS ((`a` * 2)) STORED,
  `e` int GENERATED ALWAYS AS (-(`a`)) VIRTUAL,
  `f` int GENERATED ALWAYS AS (`a`) VIRTUAL,
  `n` int GENERATED ALWAYS AS (NULL) VIRTUAL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "SHOW CREATE TABLE generated columns" \
    "$show_create_expected" \
    "SHOW CREATE TABLE exprs;" \
    "$DATABASE"

information_schema_expected=$(printf '%s\n' \
    $'a\tNULL\tYES\t\t\t' \
    $'b\tNULL\tYES\tVIRTUAL GENERATED\t(`a` + 1)\t' \
    $'c\tNULL\tYES\tVIRTUAL GENERATED\t(`a` - 1)\t' \
    $'d\tNULL\tYES\tSTORED GENERATED\t(`a` * 2)\t' \
    $'e\tNULL\tYES\tVIRTUAL GENERATED\t-(`a`)\t' \
    $'f\tNULL\tYES\tVIRTUAL GENERATED\t`a`\t' \
    $'n\tNULL\tYES\tVIRTUAL GENERATED\tNULL\t')
expect_output \
    "INFORMATION_SCHEMA generated expressions" \
    "$information_schema_expected" \
    "SELECT COLUMN_NAME, COLUMN_DEFAULT, IS_NULLABLE, EXTRA, GENERATION_EXPRESSION, "\
"COLUMN_COMMENT FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA='${DATABASE}' "\
"AND TABLE_NAME='exprs' ORDER BY ORDINAL_POSITION;" \
    "$DATABASE"

dml_expected=$(cat <<\EXPECTED
0	0
2	3
5	6
20	21
EXPECTED
)
expect_output \
    "insert default slots and update recomputation" \
    "$dml_expected" \
    "CREATE TABLE dml (a INT, b INT AS (a + 1)); "\
"INSERT INTO dml(a) VALUES (1); "\
"INSERT INTO dml VALUES (2, DEFAULT); "\
"INSERT INTO dml(b,a) VALUES (DEFAULT,5); "\
"UPDATE dml SET a = 20 WHERE a = 1; "\
"UPDATE dml SET b = DEFAULT WHERE a = 20; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT a,b FROM dml ORDER BY a;" \
    "$DATABASE"

not_null_expected=$(cat <<\EXPECTED
1	2
EXPECTED
)
expect_output \
    "generated NOT NULL accepted after expression" \
    "$not_null_expected" \
    "CREATE TABLE generated_not_null (a INT, b INT AS (a + 1) NOT NULL); "\
"INSERT INTO generated_not_null(a) VALUES (1); "\
"SELECT a,b FROM generated_not_null;" \
    "$DATABASE"

expect_error \
    "generated not null computed null" \
    1048 \
    23000 \
    "Column 'b' cannot be null" \
    "INSERT INTO generated_not_null(a) VALUES (NULL);" \
    "$DATABASE"

expect_error \
    "generated not null names computed column" \
    1048 \
    23000 \
    "Column 'c' cannot be null" \
    "CREATE TABLE generated_not_null_names ("\
"a INT, b INT AS (a + 1) NOT NULL, c INT AS (NULL) NOT NULL); "\
"INSERT INTO generated_not_null_names(a) VALUES (1);" \
    "$DATABASE"

expect_error \
    "generated tinyint out of range" \
    1264 \
    22003 \
    "Out of range value for column 'b' at row 1" \
    "CREATE TABLE generated_tinyint_range (a INT, b TINYINT AS (a + 1000)); "\
"INSERT INTO generated_tinyint_range(a) VALUES (1);" \
    "$DATABASE"

expect_error \
    "generated unsigned out of range" \
    1264 \
    22003 \
    "Out of range value for column 'b' at row 1" \
    "CREATE TABLE generated_unsigned_range (a INT, b BIGINT UNSIGNED AS (-a)); "\
"INSERT INTO generated_unsigned_range(a) VALUES (1);" \
    "$DATABASE"

multi_default_expected=$(cat <<\EXPECTED
20	21	40	0	0
EXPECTED
)
expect_output \
    "generated DEFAULT in multiple assignments" \
    "$multi_default_expected" \
    "CREATE TABLE generated_multi_default ("\
"id INT, a INT, b INT AS (a + 1), c INT AS (a * 2)); "\
"INSERT INTO generated_multi_default(id,a) VALUES (1,10); "\
"UPDATE generated_multi_default SET a = 20, b = DEFAULT WHERE id = 1; "\
"UPDATE generated_multi_default SET b = DEFAULT, c = DEFAULT WHERE id = 1; "\
"SELECT a,b,c,ROW_COUNT(),@@warning_count FROM generated_multi_default WHERE id = 1;" \
    "$DATABASE"

expect_error \
    "explicit generated insert value" \
    3105 \
    HY000 \
    "The value specified for generated column 'b' in table 'dml' is not allowed." \
    "INSERT INTO dml VALUES (3, 4);" \
    "$DATABASE"

expect_error \
    "explicit generated update value" \
    3105 \
    HY000 \
    "The value specified for generated column 'b' in table 'dml' is not allowed." \
    "UPDATE dml SET b = 7 WHERE a = 2;" \
    "$DATABASE"

expect_error \
    "generated default attribute" \
    1221 \
    HY000 \
    "Incorrect usage of DEFAULT and generated column" \
    "CREATE TABLE bad_default (a INT, b INT AS (a + 1) DEFAULT 7);" \
    "$DATABASE"

expect_error \
    "generated auto increment attribute" \
    1221 \
    HY000 \
    "Incorrect usage of AUTO_INCREMENT and generated column" \
    "CREATE TABLE bad_auto (a INT, b INT AS (a + 1) AUTO_INCREMENT);" \
    "$DATABASE"

expect_error \
    "unknown generated expression column" \
    1054 \
    42S22 \
    "Unknown column 'unknown' in 'generated column function'" \
    "CREATE TABLE bad_unknown (a INT, b INT AS (unknown + 1));" \
    "$DATABASE"

expect_error \
    "generated subquery expression" \
    3102 \
    HY000 \
    "Expression of generated column 'b' contains a disallowed function." \
    "CREATE TABLE bad_subquery (a INT, b INT AS ((SELECT 1)));" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_generated_column_lifecycle_expectations: ok"
