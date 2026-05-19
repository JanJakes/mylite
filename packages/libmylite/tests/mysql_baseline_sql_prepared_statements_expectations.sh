#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_sql_prepared_statements_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_sql_prepared_statements_expectations: $1" >&2
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
        *) fail "$label: expected error $code/$state containing [$message], got [$output]" ;;
    esac
}

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT HUP INT TERM

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

run_mysql "DROP DATABASE IF EXISTS ${DATABASE}; CREATE DATABASE ${DATABASE};" >/dev/null

expect_output \
    "basic lifecycle and row counts" \
    "2	two
-1	0	0
0	0	0" \
    "CREATE TABLE t (id INT, v VARCHAR(20));
     INSERT INTO t VALUES (1,'one'),(2,'two'),(3,NULL);
     PREPARE stmt FROM 'SELECT id, v FROM t WHERE id = ?';
     SET @id = 2;
     EXECUTE STMT USING @id;
     SELECT ROW_COUNT(), @@warning_count, @@error_count;
     DROP PREPARE stmt;
     SELECT ROW_COUNT(), @@warning_count, @@error_count;" \
    "$DATABASE"

expect_output \
    "source user variable and dml side effects" \
    "1	0	0
2	two-updated
1	one
2	two-updated
3	NULL" \
    "SET @sql = 'UPDATE t SET v = ? WHERE id = ?';
     PREPARE upd FROM @sql;
     SET @v = 'two-updated', @id = 2;
     EXECUTE upd USING @v, @id;
     SELECT ROW_COUNT(), @@warning_count, @@error_count;
     SELECT id, v FROM t WHERE id = 2;
     DEALLOCATE PREPARE upd;
     SELECT id, v FROM t ORDER BY id;" \
    "$DATABASE"

expect_output \
    "null and string parameters" \
    "NULL	a'b	1" \
    "PREPARE vals FROM 'SELECT ?, ?, ?';
     SET @missing_source = @never_assigned, @quoted = 'a''b', @n = 1;
     EXECUTE vals USING @missing_source, @quoted, @n;
     DEALLOCATE PREPARE vals;" \
    "$DATABASE"

expect_output \
    "backslash parameters" \
    "615C62
615C5C62" \
    "PREPARE slash FROM 'SELECT HEX(?)';
     SET @slash = 'a\\\\b';
     EXECUTE slash USING @slash;
     SET SESSION sql_mode = 'NO_BACKSLASH_ESCAPES';
     SET @slash = 'a\\\\b';
     EXECUTE slash USING @slash;
     DEALLOCATE PREPARE slash;" \
    "$DATABASE"

expect_output \
    "replacement failure removes old handler" \
    "1" \
    "PREPARE repl FROM 'SELECT 1';
     EXECUTE repl;" \
    "$DATABASE"

expect_error \
    "replacement prepare failure" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "PREPARE repl FROM 'SELECT FROM';" \
    "$DATABASE"

expect_error \
    "replacement old handler gone" \
    1243 \
    HY000 \
    "Unknown prepared statement handler" \
    "EXECUTE repl;" \
    "$DATABASE"

expect_error \
    "unknown execute" \
    1243 \
    HY000 \
    "Unknown prepared statement handler" \
    "EXECUTE missing_stmt;" \
    "$DATABASE"

expect_error \
    "unknown deallocate" \
    1243 \
    HY000 \
    "Unknown prepared statement handler" \
    "DEALLOCATE PREPARE missing_stmt;" \
    "$DATABASE"

expect_error \
    "missing using" \
    1210 \
    HY000 \
    "Incorrect arguments to EXECUTE" \
    "PREPARE arg_count FROM 'SELECT ?'; EXECUTE arg_count;" \
    "$DATABASE"

expect_error \
    "too many using variables" \
    1210 \
    HY000 \
    "Incorrect arguments to EXECUTE" \
    "PREPARE arg_count FROM 'SELECT ?'; SET @a = 1, @b = 2; EXECUTE arg_count USING @a, @b;" \
    "$DATABASE"

expect_error \
    "using constants syntax" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "PREPARE arg_count FROM 'SELECT ?'; EXECUTE arg_count USING 1;" \
    "$DATABASE"

expect_error \
    "direct parameter syntax" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SELECT ?;" \
    "$DATABASE"

expect_error \
    "null source syntax" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "PREPARE nullsrc FROM @not_set;" \
    "$DATABASE"

expect_error \
    "numeric source syntax" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SET @source_number = 123; PREPARE intsrc FROM @source_number;" \
    "$DATABASE"

expect_error \
    "marker as table name" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "PREPARE table_marker FROM 'SELECT * FROM ?';" \
    "$DATABASE"

expect_error \
    "marker as statement" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "PREPARE stmt_marker FROM '?';" \
    "$DATABASE"

expect_error \
    "nested prepare" \
    1295 \
    HY000 \
    "This command is not supported in the prepared statement protocol yet" \
    "PREPARE nested FROM 'PREPARE x FROM ''SELECT 1''';" \
    "$DATABASE"

expect_error \
    "nested execute" \
    1295 \
    HY000 \
    "This command is not supported in the prepared statement protocol yet" \
    "PREPARE nested_execute FROM 'EXECUTE x';" \
    "$DATABASE"

expect_error \
    "nested deallocate" \
    1295 \
    HY000 \
    "This command is not supported in the prepared statement protocol yet" \
    "PREPARE nested_deallocate FROM 'DEALLOCATE PREPARE x';" \
    "$DATABASE"

expect_error \
    "multiple source statements" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "PREPARE multi FROM 'SELECT 1; SELECT 2';" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_sql_prepared_statements_expectations: ok"
