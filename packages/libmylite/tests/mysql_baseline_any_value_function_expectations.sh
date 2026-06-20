#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_any_value_function_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_any_value_function_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | "${MYSQL_BIN:-mysql}" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                --default-character-set=utf8mb4 --batch --raw --skip-column-names "$@"
        return
    fi
    if [ -n "$MYSQL_BIN" ]; then
        printf '%s\n' "$sql" \
            | "$MYSQL_BIN" --protocol=TCP -h127.0.0.1 -uroot --default-character-set=utf8mb4 \
                --batch --raw --skip-column-names "$@"
        return
    fi
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --default-character-set=utf8mb4 \
            --batch --raw --skip-column-names "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | "${MYSQL_BIN:-mysql}" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                --default-character-set=utf8mb4 --batch --raw "$@"
        return
    fi
    if [ -n "$MYSQL_BIN" ]; then
        printf '%s\n' "$sql" \
            | "$MYSQL_BIN" --protocol=TCP -h127.0.0.1 -uroot --default-character-set=utf8mb4 \
                --batch --raw "$@"
        return
    fi
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --default-character-set=utf8mb4 \
            --batch --raw "$@"
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
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE};" >/dev/null

sql_mode=$(run_mysql "SELECT @@sql_mode;")
case "$sql_mode" in
    *ONLY_FULL_GROUP_BY*) ;;
    *) fail "expected ONLY_FULL_GROUP_BY in default sql_mode, got [$sql_mode]" ;;
esac

scalar_expected=$(cat <<EXPECTED
1	NULL	abc	0
1
-1	0
EXPECTED
)
expect_output \
    "scalar any_value values" \
    "$scalar_expected" \
    "USE ${DATABASE}; DO 0; "\
"SELECT ANY_VALUE(1), ANY_VALUE(NULL), ANY_VALUE('abc'), @@warning_count; "\
"SELECT ANY_VALUE (1); "\
"SELECT ROW_COUNT(), @@warning_count;"

label_expected=$(cat <<EXPECTED
ANY_VALUE(1)	alias_value
1	abc
EXPECTED
)
expect_output_with_headers \
    "scalar any_value labels" \
    "$label_expected" \
    "USE ${DATABASE}; SELECT ANY_VALUE(1), ANY_VALUE('abc') AS alias_value;"

run_mysql \
    "USE ${DATABASE}; "\
"CREATE TABLE t(g INT, v INT, s VARCHAR(20)); "\
"INSERT INTO t VALUES (1,10,'ten'),(1,10,'ten'),(2,NULL,NULL),(2,NULL,NULL),(3,20,'twenty');" \
    >/dev/null

row_scalar_expected=$(cat <<EXPECTED
10
10
NULL
NULL
20
EXPECTED
)
expect_output \
    "row scalar any_value descriptor column" \
    "$row_scalar_expected" \
    "USE ${DATABASE}; SELECT ANY_VALUE(v) FROM t ORDER BY g, v;"

empty_expected=""
expect_output \
    "row scalar any_value empty filter" \
    "$empty_expected" \
    "USE ${DATABASE}; SELECT ANY_VALUE(v) FROM t WHERE g = 99;"

grouped_expected=$(cat <<EXPECTED
1	10	ten	10	2
2	NULL	NULL	NULL	2
3	20	twenty	20	1
EXPECTED
)
expect_output \
    "grouped any_value representative columns" \
    "$grouped_expected" \
    "USE ${DATABASE}; "\
"SELECT g, ANY_VALUE(v), ANY_VALUE(s), MAX(v), COUNT(*) FROM t GROUP BY g ORDER BY g;"

having_expected=$(cat <<EXPECTED
1	10	10
3	20	20
EXPECTED
)
expect_output \
    "grouped any_value selected alias having" \
    "$having_expected" \
    "USE ${DATABASE}; "\
"SELECT g, ANY_VALUE(v) AS av, MAX(v) AS mx FROM t GROUP BY g HAVING av IS NOT NULL ORDER BY g;"

order_expected=$(cat <<EXPECTED
3	20
1	10
2	NULL
EXPECTED
)
expect_output \
    "grouped any_value selected alias order" \
    "$order_expected" \
    "USE ${DATABASE}; SELECT g, ANY_VALUE(v) AS av FROM t GROUP BY g ORDER BY av DESC;"

expect_output \
    "grouped any_value selected expression order" \
    "$order_expected" \
    "USE ${DATABASE}; SELECT g, ANY_VALUE(v) AS av FROM t GROUP BY g ORDER BY ANY_VALUE(v) DESC;"

expect_output \
    "any_value ordinary table identifier" \
    "" \
    "USE ${DATABASE}; CREATE TABLE any_value(id INT); DROP TABLE any_value;"

expect_error \
    "any_value zero arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'ANY_VALUE'" \
    "USE ${DATABASE}; SELECT ANY_VALUE();"

expect_error \
    "any_value multiple arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'ANY_VALUE'" \
    "USE ${DATABASE}; SELECT ANY_VALUE(1,2);"

expect_error \
    "any_value star syntax" \
    1064 \
    42000 \
    "near '* ) FROM t'" \
    "USE ${DATABASE}; SELECT ANY_VALUE(* ) FROM t;"

expect_error \
    "any_value distinct syntax" \
    1064 \
    42000 \
    "near 'DISTINCT v) FROM t'" \
    "USE ${DATABASE}; SELECT ANY_VALUE(DISTINCT v) FROM t;"

expect_error \
    "any_value unknown column" \
    1054 \
    "42S22" \
    "Unknown column 'missing' in 'field list'" \
    "USE ${DATABASE}; SELECT ANY_VALUE(missing) FROM t;"

printf '%s\n' "mysql_baseline_any_value_function_expectations: ok"
