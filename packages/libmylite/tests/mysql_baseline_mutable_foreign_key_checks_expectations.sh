#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_mutable_foreign_key_checks_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_mutable_foreign_key_checks_expectations: $1" >&2
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

expected_values=$(cat <<EOF
initial	1	1	1	1	0	0	0
set0	0	1	0	0	0	0	0
session1	1	1	0	0	0
atsession0	0	1	0	0	0
atlocal1	1	1	0	0	0
off	0	0	0	0
on	1	0	0	0
true	1	0	0	0
false	0	0	0	0
plus1	1	0	0	0
plus0	0	0	0	0
default	0	1	0	0	0
EOF
)
expect_output \
    "mutable session foreign_key_checks values" \
    "$expected_values" \
    "USE ${DATABASE};
     SELECT 'initial', @@foreign_key_checks, @@global.foreign_key_checks,
            @@session.foreign_key_checks, @@local.foreign_key_checks,
            @@warning_count, @@error_count, ROW_COUNT();
     SET foreign_key_checks = 0;
     SELECT 'set0', @@foreign_key_checks, @@global.foreign_key_checks,
            @@session.foreign_key_checks, @@local.foreign_key_checks,
            @@warning_count, @@error_count, ROW_COUNT();
     SET SESSION foreign_key_checks = 1;
     SELECT 'session1', @@foreign_key_checks, @@global.foreign_key_checks,
            @@warning_count, @@error_count, ROW_COUNT();
     SET @@SESSION.foreign_key_checks = 0;
     SELECT 'atsession0', @@foreign_key_checks, @@global.foreign_key_checks,
            @@warning_count, @@error_count, ROW_COUNT();
     SET @@LOCAL.foreign_key_checks = 1;
     SELECT 'atlocal1', @@foreign_key_checks, @@global.foreign_key_checks,
            @@warning_count, @@error_count, ROW_COUNT();
     SET foreign_key_checks = OFF;
     SELECT 'off', @@foreign_key_checks, @@warning_count, @@error_count, ROW_COUNT();
     SET foreign_key_checks = ON;
     SELECT 'on', @@foreign_key_checks, @@warning_count, @@error_count, ROW_COUNT();
     SET foreign_key_checks = TRUE;
     SELECT 'true', @@foreign_key_checks, @@warning_count, @@error_count, ROW_COUNT();
     SET foreign_key_checks = FALSE;
     SELECT 'false', @@foreign_key_checks, @@warning_count, @@error_count, ROW_COUNT();
     SET foreign_key_checks = +1;
     SELECT 'plus1', @@foreign_key_checks, @@warning_count, @@error_count, ROW_COUNT();
     SET foreign_key_checks = +0;
     SELECT 'plus0', @@foreign_key_checks, @@warning_count, @@error_count, ROW_COUNT();
     SET foreign_key_checks = DEFAULT;
     SELECT 'default', @@foreign_key_checks, @@global.foreign_key_checks,
            @@warning_count, @@error_count, ROW_COUNT();"

expect_error \
    "foreign_key_checks rejects negative values" \
    1231 \
    42000 \
    "can't be set to the value of '-1'" \
    "USE ${DATABASE}; SET foreign_key_checks = -1;"
expect_error \
    "foreign_key_checks rejects values above one" \
    1231 \
    42000 \
    "can't be set to the value of '2'" \
    "USE ${DATABASE}; SET foreign_key_checks = 2;"
expect_error \
    "foreign_key_checks rejects strings" \
    1231 \
    42000 \
    "can't be set to the value of '0'" \
    "USE ${DATABASE}; SET foreign_key_checks = '0';"
expect_error \
    "foreign_key_checks rejects null" \
    1231 \
    42000 \
    "can't be set to the value of 'NULL'" \
    "USE ${DATABASE}; SET foreign_key_checks = NULL;"

expected_dml=$(cat <<EOF
orphan	3	1	0	0	3
after_delete_off	2	3	1	1	0	0
after_update_off	1	1	0	1	0	0
after_insert_ignore_off	1	0	0	4	1
EOF
)
expect_output \
    "disabled foreign_key_checks skips DML checks and actions" \
    "$expected_dml" \
    "USE ${DATABASE};
     DROP TABLE IF EXISTS c_dml;
     DROP TABLE IF EXISTS p_dml;
     SET foreign_key_checks = 0;
     CREATE TABLE p_dml (id INT PRIMARY KEY) ENGINE=InnoDB;
     CREATE TABLE c_dml (
       id INT PRIMARY KEY,
       pid INT,
       CONSTRAINT fk_c_dml_p FOREIGN KEY (pid) REFERENCES p_dml(id)
         ON DELETE CASCADE ON UPDATE CASCADE
     ) ENGINE=InnoDB;
     INSERT INTO p_dml VALUES (1), (2), (3);
     INSERT INTO c_dml VALUES (10,1), (20,2), (30,99);
     SELECT 'orphan', COUNT(*), SUM(pid = 99), @@warning_count, @@error_count, ROW_COUNT()
       FROM c_dml;
     DELETE FROM p_dml WHERE id = 1;
     SELECT 'after_delete_off', (SELECT COUNT(*) FROM p_dml), (SELECT COUNT(*) FROM c_dml),
            (SELECT COUNT(*) FROM c_dml WHERE pid = 1), ROW_COUNT(), @@warning_count,
            @@error_count;
     UPDATE p_dml SET id = 22 WHERE id = 2;
     SELECT 'after_update_off', (SELECT COUNT(*) FROM p_dml WHERE id = 22),
            (SELECT COUNT(*) FROM c_dml WHERE pid = 2),
            (SELECT COUNT(*) FROM c_dml WHERE pid = 22), ROW_COUNT(), @@warning_count,
            @@error_count;
     INSERT IGNORE INTO c_dml VALUES (40,100);
     SELECT 'after_insert_ignore_off', ROW_COUNT(), @@warning_count, @@error_count,
            COUNT(*), SUM(pid = 100) FROM c_dml;"

expect_error \
    "reenabled foreign_key_checks rejects new orphan rows" \
    1452 \
    23000 \
    "Cannot add or update a child row" \
    "USE ${DATABASE}; SET foreign_key_checks = 1; INSERT INTO c_dml VALUES (50,200);"

expect_error \
    "disabled foreign_key_checks still rejects required index drop" \
    1553 \
    HY000 \
    "needed in a foreign key constraint" \
    "USE ${DATABASE};
     DROP TABLE IF EXISTS c_drop;
     DROP TABLE IF EXISTS p_drop;
     SET foreign_key_checks = 1;
     CREATE TABLE p_drop (id INT PRIMARY KEY) ENGINE=InnoDB;
     CREATE TABLE c_drop (
       id INT PRIMARY KEY,
       pid INT,
       CONSTRAINT fk_c_drop_p FOREIGN KEY (pid) REFERENCES p_drop(id)
     ) ENGINE=InnoDB;
     SET foreign_key_checks = 0;
     DROP INDEX fk_c_drop_p ON c_drop;"

cleanup
trap - EXIT HUP INT TERM
