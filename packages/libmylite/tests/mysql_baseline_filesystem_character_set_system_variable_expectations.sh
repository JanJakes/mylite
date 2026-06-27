#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_ARGS="--protocol=TCP -h127.0.0.1 -uroot --batch --raw --default-character-set=utf8mb4"
DATABASE="mylite_filesystem_charset_vars_$$"

fail() {
    printf '%s\n' "mysql_baseline_filesystem_character_set_system_variable_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql $MYSQL_ARGS --skip-column-names "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql $MYSQL_ARGS "$@"
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

expect_output_with_headers() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql_with_headers "$sql" "$@")
    if [ "$output" != "$expected" ]; then
        fail "$label: expected [$expected], got [$output]"
    fi
}

expect_value() {
    label=$1
    expected=$2
    actual=$3

    if [ "$actual" != "$expected" ]; then
        fail "$label: expected [$expected], got [$actual]"
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

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup

expected_values="binary	binary	binary	binary	0	-1"
values=$(run_mysql \
    "SELECT 1; SELECT @@character_set_filesystem, @@global.character_set_filesystem, \
     @@session.character_set_filesystem, @@local.character_set_filesystem, \
     @@warning_count, ROW_COUNT();" \
    | tail -n 1)
expect_value "filesystem charset variables and diagnostics" "$expected_values" "$values"

expected_headers=$(cat <<EOF
@@character_set_filesystem	@@global.\`character_set_filesystem\`	@@session.\`character_set_filesystem\`	@@\`character_set_filesystem\`
binary	binary	binary	binary
EOF
)
expect_output_with_headers \
    "filesystem charset labels preserve source text" \
    "$expected_headers" \
    "SELECT @@character_set_filesystem, @@global.\`character_set_filesystem\`, \
     @@session.\`character_set_filesystem\`, @@\`character_set_filesystem\`;"

expect_output \
    "case-insensitive filesystem charset variables" \
    "binary	binary" \
    "SELECT @@CHARACTER_SET_FILESYSTEM, @@GLOBAL.CHARACTER_SET_FILESYSTEM;"

expect_output \
    "from dual returns filesystem charset variable" \
    "binary" \
    "SELECT @@character_set_filesystem FROM DUAL;"

expect_output \
    "selected database does not change filesystem charset variable" \
    "binary	${DATABASE}" \
    "CREATE DATABASE ${DATABASE}; USE ${DATABASE}; \
     SELECT @@character_set_filesystem, DATABASE();"

mutable_values=$(run_mysql \
    "SELECT @@character_set_filesystem, @@global.character_set_filesystem; \
     SET SESSION character_set_filesystem=utf8mb4; \
     SELECT @@character_set_filesystem, @@global.character_set_filesystem, \
            @@session.character_set_filesystem, @@local.character_set_filesystem, \
            @@warning_count, ROW_COUNT(); \
     SET SESSION character_set_filesystem=binary;" \
    | tail -n 1)
expect_value \
    "mysql session filesystem charset is mutable upstream" \
    "utf8mb4	binary	utf8mb4	utf8mb4	0	0" \
    "$mutable_values"

assignment_values=$(run_mysql \
    "SET SESSION character_set_filesystem=utf8; \
     SELECT @@character_set_filesystem, @@global.character_set_filesystem, \
            @@session.character_set_filesystem, @@local.character_set_filesystem, \
            @@warning_count, ROW_COUNT(); \
     SET LOCAL character_set_filesystem='latin2'; \
     SELECT @@character_set_filesystem, @@global.character_set_filesystem, \
            @@session.character_set_filesystem, @@local.character_set_filesystem, \
            @@warning_count, ROW_COUNT(); \
     SET character_set_filesystem=+33; \
     SELECT @@character_set_filesystem, @@global.character_set_filesystem, \
            @@session.character_set_filesystem, @@local.character_set_filesystem, \
            @@warning_count, ROW_COUNT(); \
     SET @filesystem_charset_id = 255; \
     SET character_set_filesystem=@filesystem_charset_id; \
     SELECT @@character_set_filesystem, @@global.character_set_filesystem, \
            @@session.character_set_filesystem, @@local.character_set_filesystem, \
            @@warning_count, ROW_COUNT(); \
     SET character_set_filesystem=DEFAULT; \
     SELECT @@character_set_filesystem, @@global.character_set_filesystem, \
            @@session.character_set_filesystem, @@local.character_set_filesystem, \
            @@warning_count, ROW_COUNT();" \
    | tail -n 5 \
    | tr '\n' '|')
expect_value \
    "filesystem charset assignment canonicalization and collation ids" \
    "utf8mb3	binary	utf8mb3	utf8mb3	1	0|latin2	binary	latin2	latin2	0	0|utf8mb3	binary	utf8mb3	utf8mb3	1	0|utf8mb4	binary	utf8mb4	utf8mb4	0	0|binary	binary	binary	binary	0	0|" \
    "$assignment_values"

warning_values=$(run_mysql \
    "SELECT 1; SHOW PROCESSLIST; \
     SELECT @@character_set_filesystem, @@warning_count, @@error_count, ROW_COUNT(); \
     SHOW COUNT(*) WARNINGS;" \
    | tail -n 2 \
    | tr '\n' '|')
expect_value \
    "filesystem charset variable reads and clears warning diagnostics" \
    "binary	1	0	-1|0|" \
    "$warning_values"

parse_error_values=$(printf '%s\n' \
    'BAD SQL; SELECT @@character_set_filesystem, @@warning_count, @@error_count, ROW_COUNT(); SHOW COUNT(*) WARNINGS;' \
    | docker exec -i "$MYSQL_CONTAINER" mysql $MYSQL_ARGS --skip-column-names --force 2>/dev/null \
    | tail -n 2 \
    | tr '\n' '|')
expect_value \
    "filesystem charset variable reads and clears error diagnostics" \
    "binary	1	1	-1|0|" \
    "$parse_error_values"

expect_error \
    "unknown unscoped filesystem charset variable" \
    1193 \
    HY000 \
    "Unknown system variable 'no_such_filesystem_charset_variable'" \
    "SELECT @@no_such_filesystem_charset_variable;"

expect_error \
    "unknown scoped filesystem charset variable" \
    1193 \
    HY000 \
    "Unknown system variable 'no_such_filesystem_charset_variable'" \
    "SELECT @@global.no_such_filesystem_charset_variable;"

expect_error \
    "unknown filesystem charset assignment" \
    1115 \
    42000 \
    "Unknown character set: 'nosuch'" \
    "SET SESSION character_set_filesystem=nosuch;"

expect_error \
    "string digit filesystem charset assignment is a charset name" \
    1115 \
    42000 \
    "Unknown character set: '33'" \
    "SET SESSION character_set_filesystem='33';"

expect_error \
    "unknown filesystem charset collation id" \
    1115 \
    42000 \
    "Unknown character set: '999'" \
    "SET SESSION character_set_filesystem=999;"

expect_error \
    "decimal filesystem charset assignment type" \
    1232 \
    42000 \
    "Incorrect argument type to variable 'character_set_filesystem'" \
    "SET SESSION character_set_filesystem=33.0;"

expect_error \
    "negative decimal filesystem charset assignment type" \
    1232 \
    42000 \
    "Incorrect argument type to variable 'character_set_filesystem'" \
    "SET SESSION character_set_filesystem=-33.0;"

expect_error \
    "decimal filesystem charset user variable type" \
    1232 \
    42000 \
    "Incorrect argument type to variable 'character_set_filesystem'" \
    "SET @filesystem_charset_decimal = 33.0; SET character_set_filesystem=@filesystem_charset_decimal;"

expect_error \
    "quoted filesystem charset variable scope is syntax error" \
    1064 \
    42000 \
    "syntax" \
    "SELECT @@\`session\`.character_set_filesystem;"

expect_output \
    "mysql accepts expressions outside this mylite slice" \
    "1" \
    "SELECT @@character_set_filesystem + 1;"

cleanup
