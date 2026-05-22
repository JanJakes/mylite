#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_replace_select_$$"
OTHER_DATABASE="${DATABASE}_other"

fail() {
    printf '%s\n' "mysql_baseline_replace_select_lifecycle_expectations: $1" >&2
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
     REPLACE INTO dst(id, n, b, iu)
         SELECT id, n, b, iu FROM src WHERE id >= 2 ORDER BY id DESC LIMIT 1;
     SELECT ROW_COUNT(), @@warning_count, @@error_count;
     SELECT id, n, b, iu, inv FROM dst ORDER BY id;
     REPLACE dst(id, n, b, iu) SELECT id, n, b, iu FROM src WHERE id = 1;
     SELECT ROW_COUNT(), @@warning_count, @@error_count;
     SELECT id, n, b, iu, inv FROM dst ORDER BY id;" \
    >"/tmp/${DATABASE}_basic.out"

expect_value \
    "basic replace-select status" \
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
         REPLACE INTO required_target(id) SELECT id FROM src WHERE id = 999;
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
         REPLACE INTO default_target(id, n, req) SELECT id, n, b FROM src WHERE id = 1;
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
         REPLACE INTO invisible_target SELECT * FROM src WHERE id = 2;
         REPLACE INTO invisible_target(id, n, b, iu, inv)
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
         REPLACE INTO ${OTHER_DATABASE}.qualified_dst(id, n)
             SELECT id, n FROM ${DATABASE}.src WHERE id = 1;
         SELECT ROW_COUNT(), @@warning_count, @@error_count;
         SELECT id, n FROM ${OTHER_DATABASE}.qualified_dst;"
)
expect_value \
    "qualified target and source without default database" \
    "1	0	0
1	10" \
    "$(printf '%s\n' "$qualified_status" | tail -n 2)"

no_key_status=$(
    run_mysql \
        "USE ${DATABASE};
         CREATE TABLE no_key(id INT, v INT);
         REPLACE INTO no_key SELECT 1, 10;
         REPLACE INTO no_key SELECT 1, 20;
         SELECT ROW_COUNT(), @@warning_count, COUNT(*),
             GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY v)
         FROM no_key;"
)
expect_value \
    "replace-select without key remains insert equivalent" \
    "1	0	2	1:10,1:20" \
    "$(printf '%s\n' "$no_key_status" | tail -n 1)"

keyed_status=$(
    run_mysql \
        "USE ${DATABASE};
         CREATE TABLE keyed(id INT PRIMARY KEY, v INT);
         INSERT INTO keyed VALUES (1, 10);
         CREATE TABLE keyed_src(id INT, v INT);
         INSERT INTO keyed_src VALUES (1, 20), (2, 30);
         REPLACE INTO keyed SELECT id, v FROM keyed_src ORDER BY id;
         SELECT ROW_COUNT(), @@warning_count, COUNT(*),
             GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id)
         FROM keyed;"
)
expect_value \
    "mysql primary key replace-select deletes and inserts" \
    "3	0	2	1:20,2:30" \
    "$(printf '%s\n' "$keyed_status" | tail -n 1)"

same_table_keyed_status=$(
    run_mysql \
        "USE ${DATABASE};
         CREATE TABLE keyed_same(id INT PRIMARY KEY, v INT);
         INSERT INTO keyed_same VALUES (1, 10), (2, 20);
         REPLACE INTO keyed_same SELECT id, v FROM keyed_same ORDER BY id;
         SELECT ROW_COUNT(), @@warning_count, COUNT(*),
             GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id)
         FROM keyed_same;"
)
expect_value \
    "mysql exact same-table replace-select counts selected rows" \
    "2	0	2	1:10,2:20" \
    "$(printf '%s\n' "$same_table_keyed_status" | tail -n 1)"

unique_status=$(
    run_mysql \
        "USE ${DATABASE};
         CREATE TABLE unique_target(a INT UNIQUE, b INT, v INT);
         INSERT INTO unique_target VALUES (1, 10, 100);
         CREATE TABLE unique_source(a INT, b INT, v INT);
         INSERT INTO unique_source VALUES (1, 20, 200), (2, 30, 300);
         REPLACE INTO unique_target SELECT a, b, v FROM unique_source ORDER BY a;
         SELECT ROW_COUNT(), @@warning_count, COUNT(*),
             GROUP_CONCAT(CONCAT(a, ':', b, ':', v) ORDER BY a)
         FROM unique_target;"
)
expect_value \
    "mysql unique replace-select deletes and inserts" \
    "3	0	2	1:20:200,2:30:300" \
    "$(printf '%s\n' "$unique_status" | tail -n 1)"

