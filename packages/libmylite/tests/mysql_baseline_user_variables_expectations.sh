#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_user_variables_expectations_$$"
DEFAULT_SQL_MODE="ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION"

fail() {
    printf '%s\n' "mysql_baseline_user_variables_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    sql="SET SESSION sql_mode = '${DEFAULT_SQL_MODE}';
${sql}"
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

expect_output_ignore_stderr() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql "$sql" "$@" 2>/dev/null)
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

repeat_text() {
    text=$1
    count=$2
    output=

    while [ "$count" -gt 0 ]; do
        output="${output}${text}"
        count=$((count - 1))
    done

    printf '%s' "$output"
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

e_acute=$(printf '\303\251')
e_acute_64=$(repeat_text "$e_acute" 64)
e_acute_65=$(repeat_text "$e_acute" 65)

expect_output \
    "uninitialized variables" \
    "NULL	NULL	NULL	NULL	NULL	0	0	0" \
    "SET @reset_diagnostics = NULL;
     SELECT @missing_a, @Missing_A, @, @'', @\`\`, @@warning_count, @@error_count, ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "basic assignments and labels" \
    "1	x	NULL	0	0	0
1	x	NULL" \
    "SET @a = 1, @b := 'x', @c = NULL;
     SELECT @a, @b, @c, @@warning_count, @@error_count, ROW_COUNT();
     SELECT @a, @b AS bee, @nope;" \
    "$DATABASE"

expect_output \
    "case-insensitive variables" \
    "7	7	7" \
    "SET @Foo = 7; SELECT @foo, @FOO, @Foo;" \
    "$DATABASE"

expect_output \
    "decimal assignments" \
    "1.0	-1.50" \
    "SET @d = 1.0, @nd = -1.50; SELECT @d, @nd;" \
    "$DATABASE"

expect_output \
    "quoted variables" \
    "ok	space	dq" \
    "SET @\`dash-name\` = 'ok', @'sp ace' = 'space', @\"dq-name\" = 'dq';
     SELECT @\`dash-name\`, @'sp ace', @\"dq-name\";" \
    "$DATABASE"

expect_output \
    "system variable save restore" \
    "${DEFAULT_SQL_MODE}	NO_ENGINE_SUBSTITUTION	0
${DEFAULT_SQL_MODE}	0" \
    "SET @old_sql_mode = @@sql_mode, sql_mode = 'NO_ENGINE_SUBSTITUTION';
     SELECT @old_sql_mode, @@sql_mode, ROW_COUNT();
     SET sql_mode = @old_sql_mode;
     SELECT @@sql_mode, ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "fixed boolean save restore" \
    "1	1	1
1	0	1
1	1	0" \
    "SET @old_notes = @@sql_notes, @old_unique = @@unique_checks, @old_fk = @@foreign_key_checks;
     SELECT @old_notes, @old_unique, @old_fk;
     SET sql_notes = @old_notes, unique_checks = @old_unique, foreign_key_checks = 0;
     SELECT @@sql_notes, @@foreign_key_checks, @@unique_checks;
     SET foreign_key_checks = @old_fk;
     SELECT @@sql_notes, @@foreign_key_checks, ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "time zone save restore" \
    "SYSTEM	+02:30
SYSTEM	0" \
    "SET @old_time_zone = @@time_zone, time_zone = '+02:30';
     SELECT @old_time_zone, @@time_zone;
     SET time_zone = @old_time_zone;
     SELECT @@time_zone, ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "do and scalar expression operands" \
    "3	0	0" \
    "SET @n = 2; DO @n, @n + 1; SELECT @n + 1, @@warning_count, @@error_count;" \
    "$DATABASE"

expect_output \
    "assignment expressions" \
    "1	1	3	3
2
NULL	1	1
1
7	7	1
5	0	1" \
    "SELECT @a := 1, @a, @a := @a + 2, @a;
     SELECT @@warning_count;
     SELECT @b = 1, @b := 1, @b;
     SELECT @@warning_count;
     SELECT @sub := (SELECT 7), @sub, @@warning_count;
     DO @done := 5;
     SELECT @done, ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_output \
    "name length boundary" \
    "1" \
    "SET @aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa = 1;
     SELECT @aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa;" \
    "$DATABASE"

expect_error \
    "name too long" \
    3061 \
    42000 \
    "User variable name 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa' is illegal" \
    "SET @aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa = 1;" \
    "$DATABASE"

expect_output \
    "UTF-8 name length boundary" \
    "1" \
    "SET @'${e_acute_64}' = 1;
     SELECT @'${e_acute_64}';" \
    "$DATABASE"

expect_error \
    "UTF-8 name too long" \
    3061 \
    42000 \
    "User variable name" \
    "SET @'${e_acute_65}' = 1;" \
    "$DATABASE"

expect_error \
    "default user variable assignment syntax" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SET @d = DEFAULT;" \
    "$DATABASE"

expect_error \
    "empty bare user variable assignment" \
    3061 \
    42000 \
    "User variable name '' is illegal" \
    "SET @ = 1;" \
    "$DATABASE"

expect_error \
    "empty quoted user variable assignment" \
    3061 \
    42000 \
    "User variable name '' is illegal" \
    "SET @'' = 1;" \
    "$DATABASE"

expect_error \
    "empty user variable assignment expression" \
    3061 \
    42000 \
    "User variable name '' is illegal" \
    "SELECT @ := 1;" \
    "$DATABASE"

expect_output_ignore_stderr \
    "assignment list failure is atomic" \
    "before
before	1	1	-1" \
    "SET @atomic = 'before';
     SELECT @atomic;
     SET @atomic = 'after', no_such_system_var = 1;
     SELECT @atomic, @@warning_count, @@error_count, ROW_COUNT();" \
    --force \
    "$DATABASE"
