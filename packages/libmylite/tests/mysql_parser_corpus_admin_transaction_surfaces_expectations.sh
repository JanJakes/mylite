#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_parser_admin_tx_$$"
SERVER_NAME="mylite_parser_server_$$"

fail() {
    printf '%s\n' "mysql_parser_corpus_admin_transaction_surfaces_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_BIN" ]; then
        if [ -n "$MYSQL_SOCKET" ]; then
            printf '%s\n' "$sql" \
                | "$MYSQL_BIN" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                    --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
        else
            printf '%s\n' "$sql" \
                | "$MYSQL_BIN" --protocol=TCP -h127.0.0.1 -uroot \
                    --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
        fi
    else
        printf '%s\n' "$sql" \
            | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
                --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
    fi
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

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE}; DROP SERVER IF EXISTS ${SERVER_NAME};" \
        >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

expect_output \
    "commit and chain starts next transaction" \
    "1" \
    "CREATE TABLE tx(id INT); "\
"START TRANSACTION; INSERT INTO tx VALUES (1); COMMIT AND CHAIN; "\
"INSERT INTO tx VALUES (2); ROLLBACK; "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM tx;" \
    "$DATABASE"

expect_output \
    "rollback and chain starts next transaction" \
    "1,4" \
    "START TRANSACTION; INSERT INTO tx VALUES (3); ROLLBACK AND CHAIN; "\
"INSERT INTO tx VALUES (4); COMMIT; "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM tx;" \
    "$DATABASE"

expect_output \
    "chain without active transaction starts next transaction" \
    "1,4" \
    "COMMIT AND CHAIN; INSERT INTO tx VALUES (5); ROLLBACK; "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM tx;" \
    "$DATABASE"

run_mysql "START TRANSACTION; COMMIT WORK RELEASE;" >/dev/null || \
    fail "commit work release was rejected"
expect_output \
    "completion no release accepted" \
    "1" \
    "START TRANSACTION; ROLLBACK NO RELEASE; SELECT 1;"
expect_output \
    "completion no chain accepted" \
    "2" \
    "START TRANSACTION; COMMIT AND NO CHAIN NO RELEASE; SELECT 2;"
run_mysql "START TRANSACTION; ROLLBACK WORK AND NO CHAIN RELEASE;" >/dev/null || \
    fail "rollback work and no chain release was rejected"

expect_output \
    "rename tables alias" \
    "2" \
    "CREATE TABLE r1(id INT); CREATE TABLE r2(id INT); "\
"RENAME TABLES r1 TO r1a, r2 TO r2a; "\
"SELECT COUNT(*) FROM information_schema.tables "\
"WHERE table_schema = '${DATABASE}' AND table_name IN ('r1a','r2a');" \
    "$DATABASE"

expect_output \
    "foreign server ddl updates mysql.servers" \
    "${SERVER_NAME}:Remote:test:3306
sally
0" \
    "DROP SERVER IF EXISTS ${SERVER_NAME}; "\
"CREATE SERVER ${SERVER_NAME} FOREIGN DATA WRAPPER mysql "\
"OPTIONS (USER 'Remote', HOST '127.0.0.1', DATABASE 'test', PORT 3306); "\
"SELECT CONCAT(Server_name, ':', Username, ':', Db, ':', Port) "\
"FROM mysql.servers WHERE Server_name = '${SERVER_NAME}'; "\
"ALTER SERVER ${SERVER_NAME} OPTIONS (USER 'sally'); "\
"SELECT Username FROM mysql.servers WHERE Server_name = '${SERVER_NAME}'; "\
"DROP SERVER ${SERVER_NAME}; "\
"SELECT COUNT(*) FROM mysql.servers WHERE Server_name = '${SERVER_NAME}';"

expect_output \
    "alter schema encryption and read-only accepted" \
    "${DATABASE}" \
    "ALTER SCHEMA ${DATABASE} ENCRYPTION = 'N'; "\
"ALTER SCHEMA ${DATABASE} READ ONLY DEFAULT; "\
"SELECT SCHEMA_NAME FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME = '${DATABASE}';"

cleanup

printf '%s\n' "mysql_parser_corpus_admin_transaction_surfaces_expectations: ok"
