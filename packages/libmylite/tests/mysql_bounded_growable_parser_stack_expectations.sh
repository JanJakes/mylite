#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"

fail() {
    printf '%s\n' "mysql_bounded_growable_parser_stack_expectations: $1" >&2
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

generate_nested_select() {
    shape=$1
    depth=$2
    awk -v shape="$shape" -v depth="$depth" 'BEGIN {
        printf "SELECT "
        if (shape == "parentheses") {
            for (item = 0; item < depth; ++item) {
                printf "("
            }
        } else {
            for (item = 0; item < depth; ++item) {
                printf "IF(1,1,"
            }
        }
        printf "0"
        for (item = 0; item < depth; ++item) {
            printf ")"
        }
        print ";"
    }'
}

expect_generated_success() {
    label=$1
    shape=$2
    depth=$3
    expected_value=$4
    output=$(
        {
            generate_nested_select "$shape" "$depth"
            printf '%s\n' "SELECT 9;"
        } | run_mysql_stream
    )
    expected=$(printf '%s\n9' "$expected_value")

    if [ "$output" != "$expected" ]; then
        fail "$label: expected [$expected], got [$output]"
    fi
}

expect_generated_error() {
    label=$1
    shape=$2
    depth=$3
    expected_state=$4
    expected_errno=$5
    expected_message_prefix=$6
    output=$(
        {
            generate_nested_select "$shape" "$depth"
            printf '%s\n' \
                "GET DIAGNOSTICS CONDITION 1 @mylite_state = RETURNED_SQLSTATE, @mylite_errno = MYSQL_ERRNO, @mylite_message = MESSAGE_TEXT;" \
                "SELECT @mylite_state, @mylite_errno, @mylite_message;" \
                "SELECT 9;"
        } | run_mysql_stream --force 2>/dev/null
    )
    diagnostic=$(printf '%s\n' "$output" | sed -n '1p')
    reuse=$(printf '%s\n' "$output" | sed -n '2p')
    expected_prefix=$(printf '%s\t%s\t%s' "$expected_state" "$expected_errno" "$expected_message_prefix")

    case "$diagnostic" in
        "$expected_prefix"*) ;;
        *) fail "$label: expected prefix [$expected_prefix], got [$diagnostic]" ;;
    esac
    if [ "$reuse" != "9" ]; then
        fail "$label: connection did not remain usable, got [$reuse]"
    fi
}

version_and_limits=$(
    run_mysql "SELECT VERSION(), @@lc_messages, @@thread_stack, @@parser_max_mem_size;"
)
if [ "$version_and_limits" != "8.4.9	en_US	1048576	18446744073709551615" ]; then
    fail "unexpected MySQL runtime or limits: [$version_and_limits]"
fi

expect_generated_success "direct 16384 parentheses" "parentheses" 16384 0
expect_generated_success "direct 1732 IF calls" "if" 1732 1

prepared_output=$(
    run_mysql "
SET @mylite_q = CONCAT('SELECT ', REPEAT('(', 16384), '0', REPEAT(')', 16384));
PREPARE mylite_nesting_stmt FROM @mylite_q;
EXECUTE mylite_nesting_stmt;
DEALLOCATE PREPARE mylite_nesting_stmt;
SET @mylite_q = CONCAT('SELECT ', REPEAT('IF(1,1,', 1024), '0', REPEAT(')', 1024));
PREPARE mylite_nesting_stmt FROM @mylite_q;
EXECUTE mylite_nesting_stmt;
DEALLOCATE PREPARE mylite_nesting_stmt;
SELECT 9;"
)
expected_prepared=$(printf '0\n1\n9')
if [ "$prepared_output" != "$expected_prepared" ]; then
    fail "prepared nesting: expected [$expected_prepared], got [$prepared_output]"
fi

expect_generated_error \
    "32768-parenthesis resource boundary" \
    "parentheses" \
    32768 \
    "HY000" \
    3950 \
    "Out of memory"
expect_generated_error \
    "1733-IF thread-stack boundary" \
    "if" \
    1733 \
    "HY000" \
    1436 \
    "Thread stack overrun:"

printf '%s\n' "mysql_bounded_growable_parser_stack_expectations: ok"
