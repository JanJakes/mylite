#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_information_schema_partitions_$$"

fail() {
    printf '%s\n' "mysql_baseline_information_schema_partitions_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | "$MYSQL_BIN" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                --batch --raw --skip-column-names "$@"
    else
        printf '%s\n' "$sql" \
            | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
                --batch --raw --skip-column-names "$@"
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

trap cleanup EXIT

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE}; "\
"CREATE TABLE plain (id INT PRIMARY KEY, v VARCHAR(10)); "\
"INSERT INTO plain VALUES (1,'a'),(2,'b'),(3,'c');" >/dev/null

nonpartitioned_expected=$(printf '%b' \
"def\t${DATABASE}\tplain\t1\t1\t1\t1\t1\t1\t3\t16384\t0\t0\t0\t\t\t1")
expect_output \
    "nonpartitioned table has one null partition row" \
    "$nonpartitioned_expected" \
    "SELECT TABLE_CATALOG, TABLE_SCHEMA, TABLE_NAME, PARTITION_NAME IS NULL, "\
"SUBPARTITION_NAME IS NULL, PARTITION_ORDINAL_POSITION IS NULL, "\
"SUBPARTITION_ORDINAL_POSITION IS NULL, PARTITION_METHOD IS NULL, "\
"PARTITION_DESCRIPTION IS NULL, TABLE_ROWS, DATA_LENGTH, MAX_DATA_LENGTH, "\
"INDEX_LENGTH, DATA_FREE, PARTITION_COMMENT, NODEGROUP, TABLESPACE_NAME IS NULL "\
"FROM INFORMATION_SCHEMA.PARTITIONS WHERE TABLE_SCHEMA = '${DATABASE}' "\
"AND TABLE_NAME = 'plain';"

status_expected=$(printf '%b' "1\n0\t-1")
expect_output \
    "successful partitions read status" \
    "$status_expected" \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.PARTITIONS WHERE TABLE_SCHEMA = '${DATABASE}'; "\
"SELECT @@warning_count, ROW_COUNT();"

system_row_expected=$(printf '%b' "def\tinformation_schema\tPARTITIONS\t1\t0\t0\t\t\t1")
expect_output \
    "information schema partitions system row" \
    "$system_row_expected" \
    "SELECT TABLE_CATALOG, TABLE_SCHEMA, TABLE_NAME, PARTITION_NAME IS NULL, "\
"TABLE_ROWS, DATA_LENGTH, PARTITION_COMMENT, NODEGROUP, TABLESPACE_NAME IS NULL "\
"FROM INFORMATION_SCHEMA.PARTITIONS WHERE TABLE_SCHEMA = 'information_schema' "\
"AND TABLE_NAME = 'PARTITIONS';"

system_table_expected=$(printf '%b' "information_schema\tPARTITIONS\tSYSTEM VIEW\tNULL\t10\tNULL\t0")
expect_output \
    "partitions system table metadata" \
    "$system_table_expected" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, TABLE_ROWS "\
"FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'information_schema' "\
"AND TABLE_NAME = 'PARTITIONS';"

