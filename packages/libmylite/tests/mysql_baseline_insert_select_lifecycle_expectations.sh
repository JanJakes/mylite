#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_insert_select_$$"
OTHER_DATABASE="${DATABASE}_other"

fail() {
    printf '%s\n' "mysql_baseline_insert_select_lifecycle_expectations: $1" >&2
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
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE}; DROP DATABASE IF EXISTS ${OTHER_DATABASE};" \
        >/dev/null 2>&1 || true
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
     CREATE TABLE src(
         id INT NOT NULL,
         n INT NULL,
         b BIGINT,
         iu INT UNSIGNED,
         inv INT INVISIBLE
     ) ENGINE=InnoDB;
     CREATE TABLE dst(
         id INT NOT NULL,
         n INT NULL,
         b BIGINT,
         iu INT UNSIGNED,
         inv INT INVISIBLE
     ) ENGINE=InnoDB;
     INSERT INTO src(id, n, b, iu, inv)
         VALUES (1, 10, 100, 1000, 7), (2, NULL, 200, 2000, 8), (3, 30, 300, 3000, 9);
     INSERT INTO dst(id, n, b, iu)
         SELECT id, n, b, iu FROM src WHERE id >= 2 ORDER BY id DESC LIMIT 1;
     SELECT ROW_COUNT(), @@warning_count, @@error_count;
     SELECT id, n, b, iu, inv FROM dst ORDER BY id;
     INSERT dst(id, n, b, iu) SELECT id, n, b, iu FROM src WHERE id = 1;
     SELECT ROW_COUNT(), @@warning_count, @@error_count;
     SELECT id, n, b, iu, inv FROM dst ORDER BY id;" \
    >"/tmp/${DATABASE}_basic.out"

expect_value \
    "basic insert-select status" \
    "1	0	0" \
    "$(sed -n '1p' "/tmp/${DATABASE}_basic.out")"
expect_value \
    "basic inserted row" \
    "3	30	300	3000	NULL" \
    "$(sed -n '2p' "/tmp/${DATABASE}_basic.out")"
expect_value \
    "optional into status" \
    "1	0	0" \
    "$(sed -n '3p' "/tmp/${DATABASE}_basic.out")"
expect_value \
    "optional into rows" \
    "1	10	100	1000	NULL
3	30	300	3000	NULL" \
    "$(sed -n '4,$p' "/tmp/${DATABASE}_basic.out")"

zero_status=$(
    run_mysql \
        "USE ${DATABASE};
         CREATE TABLE required_target(id INT NOT NULL, must INT NOT NULL);
         INSERT INTO required_target(id) SELECT id FROM src WHERE id = 999;
         SELECT ROW_COUNT(), @@warning_count, @@error_count;
         SELECT COUNT(*) FROM required_target;"
)
expect_value \
    "zero-row source omits required column without error" \
    "0	0	0
0" \
    "$(printf '%s\n' "$zero_status" | tail -n 2)"

defaults_status=$(
    run_mysql \
        "USE ${DATABASE};
         CREATE TABLE default_target(
             id INT NOT NULL,
             n INT NULL DEFAULT NULL,
             nn INT NOT NULL DEFAULT 7,
             req INT NOT NULL,
             inv INT DEFAULT 9 INVISIBLE
         );
         INSERT INTO default_target(id, n, req) SELECT id, n, b FROM src WHERE id = 1;
         SELECT ROW_COUNT(), @@warning_count, @@error_count;
         SELECT id, n, nn, req, inv FROM default_target;"
)
expect_value \
    "omitted defaults and invisible target defaults" \
    "1	0	0
1	10	7	100	9" \
    "$(printf '%s\n' "$defaults_status" | tail -n 2)"

