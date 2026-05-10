#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_ARGS="--protocol=TCP -h127.0.0.1 -uroot --batch --raw --default-character-set=utf8mb4"

fail() {
    printf '%s\n' "mysql_baseline_set_connection_character_set_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql $MYSQL_ARGS --skip-column-names "$@"
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
        *) fail "$label: expected error $code/$state containing [$message], got [$output]" ;;
    esac
}

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

supported_values=$(run_mysql "
SET NAMES utf8mb4;
SELECT @@character_set_client, @@character_set_connection, @@character_set_results,
       @@collation_connection, @@warning_count, @@error_count, ROW_COUNT();
SET NAMES DEFAULT;
SELECT @@character_set_client, @@character_set_connection, @@character_set_results,
       @@collation_connection, @@warning_count, @@error_count, ROW_COUNT();
SET NAMES utf8mb4 COLLATE utf8mb4_0900_ai_ci;
SELECT @@character_set_client, @@character_set_connection, @@character_set_results,
       @@collation_connection, @@warning_count, @@error_count, ROW_COUNT();
SET CHARACTER SET utf8mb4;
SELECT @@character_set_client, @@character_set_connection, @@character_set_results,
       @@collation_connection, @@warning_count, @@error_count, ROW_COUNT();
SET CHARSET DEFAULT;
SELECT @@character_set_client, @@character_set_connection, @@character_set_results,
       @@collation_connection, @@warning_count, @@error_count, ROW_COUNT();
SET NAMES 'utf8mb4' COLLATE 'utf8mb4_0900_ai_ci';
SELECT @@character_set_client, @@character_set_connection, @@character_set_results,
       @@collation_connection, @@warning_count, @@error_count, ROW_COUNT();
SET NAMES \`utf8mb4\`;
SELECT @@character_set_client, @@character_set_connection, @@character_set_results,
       @@collation_connection, @@warning_count, @@error_count, ROW_COUNT();
SET CHARACTER SET \`utf8mb4\`;
SELECT @@character_set_client, @@character_set_connection, @@character_set_results,
       @@collation_connection, @@warning_count, @@error_count, ROW_COUNT();
SET NAMES UTF8MB4 COLLATE UTF8MB4_0900_AI_CI;
SELECT @@character_set_client, @@character_set_connection, @@character_set_results,
       @@collation_connection, @@warning_count, @@error_count, ROW_COUNT();
")

expected_line="utf8mb4	utf8mb4	utf8mb4	utf8mb4_0900_ai_ci	0	0	0"
line_count=$(printf '%s\n' "$supported_values" | wc -l | tr -d ' ')
expect_value "supported set charset result count" "9" "$line_count"
while IFS= read -r line; do
    expect_value "supported set charset values" "$expected_line" "$line"
done <<EOF
$supported_values
EOF

mutable_collation=$(run_mysql "
SET NAMES utf8mb4 COLLATE utf8mb4_bin;
SELECT @@character_set_client, @@character_set_connection, @@character_set_results,
       @@collation_connection, @@warning_count, @@error_count, ROW_COUNT();
")
expect_value \
    "mysql wider set names collation changes connection collation" \
    "utf8mb4	utf8mb4	utf8mb4	utf8mb4_bin	0	0	0" \
    "$mutable_collation"

latin1_values=$(run_mysql "
SET NAMES latin1;
SELECT @@character_set_client, @@character_set_connection, @@character_set_results,
       @@warning_count, @@error_count, ROW_COUNT();
")
expect_value \
    "mysql wider set names latin1 changes connection charset" \
    "latin1	latin1	latin1	0	0	0" \
    "$latin1_values"

expect_error \
    "set names missing value rejected" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SET NAMES;"

expect_error \
    "set names null rejected" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SET NAMES NULL;"

expect_error \
    "set names comma rejected" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SET NAMES utf8mb4, latin1;"

expect_error \
    "set character set collate rejected" \
    1064 \
    42000 \
    "COLLATE utf8mb4_0900_ai_ci" \
    "SET CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci;"

expect_error \
    "unknown set names character set rejected" \
    1115 \
    42000 \
    "Unknown character set: 'bogus'" \
    "SET NAMES bogus;"

expect_error \
    "unknown set character set rejected" \
    1115 \
    42000 \
    "Unknown character set: 'bogus'" \
    "SET CHARACTER SET bogus;"

expect_error \
    "unknown set names collation rejected" \
    1273 \
    HY000 \
    "Unknown collation: 'bogus'" \
    "SET NAMES utf8mb4 COLLATE bogus;"

expect_error \
    "invalid set names collation rejected" \
    1253 \
    42000 \
    "COLLATION 'latin1_swedish_ci' is not valid for CHARACTER SET 'utf8mb4'" \
    "SET NAMES utf8mb4 COLLATE latin1_swedish_ci;"

expect_error \
    "impermissible client character set rejected" \
    1231 \
    42000 \
    "Variable 'character_set_client' can't be set to the value of 'ucs2'" \
    "SET NAMES ucs2;"