columns_expected=$(cat <<\EXPECTED
TABLE_CATALOG	1	NULL	YES	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_tolower_ci	varchar(64)			select			NULL
TABLE_SCHEMA	2	NULL	YES	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_tolower_ci	varchar(64)			select			NULL
TABLE_NAME	3	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_tolower_ci	varchar(64)			select			NULL
PARTITION_NAME	4	NULL	YES	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_tolower_ci	varchar(64)			select			NULL
SUBPARTITION_NAME	5	NULL	YES	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_tolower_ci	varchar(64)			select			NULL
PARTITION_ORDINAL_POSITION	6	NULL	YES	int	NULL	NULL	10	0	NULL	NULL	NULL	int unsigned			select			NULL
SUBPARTITION_ORDINAL_POSITION	7	NULL	YES	int	NULL	NULL	10	0	NULL	NULL	NULL	int unsigned			select			NULL
PARTITION_METHOD	8	NULL	YES	varchar	13	39	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(13)			select			NULL
SUBPARTITION_METHOD	9	NULL	YES	varchar	13	39	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(13)			select			NULL
PARTITION_EXPRESSION	10	NULL	YES	varchar	2048	6144	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(2048)			select			NULL
SUBPARTITION_EXPRESSION	11	NULL	YES	varchar	2048	6144	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(2048)			select			NULL
PARTITION_DESCRIPTION	12	NULL	YES	text	65535	65535	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	text			select			NULL
TABLE_ROWS	13	NULL	YES	bigint	NULL	NULL	20	0	NULL	NULL	NULL	bigint unsigned			select			NULL
AVG_ROW_LENGTH	14	NULL	YES	bigint	NULL	NULL	20	0	NULL	NULL	NULL	bigint unsigned			select			NULL
DATA_LENGTH	15	NULL	YES	bigint	NULL	NULL	20	0	NULL	NULL	NULL	bigint unsigned			select			NULL
MAX_DATA_LENGTH	16	NULL	YES	bigint	NULL	NULL	20	0	NULL	NULL	NULL	bigint unsigned			select			NULL
INDEX_LENGTH	17	NULL	YES	bigint	NULL	NULL	20	0	NULL	NULL	NULL	bigint unsigned			select			NULL
DATA_FREE	18	NULL	YES	bigint	NULL	NULL	20	0	NULL	NULL	NULL	bigint unsigned			select			NULL
CREATE_TIME	19	NULL	NO	timestamp	NULL	NULL	NULL	NULL	0	NULL	NULL	timestamp			select			NULL
UPDATE_TIME	20	NULL	YES	datetime	NULL	NULL	NULL	NULL	0	NULL	NULL	datetime			select			NULL
CHECK_TIME	21	NULL	YES	datetime	NULL	NULL	NULL	NULL	0	NULL	NULL	datetime			select			NULL
CHECKSUM	22	NULL	YES	bigint	NULL	NULL	19	0	NULL	NULL	NULL	bigint			select			NULL
PARTITION_COMMENT	23	NULL	NO	text	65535	65535	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	text			select			NULL
NODEGROUP	24	NULL	YES	varchar	256	768	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(256)			select			NULL
TABLESPACE_NAME	25	NULL	YES	varchar	268	804	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(268)			select			NULL
EXPECTED
)
expect_output \
    "partitions system column metadata" \
    "$columns_expected" \
    "SELECT COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE, DATA_TYPE, "\
"CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, NUMERIC_PRECISION, NUMERIC_SCALE, "\
"DATETIME_PRECISION, CHARACTER_SET_NAME, COLLATION_NAME, COLUMN_TYPE, COLUMN_KEY, "\
"EXTRA, PRIVILEGES, COLUMN_COMMENT, GENERATION_EXPRESSION, SRS_ID "\
"FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = 'information_schema' "\
"AND TABLE_NAME = 'PARTITIONS' ORDER BY ORDINAL_POSITION;"

partitioned_expected=$(cat <<\EXPECTED
p0	1	RANGE	`id`	10
pmax	2	RANGE	`id`	MAXVALUE
EXPECTED
)
expect_output \
    "mysql partitioned tables have real partition rows outside MyLite slice" \
    "$partitioned_expected" \
    "USE ${DATABASE}; CREATE TABLE p_range (id INT, v INT) PARTITION BY RANGE(id) "\
"(PARTITION p0 VALUES LESS THAN (10), PARTITION pmax VALUES LESS THAN MAXVALUE); "\
"SELECT PARTITION_NAME, PARTITION_ORDINAL_POSITION, PARTITION_METHOD, "\
"PARTITION_EXPRESSION, PARTITION_DESCRIPTION FROM INFORMATION_SCHEMA.PARTITIONS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'p_range' "\
"ORDER BY PARTITION_ORDINAL_POSITION;"

expect_error \
    "unknown projected partitions column" \
    1054 \
    42S22 \
    "Unknown column 'MISSING_COLUMN' in 'field list'" \
    "SELECT MISSING_COLUMN FROM INFORMATION_SCHEMA.PARTITIONS;"

cleanup

printf '%s\n' "mysql_baseline_information_schema_partitions_expectations: ok"
