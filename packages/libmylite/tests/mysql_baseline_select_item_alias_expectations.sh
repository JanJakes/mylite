#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_select_item_alias_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_select_item_alias_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --batch --raw --skip-column-names "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --batch --raw "$@"
}

repeat_char() {
    char=$1
    count=$2
    awk -v char="$char" -v count="$count" 'BEGIN { for (i = 0; i < count; ++i) printf "%s", char }'
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
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null
run_mysql \
    "CREATE TABLE numbers (id INT NOT NULL, n INT NULL, nn INT NOT NULL); "\
"INSERT INTO numbers VALUES (1, 10, 5), (2, NULL, 6), (3, 20, 7);" \
    "$DATABASE" >/dev/null

alias65=$(repeat_char a 65)
alias256=$(repeat_char a 256)
alias257=$(repeat_char a 257)

expect_output_with_headers \
    "identifier aliases" \
    "x	y
10	5
NULL	6
20	7" \
    "SELECT n AS x, nn y FROM numbers ORDER BY id;" \
    "$DATABASE"

expect_output_with_headers \
    "quoted identifier alias" \
    "Customer identity
10" \
    "SELECT n AS \`Customer identity\` FROM numbers ORDER BY id LIMIT 1;" \
    "$DATABASE"

expect_output_with_headers \
    "string literal alias with AS" \
    "Customer identity
20" \
    "SELECT n AS 'Customer identity' FROM numbers "\
"ORDER BY \`Customer identity\` DESC LIMIT 1;" \
    "$DATABASE"

expect_output_with_headers \
    "bare string literal alias" \
    "Customer identity
10" \
    "SELECT n 'Customer identity' FROM numbers ORDER BY id LIMIT 1;" \
    "$DATABASE"

expect_output_with_headers \
    "65-character identifier alias" \
    "${alias65}
20" \
    "SELECT n AS ${alias65} FROM numbers ORDER BY ${alias65} DESC LIMIT 1;" \
    "$DATABASE"

expect_output_with_headers \
    "256-character quoted identifier alias" \
    "${alias256}
20" \
    "SELECT n AS \`${alias256}\` FROM numbers ORDER BY \`${alias256}\` DESC LIMIT 1;" \
    "$DATABASE"

expect_output_with_headers \
    "256-character string literal alias" \
    "${alias256}
20" \
    "SELECT n AS '${alias256}' FROM numbers ORDER BY \`${alias256}\` DESC LIMIT 1;" \
    "$DATABASE"

expect_output_with_headers \
    "257-character alias truncation" \
    "${alias256}
1" \
    "SELECT 1 AS ${alias257};" \
    "$DATABASE"

expect_output_with_headers \
    "distinct alias" \
    "x
NULL
10
20" \
    "SELECT DISTINCT n AS x FROM numbers ORDER BY x;" \
    "$DATABASE"

expect_output_with_headers \
    "distinctrow alias" \
    "x
NULL
10
20" \
    "SELECT DISTINCTROW n x FROM numbers ORDER BY x;" \
    "$DATABASE"

expect_output_with_headers \
    "count star alias" \
    "c
3" \
    "SELECT COUNT(*) AS c FROM numbers;" \
    "$DATABASE"

expect_output_with_headers \
    "count column alias" \
    "cn
2" \
    "SELECT COUNT(n) cn FROM numbers;" \
    "$DATABASE"

expect_output_with_headers \
    "count distinct alias" \
    "cd
2" \
    "SELECT COUNT(DISTINCT n) AS cd FROM numbers;" \
    "$DATABASE"

expect_output_with_headers \
    "min alias" \
    "mn
10" \
    "SELECT MIN(n) AS mn FROM numbers;" \
    "$DATABASE"

expect_output_with_headers \
    "max alias" \
    "mx
20" \
    "SELECT MAX(n) mx FROM numbers;" \
    "$DATABASE"

expect_output_with_headers \
    "scalar aliases" \
    "d	u	cu	wc
${DATABASE}	root@localhost	root@localhost	0" \
    "DO 0; SELECT DATABASE() AS d, USER() u, CURRENT_USER AS cu, @@warning_count AS wc;" \
    "$DATABASE"

expect_output \
    "order by alias descending" \
    "20
10
NULL
0	-1" \
    "DO 0; SELECT n AS x FROM numbers ORDER BY x DESC; "\
"SELECT @@warning_count, ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "order by alias case insensitive" \
    "20
10
NULL" \
    "SELECT n AS X FROM numbers ORDER BY x DESC;" \
    "$DATABASE"

expect_output \
    "alias shadows descriptor column in order" \
    "NULL
10
20" \
    "SELECT n AS id FROM numbers ORDER BY id;" \
    "$DATABASE"

expect_output \
    "where resolves descriptor column not alias" \
    "NULL" \
    "SELECT n AS id FROM numbers WHERE id = 2;" \
    "$DATABASE"

expect_error \
    "where does not resolve alias" \
    1054 \
    42S22 \
    "Unknown column 'x' in 'where clause'" \
    "SELECT n AS x FROM numbers WHERE x = 10;" \
    "$DATABASE"

expect_error \
    "duplicate aliases ambiguous in order" \
    1052 \
    23000 \
    "Column 'x' in order clause is ambiguous" \
    "SELECT id AS x, n AS x FROM numbers ORDER BY x;" \
    "$DATABASE"

expect_output \
    "string order key is constant not alias" \
    "10
NULL
20" \
    "SELECT n AS 'x' FROM numbers ORDER BY 'x' DESC;" \
    "$DATABASE"

expect_error \
    "star alias syntax error" \
    1064 \
    42000 \
    "near 'AS x FROM numbers'" \
    "SELECT * AS x FROM numbers;" \
    "$DATABASE"

printf '%s\n' "baseline-select-item-alias MySQL 8.4.9 expectations verified"
