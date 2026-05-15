#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_information_schema_keywords_expectations: $1" >&2
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

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

counts_expected=$(printf '%b' "734\t259")
expect_output \
    "keyword and reserved counts" \
    "$counts_expected" \
    "SELECT COUNT(*), COUNT(CASE WHEN RESERVED = 1 THEN 1 END) "\
"FROM INFORMATION_SCHEMA.KEYWORDS;"

first_rows_expected=$(cat <<\EXPECTED
ACCESSIBLE	1
ACCOUNT	0
ACTION	0
EXPECTED
)
expect_output \
    "first keyword rows" \
    "$first_rows_expected" \
    "SELECT WORD, RESERVED FROM INFORMATION_SCHEMA.KEYWORDS ORDER BY WORD LIMIT 3;"

representative_expected=$(cat <<\EXPECTED
DATABASE	1
KEY	1
RANDOM	0
SELECT	1
VALUE	0
WINDOW	1
EXPECTED
)
expect_output \
    "representative keyword rows" \
    "$representative_expected" \
    "SELECT WORD, RESERVED FROM INFORMATION_SCHEMA.KEYWORDS "\
"WHERE WORD = 'DATABASE' OR WORD = 'KEY' OR WORD = 'RANDOM' OR WORD = 'SELECT' "\
"OR WORD = 'VALUE' OR WORD = 'WINDOW' ORDER BY WORD;"

case_predicate_expected=$(printf '%b' "SELECT\t1")
expect_output \
    "case-insensitive word predicate" \
    "$case_predicate_expected" \
    "SELECT WORD, RESERVED FROM INFORMATION_SCHEMA.KEYWORDS WHERE WORD = 'select';"

reserved_order_expected=$(cat <<\EXPECTED
ACCESSIBLE
ADD
ALL
ALTER
ANALYZE
EXPECTED
)
expect_output \
    "reserved keyword ordering" \
    "$reserved_order_expected" \
    "SELECT WORD FROM INFORMATION_SCHEMA.KEYWORDS WHERE RESERVED = 1 ORDER BY WORD LIMIT 5;"

nonreserved_order_expected=$(cat <<\EXPECTED
ACCOUNT
ACTION
ACTIVE
ADMIN
AFTER
EXPECTED
)
expect_output \
    "nonreserved keyword ordering" \
    "$nonreserved_order_expected" \
    "SELECT WORD FROM INFORMATION_SCHEMA.KEYWORDS WHERE RESERVED = 0 ORDER BY WORD LIMIT 5;"

system_table_expected=$(printf '%b' "information_schema\tKEYWORDS\tSYSTEM VIEW\tNULL\t10\tNULL\t0\tNULL")
expect_output \
    "keywords system table row" \
    "$system_table_expected" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, "\
"TABLE_ROWS, TABLE_COLLATION FROM INFORMATION_SCHEMA.TABLES "\
"WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'KEYWORDS';"

columns_expected=$(cat <<\EXPECTED
WORD	1	NULL	YES	varchar	128	512	NULL	NULL	utf8mb4	utf8mb4_0900_ai_ci	varchar(128)			select			NULL
RESERVED	2	NULL	YES	int	NULL	NULL	10	0	NULL	NULL	int			select			NULL
EXPECTED
)
expect_output \
    "keywords system column rows" \
    "$columns_expected" \
    "SELECT COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE, DATA_TYPE, "\
"CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, NUMERIC_PRECISION, "\
"NUMERIC_SCALE, CHARACTER_SET_NAME, COLLATION_NAME, COLUMN_TYPE, COLUMN_KEY, "\
"EXTRA, PRIVILEGES, COLUMN_COMMENT, GENERATION_EXPRESSION, SRS_ID "\
"FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = 'information_schema' "\
"AND TABLE_NAME = 'KEYWORDS' ORDER BY ORDINAL_POSITION;"

status_output=$(run_mysql \
    "SELECT WORD FROM INFORMATION_SCHEMA.KEYWORDS WHERE WORD = 'SELECT'; "\
"SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
status_expected=$(printf '%b' "0\t-1")
if [ "$status_output" != "$status_expected" ]; then
    fail "successful keyword select status: expected [$status_expected], got [$status_output]"
fi

printf '%s\n' "mysql_baseline_information_schema_keywords_expectations: ok"
