#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_builtin_schema_table_directory_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names "$@"
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
    sql=$3
    shift 3

    output=$(run_mysql "$sql" "$@")
    case "$output" in
        *"$needle"*) ;;
        *) fail "$label: expected output containing [$needle], got [$output]" ;;
    esac
}

append_tsv_row() {
    printf '%s' "$1"
    shift
    while [ "$#" -gt 0 ]; do
        printf '\t%s' "$1"
        shift
    done
    printf '\n'
}

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

counts_expected=$(cat <<\EXPECTED
information_schema	78	1feb3d9b1aaf492c7b5a41e273627bef02b30f309e4b1a89185e45edf46d346b
mysql	38	2822370c2fb092cf5f80de8bbfba03a94cb5c1bb577931810e582dd0b03eff22
performance_schema	114	a9d6480e48494356bb86c550487dfd0f56a445fb7d14c02a6b57ec1c4fe361ca
sys	101	148115307826b6c6a0c7140932785cada503b7e0fc892e886670fd93da6fcba1
EXPECTED
)
expect_output \
    "built-in table counts and per-schema hashes" \
    "$counts_expected" \
    "SET SESSION group_concat_max_len = 1000000;
     SELECT TABLE_SCHEMA, COUNT(*),
            SHA2(GROUP_CONCAT(CONCAT_WS('|', TABLE_NAME, TABLE_TYPE)
                               ORDER BY TABLE_NAME SEPARATOR '\n'), 256)
       FROM INFORMATION_SCHEMA.TABLES
      WHERE TABLE_SCHEMA IN ('information_schema','mysql','performance_schema','sys')
      GROUP BY TABLE_SCHEMA
      ORDER BY TABLE_SCHEMA;"

expect_output \
    "combined built-in table hash" \
    "adc4825f20567d48cffc1d402fa967b42104818a993c9265118b83c37f30c75d" \
    "SET SESSION group_concat_max_len = 1000000;
     SELECT SHA2(GROUP_CONCAT(CONCAT_WS('|', TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE)
                              ORDER BY TABLE_SCHEMA, TABLE_NAME SEPARATOR '\n'), 256)
      FROM INFORMATION_SCHEMA.TABLES
     WHERE TABLE_SCHEMA IN ('information_schema','mysql','performance_schema','sys');"

expect_output \
    "built-in table create time count" \
    "331" \
    "SELECT COUNT(*)
       FROM INFORMATION_SCHEMA.TABLES
      WHERE TABLE_SCHEMA IN ('information_schema','mysql','performance_schema','sys')
        AND CREATE_TIME IS NOT NULL;"

representatives_expected=$(
    append_tsv_row information_schema TABLES "SYSTEM VIEW" NULL 10 NULL 0 NULL "" ""
    append_tsv_row mysql user "BASE TABLE" InnoDB 10 Dynamic 16384 utf8mb3_bin \
        "row_format=DYNAMIC stats_persistent=0" "Users and global privileges"
    append_tsv_row performance_schema setup_actors "BASE TABLE" PERFORMANCE_SCHEMA 10 Fixed 0 \
        utf8mb4_0900_ai_ci "" ""
    append_tsv_row sys sys_config "BASE TABLE" InnoDB 10 Dynamic 16384 utf8mb4_0900_ai_ci "" ""
    append_tsv_row sys version VIEW NULL NULL NULL NULL NULL NULL VIEW
)
expect_output \
    "representative built-in information schema table rows" \
    "$representatives_expected" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT,
            DATA_LENGTH, TABLE_COLLATION, CREATE_OPTIONS, TABLE_COMMENT
       FROM INFORMATION_SCHEMA.TABLES
      WHERE (TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'TABLES')
         OR (TABLE_SCHEMA = 'mysql' AND TABLE_NAME = 'user')
         OR (TABLE_SCHEMA = 'performance_schema' AND TABLE_NAME = 'setup_actors')
         OR (TABLE_SCHEMA = 'sys' AND TABLE_NAME IN ('sys_config','version'))
      ORDER BY TABLE_SCHEMA, TABLE_NAME;"

show_full_expected=$(cat <<\EXPECTED
TABLES	SYSTEM VIEW
user	BASE TABLE
setup_actors	BASE TABLE
sys_config	BASE TABLE
version	VIEW
EXPECTED
)
expect_output \
    "show full tables representative built-ins" \
    "$show_full_expected" \
    "SHOW FULL TABLES FROM information_schema LIKE 'TABLES';
     SHOW FULL TABLES FROM mysql LIKE 'user';
     SHOW FULL TABLES FROM performance_schema LIKE 'setup_actors';
     SHOW FULL TABLES FROM sys WHERE Tables_in_sys IN ('sys_config', 'version');"

expect_contains \
    "show table status information schema" \
    "TABLES	NULL	10	NULL	0	0	0	0	0	0	NULL" \
    "SHOW TABLE STATUS FROM information_schema LIKE 'TABLES';"
expect_contains \
    "show table status mysql user" \
    "user	InnoDB	10	Dynamic" \
    "SHOW TABLE STATUS FROM mysql LIKE 'user';"
expect_contains \
    "show table status performance schema setup actors" \
    "setup_actors	PERFORMANCE_SCHEMA	10	Fixed" \
    "SHOW TABLE STATUS FROM performance_schema LIKE 'setup_actors';"
expect_contains \
    "show table status sys version" \
    "version	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL" \
    "SHOW TABLE STATUS FROM sys LIKE 'version';"

printf '%s\n' "mysql_baseline_builtin_schema_table_directory_expectations: ok"
