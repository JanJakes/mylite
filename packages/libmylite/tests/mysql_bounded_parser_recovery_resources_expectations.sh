#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"

fail() {
    printf '%s\n' "mysql_bounded_parser_recovery_resources_expectations: $1" >&2
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

run_mysql_stream() {
    if [ -n "$MYSQL_SOCKET" ]; then
        "$MYSQL_BIN" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
            --batch --raw --skip-column-names "$@"
    else
        docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names "$@"
    fi
}

generate_flat_malformed() {
    token_count=$1
    awk -v count="$token_count" 'BEGIN {
        printf "SELECT"
        for (item = 0; item < count; ++item) {
            printf " 1"
        }
        print " +;"
    }'
}

generate_padded_malformed() {
    total_bytes=$1
    awk -v count="$((total_bytes - 16))" 'BEGIN {
        printf "SELECT 1 /*"
        for (item = 0; item < count; ++item) {
            printf "a"
        }
        print "*/ +;"
    }'
}

expect_generated_syntax_error() {
    label=$1
    generator=$2
    scale=$3

    output=$(
        {
            $generator "$scale"
            printf '%s\n' "SELECT 1;"
        } | run_mysql_stream --force 2>&1
    )
    case "$output" in
        *"ERROR 1064 (42000)"*"You have an error in your SQL syntax"*) ;;
        *) fail "$label: expected 1064/42000 syntax error, got [$output]" ;;
    esac
    if [ "$(printf '%s\n' "$output" | tail -n 1)" != "1" ]; then
        fail "$label: connection did not remain usable"
    fi
}

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9, got [$version]" ;;
esac

if [ "$(run_mysql "SELECT @@GLOBAL.max_allowed_packet;")" != "67108864" ]; then
    fail "expected default 64 MiB max_allowed_packet"
fi

expect_generated_syntax_error "64 flat tokens" generate_flat_malformed 64
expect_generated_syntax_error "65536 flat tokens" generate_flat_malformed 65536
expect_generated_syntax_error "one MiB comment padding" generate_padded_malformed 1048576

printf '%s\n' "mysql_bounded_parser_recovery_resources_expectations: ok"
