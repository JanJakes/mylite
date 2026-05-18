#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
TAB=$(printf '\t')

fail() {
    printf '%s\n' "mysql_baseline_show_status_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | "$MYSQL_BIN" --socket="$MYSQL_SOCKET" -uroot \
                --batch --raw --skip-column-names \
                --default-character-set=utf8mb4 "$@"
        return
    fi

    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql \
            --protocol=TCP \
            -h127.0.0.1 \
            -uroot \
            --batch \
            --raw \
            --skip-column-names \
            --default-character-set=utf8mb4 \
            "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | "$MYSQL_BIN" --socket="$MYSQL_SOCKET" -uroot \
                --batch --raw \
                --default-character-set=utf8mb4 "$@"
        return
    fi

    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql \
            --protocol=TCP \
            -h127.0.0.1 \
            -uroot \
            --batch \
            --raw \
            --default-character-set=utf8mb4 \
            "$@"
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

expect_value() {
    label=$1
    expected=$2
    actual=$3

    if [ "$actual" != "$expected" ]; then
        fail "$label: expected [$expected], got [$actual]"
    fi
}

normalize_tsv() {
    sed "s/${TAB}/|/g"
}

expect_positive_numeric_status_row() {
    label=$1
    variable_name=$2
    actual=$3

    case "$actual" in
        "$variable_name"\|*[!0-9]* | "$variable_name"\|) ;;
        "$variable_name"\|0) fail "$label: expected positive value, got [$actual]" ;;
        "$variable_name"\|*) return 0 ;;
        *) fail "$label: row shape mismatch: [$actual]" ;;
    esac

    fail "$label: expected numeric value, got [$actual]"
}

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

headers=$(run_mysql_with_headers "SHOW STATUS LIKE 'Threads_connected';" | sed -n '1p')
expect_value "headers" "Variable_name${TAB}Value" "$headers"

threads_connected=$(run_mysql "SHOW STATUS LIKE 'Threads_connected';" | normalize_tsv)
expect_positive_numeric_status_row "default threads_connected" "Threads_connected" "$threads_connected"

session_threads_connected=$(run_mysql "SHOW SESSION STATUS LIKE 'Threads_connected';" | normalize_tsv)
expect_positive_numeric_status_row \
    "session threads_connected" \
    "Threads_connected" \
    "$session_threads_connected"

local_threads_connected=$(run_mysql "SHOW LOCAL STATUS LIKE 'Threads_connected';" | normalize_tsv)
expect_positive_numeric_status_row "local threads_connected" "Threads_connected" "$local_threads_connected"

global_threads_connected=$(run_mysql "SHOW GLOBAL STATUS LIKE 'Threads_connected';" | normalize_tsv)
expect_positive_numeric_status_row "global threads_connected" "Threads_connected" "$global_threads_connected"

compression=$(run_mysql "SHOW STATUS LIKE 'Compression';" | normalize_tsv)
expect_value "default compression" "Compression|OFF" "$compression"

session_compression=$(run_mysql "SHOW SESSION STATUS LIKE 'Compression';" | normalize_tsv)
expect_value "session compression" "Compression|OFF" "$session_compression"

local_compression=$(run_mysql "SHOW LOCAL STATUS LIKE 'Compression';" | normalize_tsv)
expect_value "local compression" "Compression|OFF" "$local_compression"

global_compression=$(run_mysql "SHOW GLOBAL STATUS LIKE 'Compression';" | normalize_tsv)
expect_value "global omits compression" "" "$global_compression"

thread_row_names=$(run_mysql "SHOW STATUS LIKE 'Threads\\_%';" | cut -f1)
expect_value "threads like row names" "Threads_cached
Threads_connected
Threads_created
Threads_running" "$thread_row_names"

thread_row_names_upper=$(run_mysql "SHOW STATUS LIKE 'THREADS\\_%';" | cut -f1)
expect_value "threads uppercase like row names" "Threads_cached
Threads_connected
Threads_created
Threads_running" "$thread_row_names_upper"

connections=$(run_mysql "SHOW STATUS LIKE 'Connections';" | normalize_tsv)
expect_positive_numeric_status_row "connections" "Connections" "$connections"

row_count_state=$(run_mysql \
    "SHOW STATUS LIKE 'Threads_connected'; SELECT ROW_COUNT(), @@warning_count, @@error_count;" \
    | tail -n 1 \
    | normalize_tsv)
expect_value "row count state" "-1|0|0" "$row_count_state"

show_where=$(run_mysql "SHOW STATUS WHERE Variable_name = 'Threads_connected';" | normalize_tsv)
expect_positive_numeric_status_row "mysql supports status where" "Threads_connected" "$show_where"

expect_error \
    "like then where is syntax error" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SHOW STATUS LIKE 'Threads%' WHERE Variable_name = 'Threads_connected';"

expect_error \
    "order by is syntax error" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SHOW STATUS ORDER BY Variable_name;"

expect_error \
    "limit is syntax error" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SHOW STATUS LIMIT 1;"

expect_error \
    "full is syntax error" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SHOW FULL STATUS;"

expect_error \
    "non-string like is syntax error" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SHOW STATUS LIKE 1;"
