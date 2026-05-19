#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_joined_delete_expectations_$$"
OTHER_DATABASE="${DATABASE}_other"

fail() {
    printf '%s\n' "mysql_baseline_joined_delete_lifecycle_expectations: $1" >&2
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

reset_tables() {
    run_mysql \
        "DROP TABLE IF EXISTS t; DROP TABLE IF EXISTS u; "\
"CREATE TABLE t (id INT, k INT, v INT); "\
"CREATE TABLE u (id INT, k INT, v INT); "\
"INSERT INTO t VALUES (1,10,100),(2,20,200),(3,30,300),(4,NULL,400); "\
"INSERT INTO u VALUES (9,10,900),(10,10,901),(11,30,902),(12,NULL,903);" \
        "$DATABASE" >/dev/null
}

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
    run_mysql "DROP DATABASE IF EXISTS ${OTHER_DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE}; CREATE DATABASE ${OTHER_DATABASE};" >/dev/null

reset_tables
expect_output \
    "joined delete duplicate matches count target rows once" \
    "2	0	2,4" \
    "DELETE t FROM t JOIN u ON t.k = u.k; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(id ORDER BY id) FROM t;" \
    "$DATABASE"

reset_tables
expect_output \
    "using joined delete with where" \
    "2	0	2,4" \
    "DELETE FROM t USING t JOIN u ON t.k = u.k WHERE u.v > 900; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(id ORDER BY id) FROM t;" \
    "$DATABASE"

reset_tables
expect_output \
    "alias target joined delete" \
    "2	0	2,4" \
    "DELETE a FROM t AS a JOIN u AS b ON a.k = b.k; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(id ORDER BY id) FROM t;" \
    "$DATABASE"

reset_tables
expect_output \
    "using alias target joined delete" \
    "2	0	2,4" \
    "DELETE FROM a USING t AS a JOIN u AS b ON a.k = b.k; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(id ORDER BY id) FROM t;" \
    "$DATABASE"

reset_tables
expect_output \
    "left join unmatched target rows" \
    "2	0	1,3" \
    "DELETE t FROM t LEFT JOIN u ON t.k = u.k WHERE u.id IS NULL; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(id ORDER BY id) FROM t;" \
    "$DATABASE"

reset_tables
expect_output \
    "inner join without on uses where filter" \
    "4	0	NULL" \
    "DELETE t FROM t JOIN u WHERE u.id = 9; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(id ORDER BY id) FROM t;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE ${DATABASE}.qualified_t (id INT, k INT); "\
"CREATE TABLE ${DATABASE}.qualified_u (id INT, k INT); "\
"INSERT INTO ${DATABASE}.qualified_t VALUES (1,10),(2,20); "\
"INSERT INTO ${DATABASE}.qualified_u VALUES (9,10);" >/dev/null
expect_output \
    "schema-qualified target without selected schema" \
    "1	0	2" \
    "DELETE ${DATABASE}.qualified_t FROM ${DATABASE}.qualified_t "\
"JOIN ${DATABASE}.qualified_u ON qualified_t.k = qualified_u.k; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(id ORDER BY id) "\
"FROM ${DATABASE}.qualified_t;"

expect_error \
    "unqualified target without selected schema" \
    1046 \
    3D000 \
    "No database selected" \
    "DELETE qualified_t FROM ${DATABASE}.qualified_t "\
"JOIN ${DATABASE}.qualified_u ON qualified_t.k = qualified_u.k;"

expect_error \
    "alias target without selected schema" \
    1046 \
    3D000 \
    "No database selected" \
    "DELETE a FROM ${DATABASE}.qualified_t AS a "\
"JOIN ${DATABASE}.qualified_u AS b ON a.k = b.k;"

expect_error \
    "information schema explicit target is access denied" \
    1044 \
    42000 \
    "Access denied for user" \
    "DELETE information_schema.SCHEMATA FROM ${DATABASE}.qualified_t "\
"JOIN ${DATABASE}.qualified_u ON qualified_t.k = qualified_u.k;"

reset_tables
expect_error \
    "information schema selected schema target is access denied" \
    1044 \
    42000 \
    "Access denied for user" \
    "USE information_schema; "\
"DELETE a FROM ${DATABASE}.t AS a JOIN ${DATABASE}.u AS b ON a.k = b.k;"

reset_tables
expect_error \
    "target table name invalid when alias declared" \
    1109 \
    42S02 \
    "Unknown table 't' in MULTI DELETE" \
    "DELETE t FROM t AS a JOIN u AS b ON a.k = b.k;" \
    "$DATABASE"

reset_tables
expect_error \
    "target missing from joined sources" \
    1109 \
    42S02 \
    "Unknown table 'x' in MULTI DELETE" \
    "DELETE x FROM t JOIN u ON t.k = u.k;" \
    "$DATABASE"

reset_tables
expect_error \
    "unknown joined source table" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing' doesn't exist" \
    "DELETE t FROM t JOIN missing ON t.k = missing.k;" \
    "$DATABASE"

reset_tables
expect_error \
    "unknown on column" \
    1054 \
    42S22 \
    "Unknown column 't.nope' in 'on clause'" \
    "DELETE t FROM t JOIN u ON t.nope = u.k;" \
    "$DATABASE"

reset_tables
expect_error \
    "ambiguous where column" \
    1052 \
    23000 \
    "Column 'id' in where clause is ambiguous" \
    "DELETE t FROM t JOIN u ON t.k = u.k WHERE id = 1;" \
    "$DATABASE"

reset_tables
expect_error \
    "joined delete order by is syntax error" \
    1064 \
    42000 \
    "near 'ORDER BY t.id'" \
    "DELETE t FROM t JOIN u ON t.k = u.k ORDER BY t.id;" \
    "$DATABASE"

reset_tables
expect_error \
    "joined delete limit is syntax error" \
    1064 \
    42000 \
    "near 'LIMIT 1'" \
    "DELETE t FROM t JOIN u ON t.k = u.k LIMIT 1;" \
    "$DATABASE"
