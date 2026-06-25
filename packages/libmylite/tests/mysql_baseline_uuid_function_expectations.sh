#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_uuid_function_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_uuid_function_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw "$@"
}

run_mysql_type_info() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --column-type-info -vvv "$@"
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

expect_uuid_shape_row() {
    label=$1
    sql=$2
    shift 2

    output=$(run_mysql "$sql" "$@")
    printf '%s\n' "$output" | awk -F '\t' '
        NF != 10 { exit 1 }
        $1 !~ /^[0-9a-f]{8}-[0-9a-f]{4}-1[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/ {
            exit 1
        }
        $2 != "36" || $3 != "36" || $4 != "1" || $5 != "1" || $6 != "utf8mb3" {
            exit 1
        }
        $7 != "utf8mb3_general_ci" || $8 != "4" || $9 != "1" || $10 != "0" {
            exit 1
        }
    ' || fail "$label: unexpected UUID status row [$output]"
}

expect_table_backed_uuids() {
    label=$1
    sql=$2
    shift 2

    output=$(run_mysql "$sql" "$@")
    printf '%s\n' "$output" | awk -F '\t' '
        NF != 2 { exit 1 }
        $1 !~ /^[123]$/ { exit 1 }
        $2 !~ /^[0-9a-f]{8}-[0-9a-f]{4}-1[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/ {
            exit 1
        }
        seen[$2]++ { duplicate = 1 }
        END {
            if (NR != 3 || duplicate) {
                exit 1
            }
        }
    ' || fail "$label: expected three unique table-backed UUID rows, got [$output]"
}

expect_unsigned_integer() {
    label=$1
    value=$2

    case "$value" in
        ''|*[!0-9]*) fail "$label: expected unsigned integer, got [$value]" ;;
        0) fail "$label: expected nonzero unsigned integer, got [$value]" ;;
        *) ;;
    esac
}

expect_uuid_short_row() {
    label=$1
    sql=$2
    shift 2

    output=$(run_mysql "$sql" "$@")
    set -- $output
    if [ "$#" -ne 9 ]; then
        fail "$label: expected 9 columns, got [$output]"
    fi
    expect_unsigned_integer "$label value 1" "$1"
    expect_unsigned_integer "$label value 2" "$2"
    expect_unsigned_integer "$label value 3" "$3"
    expect_unsigned_integer "$label value 4" "$4"
    if [ "$(($2 - $1))" -ne 1 ] || [ "$(($3 - $2))" -ne 1 ] ||
        [ "$(($4 - $3))" -ne 1 ]; then
        fail "$label: expected first UUID_SHORT values to be consecutive, got [$1 $2 $3 $4]"
    fi
    if [ "$5" != "binary" ] || [ "$6" != "binary" ] || [ "$7" != "5" ] ||
        [ "$8" != "0" ] || [ "$9" != "0" ]; then
        fail "$label: unexpected UUID_SHORT status row [$output]"
    fi
}

expect_table_backed_uuid_shorts() {
    label=$1
    sql=$2
    shift 2

    output=$(run_mysql "$sql" "$@")
    printf '%s\n' "$output" | awk -F '\t' '
        NF != 2 { exit 1 }
        $1 !~ /^[123]$/ { exit 1 }
        $2 !~ /^[1-9][0-9]*$/ { exit 1 }
        seen[$2]++ { duplicate = 1 }
        END {
            if (NR != 3 || duplicate) {
                exit 1
            }
        }
    ' || fail "$label: expected three unique table-backed UUID_SHORT rows, got [$output]"
}

expect_header() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql_with_headers "$sql" "$@")
    header=$(printf '%s\n' "$output" | sed -n '1p')
    if [ "$header" != "$expected" ]; then
        fail "$label: expected header [$expected], got [$header]"
    fi
}

expect_contains() {
    label=$1
    needle=$2
    haystack=$3

    case "$haystack" in
        *"$needle"*) ;;
        *) fail "$label: expected output to contain [$needle]" ;;
    esac
}

expect_not_contains() {
    label=$1
    needle=$2
    haystack=$3

    case "$haystack" in
        *"$needle"*) fail "$label: expected output not to contain [$needle]" ;;
        *) ;;
    esac
}

expect_accepts() {
    label=$1
    sql=$2
    shift 2

    set +e
    output=$(run_mysql "$sql" "$@" 2>&1)
    status_code=$?
    set -e

    if [ "$status_code" -ne 0 ]; then
        fail "$label: expected MySQL to accept behavior, got [$output]"
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

cleanup
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE};" >/dev/null

expect_uuid_shape_row \
    "UUID shape and metadata functions" \
    "DO 0; SELECT UUID(), LENGTH(UUID()), CHAR_LENGTH(UUID()), IS_UUID(UUID()), "\
"SUBSTRING(UUID(), 15, 1) = '1', CHARSET(UUID()), COLLATION(UUID()), "\
"COERCIBILITY(UUID()), UUID() <> UUID(), @@warning_count;" \
    "$DATABASE"

