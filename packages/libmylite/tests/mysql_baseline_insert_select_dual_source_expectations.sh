#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_insert_select_dual_$$"
OTHER_DATABASE="${DATABASE}_other"

fail() {
    printf '%s\n' "mysql_baseline_insert_select_dual_source_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names "$@"
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
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE}; DROP DATABASE IF EXISTS ${OTHER_DATABASE};" \
        >/dev/null 2>&1 || true
    rm -f "/tmp/${DATABASE}_basic.out"
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
     CREATE DATABASE ${OTHER_DATABASE};
     USE ${DATABASE};
     CREATE TABLE guard(id INT NOT NULL, label VARCHAR(64)) ENGINE=InnoDB;
     CREATE TABLE dst(id INT NOT NULL, label VARCHAR(128), must INT NOT NULL DEFAULT 7)
         ENGINE=InnoDB;
     INSERT INTO guard VALUES (1, 'open'), (2, 'closed');
     INSERT INTO dst(id, label) SELECT 1, 'dual' FROM DUAL;
     SELECT 'dual', ROW_COUNT(), @@warning_count, @@error_count;
     INSERT INTO dst(id, label) SELECT 2, 'nosource';
     SELECT 'nosource', ROW_COUNT(), @@warning_count, @@error_count;
     INSERT INTO dst(id, label)
         SELECT 3, CONCAT('exists-', DATABASE()) FROM DUAL
         WHERE EXISTS (SELECT 1 FROM guard WHERE id = 1);
     SELECT 'exists-true', ROW_COUNT(), @@warning_count, @@error_count;
     INSERT INTO dst(id, label) SELECT 4, 'not-inserted' FROM DUAL
         WHERE EXISTS (SELECT 1 FROM guard WHERE id = 99);
     SELECT 'exists-false', ROW_COUNT(), @@warning_count, @@error_count;
     INSERT INTO dst(id, label) SELECT 5, 'not-exists-true' FROM DUAL
         WHERE NOT EXISTS (SELECT 1 FROM guard WHERE id = 99);
     SELECT 'not-exists-true', ROW_COUNT(), @@warning_count, @@error_count;
     INSERT INTO dst(id, label) SELECT 6, 'not-exists-false' FROM DUAL
         WHERE NOT EXISTS (SELECT 1 FROM guard WHERE id = 1);
     SELECT 'not-exists-false', ROW_COUNT(), @@warning_count, @@error_count;
     SELECT id, label, must FROM dst ORDER BY id;" \
    >"/tmp/${DATABASE}_basic.out"

expect_value \
    "dual source status" \
    "dual	1	0	0" \
    "$(sed -n '1p' "/tmp/${DATABASE}_basic.out")"
expect_value \
    "no-source status" \
    "nosource	1	0	0" \
    "$(sed -n '2p' "/tmp/${DATABASE}_basic.out")"
expect_value \
    "exists true status" \
    "exists-true	1	0	0" \
    "$(sed -n '3p' "/tmp/${DATABASE}_basic.out")"
expect_value \
    "exists false status" \
    "exists-false	0	0	0" \
    "$(sed -n '4p' "/tmp/${DATABASE}_basic.out")"
expect_value \
    "not exists true status" \
    "not-exists-true	1	0	0" \
    "$(sed -n '5p' "/tmp/${DATABASE}_basic.out")"
expect_value \
    "not exists false status" \
    "not-exists-false	0	0	0" \
    "$(sed -n '6p' "/tmp/${DATABASE}_basic.out")"
expect_value \
    "inserted rows" \
    "1	dual	7
2	nosource	7
3	exists-${DATABASE}	7
5	not-exists-true	7" \
    "$(sed -n '7,$p' "/tmp/${DATABASE}_basic.out")"

zero_status=$(
    run_mysql \
        "USE ${DATABASE};
         CREATE TABLE required_target(id INT NOT NULL, must INT NOT NULL) ENGINE=InnoDB;
         INSERT INTO required_target(id)
             SELECT 10 FROM DUAL WHERE NOT EXISTS (SELECT 1 FROM guard WHERE id = 1);
         SELECT ROW_COUNT(), @@warning_count, @@error_count;
         SELECT COUNT(*) FROM required_target;"
)
expect_value \
    "zero-row source omits required column without error" \
    "0	0	0
