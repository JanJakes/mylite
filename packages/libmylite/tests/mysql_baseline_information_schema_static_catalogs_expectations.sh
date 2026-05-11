#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_information_schema_static_catalogs_expectations: $1" >&2
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

expect_count_greater_than_one() {
    label=$1
    sql=$2

    output=$(run_mysql "$sql")
    case "$output" in
        ''|*[!0-9]*)
            fail "$label: expected numeric count, got [$output]"
            ;;
        0|1)
            fail "$label: expected count greater than one, got [$output]"
            ;;
        *) ;;
    esac
}

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

engines_expected=$(printf '%b' \
"InnoDB\tDEFAULT\tSupports transactions, row-level locking, and foreign keys\tYES\tYES\tYES")
expect_output \
    "innodb engine row" \
    "$engines_expected" \
    "SELECT * FROM INFORMATION_SCHEMA.ENGINES WHERE ENGINE = 'InnoDB';"

character_sets_expected=$(printf '%b' "utf8mb4\tutf8mb4_0900_ai_ci\tUTF-8 Unicode\t4")
expect_output \
    "utf8mb4 character set row" \
    "$character_sets_expected" \
    "SELECT * FROM INFORMATION_SCHEMA.CHARACTER_SETS "\
"WHERE CHARACTER_SET_NAME = 'utf8mb4';"

collations_expected=$(printf '%b' "utf8mb4_0900_ai_ci\tutf8mb4\t255\tYes\tYes\t0\tNO PAD")
expect_output \
    "utf8mb4 default collation row" \
    "$collations_expected" \
    "SELECT * FROM INFORMATION_SCHEMA.COLLATIONS "\
"WHERE COLLATION_NAME = 'utf8mb4_0900_ai_ci';"