multi_unique_status=$(
    run_mysql \
        "USE ${DATABASE};
         CREATE TABLE multi_unique(a INT UNIQUE, b INT UNIQUE, v INT);
         INSERT INTO multi_unique VALUES (1, 10, 100), (2, 20, 200);
         CREATE TABLE multi_unique_source(a INT, b INT, v INT);
         INSERT INTO multi_unique_source VALUES (1, 20, 300);
         REPLACE INTO multi_unique SELECT a, b, v FROM multi_unique_source;
         SELECT ROW_COUNT(), @@warning_count, COUNT(*),
             GROUP_CONCAT(CONCAT(a, ':', b, ':', v) ORDER BY a)
         FROM multi_unique;"
)
expect_value \
    "mysql replace-select can delete multiple unique conflicts" \
    "3	0	1	1:20:300" \
    "$(printf '%s\n' "$multi_unique_status" | tail -n 1)"

composite_unique_status=$(
    run_mysql \
        "USE ${DATABASE};
         CREATE TABLE composite_unique(a INT, b INT, v INT, UNIQUE KEY uq_ab(a, b));
         INSERT INTO composite_unique VALUES (1, 1, 10), (1, 2, 20);
         CREATE TABLE composite_unique_source(a INT, b INT, v INT);
         INSERT INTO composite_unique_source VALUES (1, 1, 100), (2, 2, 200);
         REPLACE INTO composite_unique SELECT a, b, v FROM composite_unique_source ORDER BY a, b;
         SELECT ROW_COUNT(), @@warning_count, COUNT(*),
             GROUP_CONCAT(CONCAT(a, ':', b, ':', v) ORDER BY a, b)
         FROM composite_unique;"
)
expect_value \
    "mysql composite unique replace-select" \
    "3	0	3	1:1:100,1:2:20,2:2:200" \
    "$(printf '%s\n' "$composite_unique_status" | tail -n 1)"

nullable_unique_status=$(
    run_mysql \
        "USE ${DATABASE};
         CREATE TABLE nullable_unique(a INT UNIQUE, v INT);
         INSERT INTO nullable_unique VALUES (NULL, 10);
         CREATE TABLE nullable_unique_source(a INT, v INT);
         INSERT INTO nullable_unique_source VALUES (NULL, 20);
         REPLACE INTO nullable_unique SELECT a, v FROM nullable_unique_source;
         SELECT ROW_COUNT(), @@warning_count, COUNT(*),
             GROUP_CONCAT(COALESCE(CONCAT(a, ':', v), CONCAT('NULL:', v)) ORDER BY v)
         FROM nullable_unique;"
)
expect_value \
    "mysql nullable unique replace-select does not conflict on null" \
    "1	0	2	NULL:10,NULL:20" \
    "$(printf '%s\n' "$nullable_unique_status" | tail -n 1)"

prefix_unique_status=$(
    run_mysql \
        "USE ${DATABASE};
         CREATE TABLE prefix_unique(s VARCHAR(10), v INT, UNIQUE KEY uq_s(s(3)));
         INSERT INTO prefix_unique VALUES ('abcdef', 10);
         CREATE TABLE prefix_unique_source(s VARCHAR(10), v INT);
         INSERT INTO prefix_unique_source VALUES ('abczzz', 20), ('xyz000', 30);
         REPLACE INTO prefix_unique SELECT s, v FROM prefix_unique_source ORDER BY s;
         SELECT ROW_COUNT(), @@warning_count, COUNT(*),
             GROUP_CONCAT(CONCAT(s, ':', v) ORDER BY s)
         FROM prefix_unique;"
)
expect_value \
    "mysql prefix unique replace-select" \
    "3	0	2	abczzz:20,xyz000:30" \
    "$(printf '%s\n' "$prefix_unique_status" | tail -n 1)"

auto_increment_status=$(
    run_mysql \
        "USE ${DATABASE};
         CREATE TABLE auto_inc(id INT AUTO_INCREMENT PRIMARY KEY, v INT UNIQUE);
         INSERT INTO auto_inc(v) VALUES (10), (20);
         CREATE TABLE auto_inc_source(id INT, v INT);
         INSERT INTO auto_inc_source VALUES (NULL, 20), (7, 30);
         REPLACE INTO auto_inc(id, v) SELECT id, v FROM auto_inc_source ORDER BY v;
         SELECT ROW_COUNT(), @@warning_count, LAST_INSERT_ID(), COUNT(*),
             GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id)
         FROM auto_inc;"
)
expect_value \
    "mysql auto increment replace-select" \
    "3	0	3	3	1:10,3:20,7:30" \
    "$(printf '%s\n' "$auto_increment_status" | tail -n 1)"

