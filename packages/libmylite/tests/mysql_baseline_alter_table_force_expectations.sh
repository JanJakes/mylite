#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_alter_table_force_$$"

fail() {
    printf '%s\n' "mysql_baseline_alter_table_force_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw --skip-column-names "$@"
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

run_mysql \
    "CREATE DATABASE ${DATABASE};
     USE ${DATABASE};
     CREATE TABLE forced_table(id INT, v INT) ENGINE=InnoDB;
     INSERT INTO forced_table VALUES (3, 30), (1, 10), (2, 20);
     ALTER TABLE forced_table FORCE;
     SELECT ROW_COUNT(), @@warning_count, @@error_count;
     SELECT CONCAT(id, ':', v) FROM forced_table ORDER BY id;" \
    >"/tmp/${DATABASE}_forced_table.out"

expect_value \
    "force status" \
    "0	0	0" \
    "$(sed -n '1p' "/tmp/${DATABASE}_forced_table.out")"
expect_value \
    "force rows" \
    "1:10
2:20
3:30" \
    "$(sed -n '2,$p' "/tmp/${DATABASE}_forced_table.out")"

empty_status=$(
    run_mysql \
        "USE ${DATABASE};
         CREATE TABLE empty_table(id INT) ENGINE=InnoDB;
         ALTER TABLE empty_table FORCE;
         SELECT ROW_COUNT(), @@warning_count, @@error_count;"
)
expect_value "empty table status" "0	0	0" "$(printf '%s\n' "$empty_status" | tail -n 1)"

qualified_status=$(
    run_mysql \
        "CREATE TABLE ${DATABASE}.qualified_target(id INT) ENGINE=InnoDB;
         INSERT INTO ${DATABASE}.qualified_target VALUES (1), (2);
         ALTER TABLE ${DATABASE}.qualified_target FORCE;
         SELECT ROW_COUNT(), @@warning_count, @@error_count;"
)
expect_value \
    "schema-qualified target status" \
    "0	0	0" \
    "$(printf '%s\n' "$qualified_status" | tail -n 1)"

repeated_status=$(
    run_mysql \
        "USE ${DATABASE};
         CREATE TABLE repeated_force(id INT) ENGINE=InnoDB;
         ALTER TABLE repeated_force FORCE, FORCE;
         SELECT ROW_COUNT(), @@warning_count, @@error_count;"
)
expect_value "repeated force status" "0	0	0" "$(printf '%s\n' "$repeated_status" | tail -n 1)"

mixed_status=$(
    run_mysql \
        "USE ${DATABASE};
         CREATE TABLE mixed_force(id INT) ENGINE=InnoDB;
         ALTER TABLE mixed_force FORCE, ALGORITHM=COPY;
         SELECT ROW_COUNT(), @@warning_count, @@error_count;"
)
expect_value "mixed force algorithm status" "0	0	0" "$(printf '%s\n' "$mixed_status" | tail -n 1)"

expect_error \
    "missing default database" \
    1046 \
    3D000 \
    "No database selected" \
    "ALTER TABLE forced_table FORCE;"

expect_error \
    "unknown explicit schema" \
    1049 \
    42000 \
    "Unknown database 'nosuch_schema_${DATABASE}'" \
    "ALTER TABLE nosuch_schema_${DATABASE}.forced_table FORCE;"

expect_error \
    "unknown table" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing' doesn't exist" \
    "USE ${DATABASE}; ALTER TABLE missing FORCE;"

expect_error \
    "force order without comma" \
    1064 \
    42000 \
    "near 'ORDER BY id'" \
    "USE ${DATABASE}; ALTER TABLE forced_table FORCE ORDER BY id;"

rm -f "/tmp/${DATABASE}_forced_table.out"