invisible_status=$(
    run_mysql \
        "USE ${DATABASE};
         CREATE TABLE invisible_target(
             id INT NOT NULL,
             n INT NULL,
             b BIGINT,
             iu INT UNSIGNED,
             inv INT INVISIBLE
         );
         INSERT INTO invisible_target SELECT * FROM src WHERE id = 2;
         INSERT INTO invisible_target(id, n, b, iu, inv)
             SELECT id, n, b, iu, inv FROM src WHERE id = 3;
         SELECT ROW_COUNT(), @@warning_count, @@error_count;
         SELECT id, n, b, iu, inv FROM invisible_target ORDER BY id;"
)
expect_value \
    "invisible source and target columns" \
    "1	0	0
2	NULL	200	2000	NULL
3	30	300	3000	9" \
    "$(printf '%s\n' "$invisible_status" | tail -n 3)"

qualified_status=$(
    run_mysql \
        "CREATE TABLE ${OTHER_DATABASE}.qualified_dst(id INT NOT NULL, n INT NULL);
         INSERT INTO ${OTHER_DATABASE}.qualified_dst(id, n)
             SELECT id, n FROM ${DATABASE}.src WHERE id = 1;
         SELECT ROW_COUNT(), @@warning_count, @@error_count;
         SELECT id, n FROM ${OTHER_DATABASE}.qualified_dst;"
)
expect_value \
    "qualified target and source without default database" \
    "1	0	0
1	10" \
    "$(printf '%s\n' "$qualified_status" | tail -n 2)"

expect_error \
    "column count mismatch too few" \
    1136 \
    21S01 \
    "Column count doesn't match value count at row 1" \
    "USE ${DATABASE}; INSERT INTO dst(id, n) SELECT id FROM src;"

expect_error \
    "column count mismatch too many with empty source" \
    1136 \
    21S01 \
    "Column count doesn't match value count at row 1" \
    "USE ${DATABASE}; INSERT INTO dst(id) SELECT id, n FROM src WHERE id = 999;"

expect_error \
    "omitted not-null no-default with matching source" \
    1364 \
    HY000 \
    "Field 'must' doesn't have a default value" \
    "USE ${DATABASE}; INSERT INTO required_target(id) SELECT id FROM src WHERE id = 1;"

expect_error \
    "selected null into not null" \
    1048 \
    23000 \
    "Column 'must' cannot be null" \
    "USE ${DATABASE}; INSERT INTO required_target(id, must) SELECT id, n FROM src WHERE id = 2;"

expect_error \
    "selected integer out of range" \
    1264 \
    22003 \
    "Out of range value for column 'tiny' at row 2" \
    "USE ${DATABASE};
     CREATE TABLE tiny_target(tiny TINYINT);
     CREATE TABLE range_source(id INT NOT NULL, value INT);
     INSERT INTO range_source VALUES (1, 1), (2, 128);
     INSERT INTO tiny_target(tiny) SELECT value FROM range_source ORDER BY id;"

expect_error \
    "duplicate target column" \
    1110 \
    42000 \
    "Column 'id' specified twice" \
    "USE ${DATABASE}; INSERT INTO dst(id, id) SELECT id, n FROM src;"

expect_error \
    "unknown target column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'field list'" \
    "USE ${DATABASE}; INSERT INTO dst(missing) SELECT id FROM src;"

expect_error \
    "unknown source column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'field list'" \
    "USE ${DATABASE}; INSERT INTO dst(id) SELECT missing FROM src;"

expect_error \
    "missing default schema for target" \
    1046 \
    3D000 \
    "No database selected" \
    "INSERT INTO dst(id) SELECT id FROM ${DATABASE}.src;"

expect_error \
    "missing default schema for source" \
    1046 \
    3D000 \
    "No database selected" \
    "INSERT INTO ${DATABASE}.dst(id) SELECT id FROM src;"

expect_error \
    "unknown target schema before source schema" \
    1049 \
    42000 \
    "Unknown database 'nosuch_target_${DATABASE}'" \
    "INSERT INTO nosuch_target_${DATABASE}.dst(id)
     SELECT id FROM nosuch_source_${DATABASE}.src;"

expect_error \
    "unknown target table before source table" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing_target' doesn't exist" \
    "INSERT INTO ${DATABASE}.missing_target(id) SELECT id FROM ${DATABASE}.missing_source;"

rm -f "/tmp/${DATABASE}_basic.out"