expect_output \
    "UUID DO status" \
    "0	0" \
    "DO UUID(); SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_output \
    "UUID composition" \
    "1	1	0" \
    "DO 0; SELECT LEFT(CONCAT('id-', UUID()), 3) = 'id-', "\
"IS_UUID(SUBSTRING(CONCAT('id-', UUID()), 4)), @@warning_count;" \
    "$DATABASE"

expect_accepts \
    "UUID identifier table" \
    "CREATE TABLE uuid (uuid INT); INSERT INTO uuid VALUES (1); SELECT uuid FROM uuid;" \
    "$DATABASE"

expect_error \
    "bare UUID identifier" \
    1054 \
    "42S22" \
    "Unknown column 'UUID' in 'field list'" \
    "SELECT UUID;" \
    "$DATABASE"

expect_header \
    "spaced UUID header" \
    "UUID ()	u" \
    "SELECT UUID (), UUID() AS u FROM DUAL;" \
    "$DATABASE"

expect_table_backed_uuids \
    "table-backed UUID rows" \
    "DROP TABLE IF EXISTS t; CREATE TABLE t(id INT); INSERT INTO t VALUES (1), (2), (3); "\
"SELECT id, UUID() FROM t ORDER BY id;" \
    "$DATABASE"

expect_uuid_short_row \
"UUID_SHORT shape and metadata functions" \
    "DO 0; SELECT UUID_SHORT(), uuid_short(), Uuid_Short(), UUID_SHORT (), "\
"CHARSET(UUID_SHORT()), COLLATION(UUID_SHORT()), "\
"COERCIBILITY(UUID_SHORT()), @@warning_count, ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "UUID_SHORT DO status" \
    "0	0" \
    "DO UUID_SHORT(); SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_header \
    "spaced UUID_SHORT header" \
    "UUID_SHORT ()	u" \
    "SELECT UUID_SHORT (), UUID_SHORT() AS u FROM DUAL;" \
    "$DATABASE"

expect_table_backed_uuid_shorts \
    "table-backed UUID_SHORT rows" \
    "SELECT id, UUID_SHORT() FROM t ORDER BY id;" \
    "$DATABASE"

expect_accepts \
    "UUID_SHORT identifier table" \
    "CREATE TABLE uuid_short (uuid_short INT); INSERT INTO uuid_short VALUES (1); "\
"SELECT uuid_short FROM uuid_short;" \
    "$DATABASE"

expect_error \
    "bare UUID_SHORT identifier" \
    1054 \
    "42S22" \
    "Unknown column 'UUID_SHORT' in 'field list'" \
    "SELECT UUID_SHORT;" \
    "$DATABASE"

type_output=$(run_mysql_type_info \
    "USE ${DATABASE}; SELECT UUID() AS u FROM DUAL; SELECT id, UUID() AS u FROM t ORDER BY id;" \
    "$DATABASE")

expect_contains "UUID metadata label" 'Field   1:  `u`' "$type_output"
expect_contains "UUID metadata type" 'Type:       VAR_STRING' "$type_output"
expect_not_contains "UUID metadata nullable flag" 'Flags:      NOT_NULL' "$type_output"

uuid_short_type_output=$(run_mysql_type_info \
    "USE ${DATABASE}; SELECT UUID_SHORT() AS u FROM DUAL; SELECT id, UUID_SHORT() AS u FROM t ORDER BY id;" \
    "$DATABASE")

expect_contains "UUID_SHORT metadata label" 'Field   1:  `u`' "$uuid_short_type_output"
expect_contains "UUID_SHORT metadata type" 'Type:       LONGLONG' "$uuid_short_type_output"
expect_contains "UUID_SHORT metadata collation" 'Collation:  binary (63)' "$uuid_short_type_output"
expect_contains "UUID_SHORT metadata length" 'Length:     21' "$uuid_short_type_output"
expect_contains \
    "UUID_SHORT metadata flags" \
    'Flags:      NOT_NULL UNSIGNED BINARY NUM' \
    "$uuid_short_type_output"

expect_error \
    "UUID one argument" \
    1582 \
    "42000" \
    "Incorrect parameter count in the call to native function 'UUID'" \
    "SELECT UUID(NULL);" \
    "$DATABASE"

expect_error \
    "UUID two arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count in the call to native function 'UUID'" \
    "SELECT UUID(1, 2);" \
    "$DATABASE"

expect_error \
    "UUID_SHORT one argument" \
    1582 \
    "42000" \
    "Incorrect parameter count in the call to native function 'UUID_SHORT'" \
    "SELECT UUID_SHORT(NULL);" \
    "$DATABASE"

expect_error \
    "UUID_SHORT two arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count in the call to native function 'UUID_SHORT'" \
    "SELECT UUID_SHORT(1, 2);" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_uuid_function_expectations: ok"