0" \
    "$(printf '%s\n' "$zero_status" | tail -n 2)"

auto_increment_status=$(
    run_mysql \
        "USE ${DATABASE};
         CREATE TABLE keyed(
             id INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
             label VARCHAR(20) UNIQUE
         ) ENGINE=InnoDB;
         INSERT INTO keyed(label) SELECT 'a' FROM DUAL;
         SELECT ROW_COUNT(), LAST_INSERT_ID(), @@warning_count;
         INSERT INTO keyed(label) SELECT 'b' FROM DUAL
             WHERE NOT EXISTS (SELECT 1 FROM guard WHERE id = 99);
         SELECT ROW_COUNT(), LAST_INSERT_ID(), @@warning_count;
         INSERT INTO keyed(id, label) SELECT NULL, 'null-id' FROM DUAL;
         SELECT ROW_COUNT(), LAST_INSERT_ID(), @@warning_count;
         INSERT INTO keyed(id, label) SELECT 0, 'zero-id' FROM DUAL;
         SELECT ROW_COUNT(), LAST_INSERT_ID(), @@warning_count;
         SELECT id, label FROM keyed ORDER BY id;"
)
expect_value \
    "auto increment dual source" \
    "1	1	0
1	2	0
1	3	0
1	4	0
1	a
2	b
3	null-id
4	zero-id" \
    "$(printf '%s\n' "$auto_increment_status" | tail -n 8)"

qualified_status=$(
    run_mysql \
        "CREATE TABLE ${OTHER_DATABASE}.qualified_dst(id INT NOT NULL, label VARCHAR(20))
             ENGINE=InnoDB;
         INSERT INTO ${OTHER_DATABASE}.qualified_dst(id, label) SELECT 1, 'q' FROM DUAL;
         SELECT ROW_COUNT(), @@warning_count, @@error_count;
         SELECT id, label FROM ${OTHER_DATABASE}.qualified_dst;"
)
expect_value \
    "schema-qualified target without default database" \
    "1	0	0
1	q" \
    "$(printf '%s\n' "$qualified_status" | tail -n 2)"

expect_error \
    "omitted not-null no-default with produced row" \
    1364 \
    HY000 \
    "Field 'must' doesn't have a default value" \
    "USE ${DATABASE}; INSERT INTO required_target(id) SELECT 11 FROM DUAL;"

expect_error \
    "selected null into not null" \
    1048 \
    23000 \
    "Column 'must' cannot be null" \
    "USE ${DATABASE}; INSERT INTO required_target(id, must) SELECT 12, NULL FROM DUAL;"

expect_error \
    "column count mismatch" \
    1136 \
    21S01 \
    "Column count doesn't match value count at row 1" \
    "USE ${DATABASE}; INSERT INTO required_target(id, must) SELECT 13 FROM DUAL;"

expect_error \
    "wildcard from dual" \
    1096 \
    HY000 \
    "No tables used" \
    "USE ${DATABASE}; INSERT INTO required_target SELECT * FROM DUAL;"

expect_error \
    "duplicate unique key" \
    1062 \
    23000 \
    "Duplicate entry 'a'" \
    "USE ${DATABASE}; INSERT INTO keyed(label) SELECT 'a' FROM DUAL;"

expect_error \
    "default select item is syntax error" \
    1064 \
    42000 \
    "near ', 'default-id' FROM DUAL'" \
    "USE ${DATABASE}; INSERT INTO keyed(id, label) SELECT DEFAULT, 'default-id' FROM DUAL;"

expect_error \
    "target before source diagnostic order" \
    1049 \
    42000 \
    "Unknown database 'missing_target_schema'" \
    "INSERT INTO missing_target_schema.t SELECT 1 FROM missing_source_schema.t;"

expect_error \
    "unknown exists table" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing_guard' doesn't exist" \
    "USE ${DATABASE}; INSERT INTO dst(id, label) SELECT 20, 'x' FROM DUAL
         WHERE EXISTS (SELECT 1 FROM missing_guard);"

expect_error \
    "unknown exists column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'where clause'" \
    "USE ${DATABASE}; INSERT INTO dst(id, label) SELECT 21, 'x' FROM DUAL
         WHERE EXISTS (SELECT 1 FROM guard WHERE missing = 1);"

printf '%s\n' "mysql_baseline_insert_select_dual_source_expectations: ok"