status_output=$(run_mysql \
    "SELECT * FROM INFORMATION_SCHEMA.ENGINES WHERE ENGINE = 'InnoDB'; "\
"SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
status_expected=$(printf '%b' "0\t-1")
if [ "$status_output" != "$status_expected" ]; then
    fail "successful static catalog select status: expected [$status_expected], got [$status_output]"
fi

case_predicates_expected=$(printf '%b' "InnoDB\nutf8mb4\nutf8mb4_0900_ai_ci")
expect_output \
    "case insensitive metadata predicates" \
    "$case_predicates_expected" \
    "SELECT ENGINE FROM INFORMATION_SCHEMA.ENGINES WHERE ENGINE = 'innodb'; "\
"SELECT CHARACTER_SET_NAME FROM INFORMATION_SCHEMA.CHARACTER_SETS "\
"WHERE CHARACTER_SET_NAME = 'UTF8MB4'; "\
"SELECT COLLATION_NAME FROM INFORMATION_SCHEMA.COLLATIONS "\
"WHERE COLLATION_NAME = 'UTF8MB4_0900_AI_CI';"

system_tables_expected=$(cat <<\EXPECTED
information_schema	CHARACTER_SETS	SYSTEM VIEW	NULL	10	NULL	0
information_schema	COLLATIONS	SYSTEM VIEW	NULL	10	NULL	0
information_schema	ENGINES	SYSTEM VIEW	NULL	10	NULL	0
EXPECTED
)
expect_output \
    "information schema static catalog system table rows" \
    "$system_tables_expected" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, TABLE_ROWS "\
"FROM INFORMATION_SCHEMA.TABLES "\
"WHERE TABLE_SCHEMA = 'information_schema' "\
"AND (TABLE_NAME = 'ENGINES' OR TABLE_NAME = 'CHARACTER_SETS' "\
"OR TABLE_NAME = 'COLLATIONS') ORDER BY TABLE_NAME;"

character_sets_columns_expected=$(cat <<\EXPECTED
CHARACTER_SET_NAME	1	NULL	NO	varchar	64	192	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(64)			select			NULL
DEFAULT_COLLATE_NAME	2	NULL	NO	varchar	64	192	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(64)			select			NULL
DESCRIPTION	3	NULL	NO	varchar	2048	6144	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(2048)			select			NULL
MAXLEN	4	NULL	NO	int	NULL	NULL	10	0	NULL	NULL	int unsigned			select			NULL
EXPECTED
)
expect_output \
    "character sets system column rows" \
    "$character_sets_columns_expected" \
    "SELECT COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE, DATA_TYPE, "\
"CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, NUMERIC_PRECISION, "\
"NUMERIC_SCALE, CHARACTER_SET_NAME, COLLATION_NAME, COLUMN_TYPE, COLUMN_KEY, "\
"EXTRA, PRIVILEGES, COLUMN_COMMENT, GENERATION_EXPRESSION, SRS_ID "\
"FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = 'information_schema' "\
"AND TABLE_NAME = 'CHARACTER_SETS' ORDER BY ORDINAL_POSITION;"

collations_columns_expected=$(cat <<\EXPECTED
COLLATION_NAME	1	NULL	NO	varchar	64	192	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(64)			select			NULL
CHARACTER_SET_NAME	2	NULL	NO	varchar	64	192	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(64)			select			NULL
ID	3	0	NO	bigint	NULL	NULL	20	0	NULL	NULL	bigint unsigned			select			NULL
IS_DEFAULT	4		NO	varchar	3	9	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(3)			select			NULL
IS_COMPILED	5		NO	varchar	3	9	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(3)			select			NULL
SORTLEN	6	NULL	NO	int	NULL	NULL	10	0	NULL	NULL	int unsigned			select			NULL
PAD_ATTRIBUTE	7	NULL	NO	enum	9	27	NULL	NULL	utf8mb3	utf8mb3_bin	enum('PAD SPACE','NO PAD')			select			NULL
EXPECTED
)
expect_output \
    "collations system column rows" \
    "$collations_columns_expected" \
    "SELECT COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE, DATA_TYPE, "\
"CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, NUMERIC_PRECISION, "\
"NUMERIC_SCALE, CHARACTER_SET_NAME, COLLATION_NAME, COLUMN_TYPE, COLUMN_KEY, "\
"EXTRA, PRIVILEGES, COLUMN_COMMENT, GENERATION_EXPRESSION, SRS_ID "\
"FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = 'information_schema' "\
"AND TABLE_NAME = 'COLLATIONS' ORDER BY ORDINAL_POSITION;"

engines_columns_expected=$(cat <<\EXPECTED
ENGINE	1		NO	varchar	21	64	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(64)			select			NULL
SUPPORT	2		NO	varchar	2	8	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(8)			select			NULL
COMMENT	3		NO	varchar	26	80	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(80)			select			NULL
TRANSACTIONS	4		YES	varchar	1	3	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(3)			select			NULL
XA	5		YES	varchar	1	3	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(3)			select			NULL
SAVEPOINTS	6		YES	varchar	1	3	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(3)			select			NULL
EXPECTED
)
expect_output \
    "engines system column rows" \
    "$engines_columns_expected" \
    "SELECT COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE, DATA_TYPE, "\
"CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, NUMERIC_PRECISION, "\
"NUMERIC_SCALE, CHARACTER_SET_NAME, COLLATION_NAME, COLUMN_TYPE, COLUMN_KEY, "\
"EXTRA, PRIVILEGES, COLUMN_COMMENT, GENERATION_EXPRESSION, SRS_ID "\
"FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = 'information_schema' "\
"AND TABLE_NAME = 'ENGINES' ORDER BY ORDINAL_POSITION;"

expect_count_greater_than_one \
    "mysql full engine catalog remains wider than MyLite slice" \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.ENGINES;"
expect_count_greater_than_one \
    "mysql full character set catalog remains wider than MyLite slice" \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.CHARACTER_SETS;"
expect_count_greater_than_one \
    "mysql full collation catalog remains wider than MyLite slice" \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLLATIONS;"
