#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"

fail() {
    printf '%s\n' "mysql_bounded_syntax_error_diagnostics_expectations: $1" >&2
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

emit_diagnostic_probe() {
    printf '%s\n' \
        "GET DIAGNOSTICS CONDITION 1 @mylite_state = RETURNED_SQLSTATE, @mylite_errno = MYSQL_ERRNO, @mylite_message = MESSAGE_TEXT;" \
        "SELECT @mylite_state, @mylite_errno, @mylite_message;" \
        "SELECT 9;"
}

verify_probe_output() {
    label=$1
    output=$2
    expected_message=$3
    expected=$(printf '42000\t1064\t%s' "$expected_message")
    diagnostic=$(printf '%s\n' "$output" | sed -n '1p')
    reuse=$(printf '%s\n' "$output" | sed -n '2p')

    if [ "$diagnostic" != "$expected" ]; then
        fail "$label: expected [$expected], got [$diagnostic]"
    fi
    if [ "$reuse" != "9" ]; then
        fail "$label: connection did not remain usable, got [$reuse]"
    fi
}

expect_parse_error() {
    label=$1
    sql=$2
    expected_message=$3
    output=$(
        {
            printf '%s\n' "$sql"
            emit_diagnostic_probe
        } | run_mysql_stream --force 2>/dev/null
    )

    verify_probe_output "$label" "$output" "$expected_message"
}

generate_large_error() {
    byte_count=$1
    awk -v count="$byte_count" 'BEGIN {
        printf "SELECT 1 x "
        for (item = 0; item < count; ++item) {
            printf "a"
        }
        print ";"
    }'
}

expect_generated_parse_error() {
    label=$1
    byte_count=$2
    expected_message=$3
    output=$(
        {
            generate_large_error "$byte_count"
            emit_diagnostic_probe
        } | run_mysql_stream --force 2>/dev/null
    )

    verify_probe_output "$label" "$output" "$expected_message"
}

version_and_locale=$(run_mysql "SELECT VERSION(), @@lc_messages;")
case "$version_and_locale" in
    8.4.9*"	en_US") ;;
    *) fail "expected MySQL 8.4.9 with en_US messages, got [$version_and_locale]" ;;
esac

message_prefix="You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near '"
near_79=$(awk 'BEGIN { for (item = 0; item < 79; ++item) printf "a" }')
near_80="${near_79}a"
token_81="${near_80}a"

expect_parse_error \
    "source remainder" \
    "SELECT FROM t;" \
    "${message_prefix}FROM t' at line 1"
expect_parse_error \
    "delimiter means end of input" \
    "SELECT;" \
    "${message_prefix}' at line 1"
expect_parse_error \
    "trailing whitespace" \
    "SELECT FROM t   ;" \
    "${message_prefix}FROM t' at line 1"
expect_parse_error \
    "79-byte excerpt" \
    "SELECT 1 x ${near_79};" \
    "${message_prefix}${near_79}' at line 1"
expect_parse_error \
    "80-byte excerpt" \
    "SELECT 1 x ${near_80};" \
    "${message_prefix}${near_80}' at line 1"
expect_parse_error \
    "81-byte excerpt" \
    "SELECT 1 x ${token_81};" \
    "${message_prefix}${near_80}' at line 1"
expect_parse_error \
    "line feed" \
    "$(printf 'SELECT 1 +\nFROM t;')" \
    "${message_prefix}FROM t' at line 2"
expect_parse_error \
    "CRLF" \
    "$(printf 'SELECT 1 +\r\nFROM t;')" \
    "${message_prefix}FROM t' at line 2"
expect_parse_error \
    "standalone carriage return" \
    "$(printf 'SELECT 1 +\rFROM t;')" \
    "${message_prefix}FROM t' at line 1"
expect_parse_error \
    "SQL-level prepare" \
    "PREPARE mylite_syntax_probe FROM 'SELECT FROM t';" \
    "${message_prefix}FROM t' at line 1"
expect_generated_parse_error \
    "one-MiB excerpt" \
    1048576 \
    "${message_prefix}${near_80}' at line 1"

printf '%s\n' "mysql_bounded_syntax_error_diagnostics_expectations: ok"
