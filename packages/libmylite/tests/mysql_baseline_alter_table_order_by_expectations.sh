#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_alter_table_order_by_$$"

fail() {
    printf '%s\n' "mysql_baseline_alter_table_order_by_expectations: $1" >&2
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
     CREATE TABLE ordered_default(id INT, v INT) ENGINE=InnoDB;
     INSERT INTO ordered_default VALUES (3, 30), (1, 10), (2, 20);
     ALTER TABLE ordered_default ORDER BY id;
     SELECT ROW_COUNT(), @@warning_count, @@error_count;
     SELECT CONCAT(id, ':', v) FROM ordered_default;" \
    >"/tmp/${DATABASE}_ordered_default.out"

expect_value \
    "default order status" \
    "3	0	0" \
    "$(sed -n '1p' "/tmp/${DATABASE}_ordered_default.out")"
expect_value \
    "default order rows" \
    "1:10
2:20
3:30" \
    "$(sed -n '2,$p' "/tmp/${DATABASE}_ordered_default.out")"

run_mysql \
    "USE ${DATABASE};
     CREATE TABLE ordered_desc(id INT, v INT) ENGINE=InnoDB;
     INSERT INTO ordered_desc VALUES (1, 10), (3, 30), (2, 20);
     ALTER TABLE ordered_desc ORDER BY id DESC;
     SELECT ROW_COUNT(), @@warning_count, @@error_count;
     SELECT CONCAT(id, ':', v) FROM ordered_desc;" \
    >"/tmp/${DATABASE}_ordered_desc.out"

expect_value \
    "descending order status" \
    "3	0	0" \
    "$(sed -n '1p' "/tmp/${DATABASE}_ordered_desc.out")"
expect_value \
    "descending order rows" \
    "3:30
2:20
1:10" \
    "$(sed -n '2,$p' "/tmp/${DATABASE}_ordered_desc.out")"

run_mysql \
    "USE ${DATABASE};
     CREATE TABLE ordered_multi(id INT, v INT) ENGINE=InnoDB;
     INSERT INTO ordered_multi VALUES (2, 10), (1, 20), (1, 30);
     ALTER TABLE ordered_multi ORDER BY id ASC, v DESC;
     SELECT ROW_COUNT(), @@warning_count, @@error_count;
     SELECT CONCAT(id, ':', v) FROM ordered_multi;" \
    >"/tmp/${DATABASE}_ordered_multi.out"

expect_value \
    "multi order status" \
    "3	0	0" \
    "$(sed -n '1p' "/tmp/${DATABASE}_ordered_multi.out")"
expect_value \
    "multi order rows" \
    "1:30
1:20
2:10" \
    "$(sed -n '2,$p' "/tmp/${DATABASE}_ordered_multi.out")"

run_mysql \
    "USE ${DATABASE};
     CREATE TABLE ordered_quoted(id INT, \`value\` INT) ENGINE=InnoDB;
     INSERT INTO ordered_quoted VALUES (1, 10), (2, 20), (3, 5);
     ALTER TABLE ordered_quoted ORDER BY \`value\` DESC;
     SELECT ROW_COUNT(), @@warning_count, @@error_count;
     SELECT CONCAT(id, ':', \`value\`) FROM ordered_quoted;" \
    >"/tmp/${DATABASE}_ordered_quoted.out"

expect_value \
    "quoted order status" \
    "3	0	0" \
    "$(sed -n '1p' "/tmp/${DATABASE}_ordered_quoted.out")"
expect_value \
    "quoted order rows" \
    "2:20
1:10
3:5" \
    "$(sed -n '2,$p' "/tmp/${DATABASE}_ordered_quoted.out")"

run_mysql \
    "USE ${DATABASE};
     CREATE TABLE ordered_nulls(id INT NULL, v INT) ENGINE=InnoDB;
     INSERT INTO ordered_nulls VALUES (2, 20), (NULL, 99), (1, 10);
     ALTER TABLE ordered_nulls ORDER BY id;
     SELECT ROW_COUNT(), @@warning_count, @@error_count;
     SELECT CONCAT(IFNULL(CAST(id AS CHAR), 'NULL'), ':', v) FROM ordered_nulls;" \
    >"/tmp/${DATABASE}_ordered_nulls.out"

expect_value \
    "nullable order status" \
    "3	0	0" \
    "$(sed -n '1p' "/tmp/${DATABASE}_ordered_nulls.out")"
expect_value \
    "nullable order rows" \
    "NULL:99
1:10
2:20" \
    "$(sed -n '2,$p' "/tmp/${DATABASE}_ordered_nulls.out")"

run_mysql \
    "USE ${DATABASE};
     CREATE TABLE qualified_target(id INT, v INT) ENGINE=InnoDB;
     INSERT INTO qualified_target VALUES (2, 20), (1, 10);
     ALTER TABLE ${DATABASE}.qualified_target ORDER BY ${DATABASE}.qualified_target.id DESC;
     SELECT ROW_COUNT(), @@warning_count, @@error_count;
     SELECT CONCAT(id, ':', v) FROM qualified_target;
     ALTER TABLE qualified_target ORDER BY qualified_target.id ASC;
     SELECT ROW_COUNT(), @@warning_count, @@error_count;
     SELECT CONCAT(id, ':', v) FROM qualified_target;" \
    >"/tmp/${DATABASE}_qualified_target.out"

expect_value \
    "schema-qualified target status" \
    "2	0	0" \
    "$(sed -n '1p' "/tmp/${DATABASE}_qualified_target.out")"
expect_value \
    "schema-qualified target rows" \
    "2:20
1:10" \
    "$(sed -n '2,3p' "/tmp/${DATABASE}_qualified_target.out")"
expect_value \
    "table-qualified order status" \
    "2	0	0" \
    "$(sed -n '4p' "/tmp/${DATABASE}_qualified_target.out")"
expect_value \
    "table-qualified order rows" \
    "1:10
2:20" \
    "$(sed -n '5,6p' "/tmp/${DATABASE}_qualified_target.out")"

empty_status=$(
    run_mysql \
        "USE ${DATABASE};
         CREATE TABLE empty_table(id INT) ENGINE=InnoDB;
         ALTER TABLE empty_table ORDER BY id;
         SELECT ROW_COUNT(), @@warning_count, @@error_count;"
)
expect_value "empty table status" "0	0	0" "$(printf '%s\n' "$empty_status" | tail -n 1)"

expect_error \
    "missing default database" \
    1046 \
    3D000 \
    "No database selected" \
    "ALTER TABLE ordered_default ORDER BY id;"

expect_error \
    "unknown explicit schema" \
    1049 \
    42000 \
    "Unknown database 'nosuch_schema_${DATABASE}'" \
    "ALTER TABLE nosuch_schema_${DATABASE}.ordered_default ORDER BY id;"

expect_error \
    "unknown table" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing' doesn't exist" \
    "USE ${DATABASE}; ALTER TABLE missing ORDER BY id;"

expect_error \
    "unknown order column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'order clause'" \
    "USE ${DATABASE}; ALTER TABLE ordered_default ORDER BY missing;"

expect_error \
    "wrong table qualifier" \
    1054 \
    42S22 \
    "Unknown column 'other_table.id' in 'order clause'" \
    "USE ${DATABASE}; ALTER TABLE ordered_default ORDER BY other_table.id;"

expect_error \
    "wrong schema qualifier" \
    1054 \
    42S22 \
    "Unknown column 'other_schema.ordered_default.id' in 'order clause'" \
    "USE ${DATABASE}; ALTER TABLE ordered_default ORDER BY other_schema.ordered_default.id;"

expect_error \
    "ordinal order key" \
    1064 \
    42000 \
    "near '1'" \
    "USE ${DATABASE}; ALTER TABLE ordered_default ORDER BY 1;"

expect_error \
    "expression order key" \
    1064 \
    42000 \
    "near '+ 1'" \
    "USE ${DATABASE}; ALTER TABLE ordered_default ORDER BY id + 1;"

expect_error \
    "limit clause" \
    1064 \
    42000 \
    "near 'LIMIT 1'" \
    "USE ${DATABASE}; ALTER TABLE ordered_default ORDER BY id LIMIT 1;"

rm -f \
    "/tmp/${DATABASE}_ordered_default.out" \
    "/tmp/${DATABASE}_ordered_desc.out" \
    "/tmp/${DATABASE}_ordered_multi.out" \
    "/tmp/${DATABASE}_ordered_quoted.out" \
    "/tmp/${DATABASE}_ordered_nulls.out" \
    "/tmp/${DATABASE}_qualified_target.out"
