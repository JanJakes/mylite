#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_sys_helper_functions_expectations: $1" >&2
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

expect_error() {
    label=$1
    sql=$2
    expected=$3

    set +e
    output=$(run_mysql "$sql" --show-warnings 2>&1)
    rc=$?
    set -e
    if [ "$rc" -eq 0 ]; then
        fail "$label: expected error, got success"
    fi
    case "$output" in
        *"$expected"*) ;;
        *) fail "$label: expected error containing [$expected], got [$output]" ;;
    esac
}

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

helper_values_expected=$(
    printf '%b' \
        '   0 bytes\t   1 bytes\t1.50 KiB\t0 ps\t3.5 ns\tworld\tCity\t`a``b`\t64\tfallback\tfallback\tfallback\t8.4.9'
)
expect_output \
    "sys helper direct values" \
    "$helper_values_expected" \
    "SELECT
        sys.format_bytes(0),
        sys.format_bytes(1),
        sys.format_bytes(1536),
        sys.format_time(0),
        sys.format_time(3501),
        sys.extract_schema_from_file_name('/usr/local/mysql/data/world/City.ibd'),
        sys.extract_table_from_file_name('/usr/local/mysql/data/world/City.ibd'),
        sys.quote_identifier('a\`b'),
        sys.sys_get_config('statement_truncate_len','fallback'),
        sys.sys_get_config('missing','fallback'),
        sys.sys_get_config(NULL,'fallback'),
        sys.sys_get_config('statement_performance_analyzer.view','fallback'),
        CONCAT(sys.version_major(),'.',sys.version_minor(),'.',sys.version_patch());"

path_values_expected=$(
    printf '%b' '@@datadir/world/City.ibd\t@@tmpdir/abc\t@@basedir/local/mysql/data/world/City.ibd\tplain'
)
expect_output \
    "sys.format_path values" \
    "$path_values_expected" \
    "SELECT
        sys.format_path(CONCAT(@@datadir,'world/City.ibd')),
        sys.format_path('/tmp/abc'),
        sys.format_path('/usr/local/mysql/data/world/City.ibd'),
        sys.format_path('plain');"

statement_values_expected=$(
    printf '%b' 'SELECT 1\tSELECT variabl ... ROM sys_config\t64'
)
expect_output \
    "sys.format_statement values" \
    "$statement_values_expected" \
    "SET @sys.statement_truncate_len=32;
     SELECT
        sys.format_statement('SELECT 1'),
        sys.format_statement('SELECT variable, value, set_time FROM sys_config'),
        sys.sys_get_config('statement_truncate_len','fallback');"

list_values_expected=$(
    printf '%b' 'x\tx\ta,b,c\ta,b,c\tNULL\t\t b, c'
)
expect_output \
    "sys list helper values" \
    "$list_values_expected" \
    "SELECT
        sys.list_add(NULL,'x'),
        sys.list_add('','x'),
        sys.list_add('a,b','c'),
        sys.list_add('a,b','c'),
        sys.list_drop(NULL,'x'),
        sys.list_drop('','x'),
        sys.list_drop('a, b, c','a');"

null_values_expected=$(
    printf '%b' 'NULL\tNULL\tNULL\tNULL\tNULL\tNULL'
)
expect_output \
    "sys helper null values" \
    "$null_values_expected" \
    "SELECT
        sys.format_bytes(NULL),
        sys.format_time(NULL),
        sys.format_path(NULL),
        sys.format_statement(NULL),
        sys.extract_schema_from_file_name(NULL),
        sys.quote_identifier(NULL);"

expect_error \
    "sys.list_add NULL value diagnostic" \
    "SELECT sys.list_add('a,b', NULL);" \
    "ERROR 1138 (02200)"
expect_error \
    "sys.list_drop NULL value diagnostic" \
    "SELECT sys.list_drop('a,b', NULL);" \
    "ERROR 1138 (02200)"

printf '%s\n' "mysql_baseline_sys_helper_functions_expectations: ok"