expect_error \
    "parent foreign key replace-select" \
    1451 \
    23000 \
    "Cannot delete or update a parent row" \
    "USE ${DATABASE};
     CREATE TABLE fk_parent(id INT PRIMARY KEY, v INT);
     CREATE TABLE fk_child(
         id INT PRIMARY KEY,
         parent_id INT,
         FOREIGN KEY(parent_id) REFERENCES fk_parent(id)
     );
     INSERT INTO fk_parent VALUES (1, 10);
     INSERT INTO fk_child VALUES (1, 1);
     CREATE TABLE fk_parent_source(id INT, v INT);
     INSERT INTO fk_parent_source VALUES (1, 20);
     REPLACE INTO fk_parent SELECT id, v FROM fk_parent_source;"

expect_error \
    "child foreign key replace-select" \
    1452 \
    23000 \
    "Cannot add or update a child row" \
    "USE ${DATABASE};
     CREATE TABLE fk_child_source(id INT, parent_id INT);
     INSERT INTO fk_child_source VALUES (2, 999);
     REPLACE INTO fk_child SELECT id, parent_id FROM fk_child_source;"

expect_error \
    "column count mismatch too few" \
    1136 \
    21S01 \
    "Column count doesn't match value count at row 1" \
    "USE ${DATABASE}; REPLACE INTO dst(id, n) SELECT id FROM src;"

expect_error \
    "column count mismatch too many with empty source" \
    1136 \
    21S01 \
    "Column count doesn't match value count at row 1" \
    "USE ${DATABASE}; REPLACE INTO dst(id) SELECT id, n FROM src WHERE id = 999;"

expect_error \
    "omitted not-null no-default with matching source" \
    1364 \
    HY000 \
    "Field 'must' doesn't have a default value" \
    "USE ${DATABASE}; REPLACE INTO required_target(id) SELECT id FROM src WHERE id = 1;"

expect_error \
    "selected null into not null" \
    1048 \
    23000 \
    "Column 'must' cannot be null" \
    "USE ${DATABASE}; REPLACE INTO required_target(id, must) SELECT id, n FROM src WHERE id = 2;"

expect_error \
    "selected integer out of range" \
    1264 \
    22003 \
    "Out of range value for column 'tiny' at row 2" \
    "USE ${DATABASE};
     CREATE TABLE tiny_target(tiny TINYINT);
     CREATE TABLE range_source(id INT NOT NULL, value INT);
     INSERT INTO range_source VALUES (1, 1), (2, 128);
     REPLACE INTO tiny_target(tiny) SELECT value FROM range_source ORDER BY id;"

expect_error \
    "duplicate target column" \
    1110 \
    42000 \
    "Column 'id' specified twice" \
    "USE ${DATABASE}; REPLACE INTO dst(id, id) SELECT id, n FROM src;"

expect_error \
    "unknown target column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'field list'" \
    "USE ${DATABASE}; REPLACE INTO dst(missing) SELECT id FROM src;"

expect_error \
    "unknown source column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'field list'" \
    "USE ${DATABASE}; REPLACE INTO dst(id) SELECT missing FROM src;"

expect_error \
    "missing default schema for target" \
    1046 \
    3D000 \
    "No database selected" \
    "REPLACE INTO dst(id) SELECT id FROM ${DATABASE}.src;"

expect_error \
    "missing default schema for source" \
    1046 \
    3D000 \
    "No database selected" \
    "REPLACE INTO ${DATABASE}.dst(id) SELECT id FROM src;"

expect_error \
    "unknown target schema before source schema" \
    1049 \
    42000 \
    "Unknown database 'nosuch_target_${DATABASE}'" \
    "REPLACE INTO nosuch_target_${DATABASE}.dst(id)
     SELECT id FROM nosuch_source_${DATABASE}.src;"

expect_error \
    "unknown target table before source table" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing_target' doesn't exist" \
    "REPLACE INTO ${DATABASE}.missing_target(id) SELECT id FROM ${DATABASE}.missing_source;"

rm -f "/tmp/${DATABASE}_basic.out"
