#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_drupal_entity_kernel_$$"

fail() {
    printf '%s\n' "mysql_drupal_entity_kernel_compatibility_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw --skip-column-names "$@"
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

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup

run_mysql \
    "CREATE DATABASE ${DATABASE} DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci;
     USE ${DATABASE};
     CREATE TABLE utf8_binary (
       v VARCHAR(8) BINARY NOT NULL,
       t TEXT BINARY
     );
     CREATE TABLE ascii_default (
       v VARCHAR(8) BINARY NOT NULL
     ) DEFAULT CHARSET=ascii;
     CREATE TABLE ascii_explicit (
       v VARCHAR(8) CHARACTER SET ascii BINARY NOT NULL
     );
     CREATE TABLE base (
       id INT NOT NULL,
       revision_id INT NULL
     );
     CREATE TABLE revision (
       id INT NOT NULL,
       revision_id INT NULL
     );
     INSERT INTO base VALUES (1, 10), (2, 20), (3, NULL);
     INSERT INTO revision VALUES (1, 10), (2, 21), (3, NULL);
     CREATE TABLE colors (
       owner_id INT NOT NULL,
       color VARCHAR(16) NULL,
       format VARCHAR(16) NULL
     );
     INSERT INTO colors VALUES
       (1, 'red', 'plain'),
       (1, 'Blue', 'rich'),
       (2, NULL, NULL),
       (2, 'green', 'basic');" >/dev/null

metadata=$(run_mysql \
    "USE ${DATABASE};
     SELECT TABLE_NAME, COLUMN_NAME, CHARACTER_SET_NAME, COLLATION_NAME, COLUMN_TYPE
       FROM information_schema.columns
      WHERE table_schema = '${DATABASE}'
        AND TABLE_NAME IN ('ascii_default', 'ascii_explicit', 'utf8_binary')
      ORDER BY TABLE_NAME, COLUMN_NAME;"
)
expect_value \
    "binary shorthand metadata" \
    "ascii_default	v	ascii	ascii_bin	varchar(8)
ascii_explicit	v	ascii	ascii_bin	varchar(8)
utf8_binary	t	utf8mb4	utf8mb4_bin	text
utf8_binary	v	utf8mb4	utf8mb4_bin	varchar(8)" \
    "$metadata"

case_rows=$(run_mysql \
    "USE ${DATABASE};
     SELECT base.id,
            CASE base.revision_id WHEN revision.revision_id THEN 1 ELSE 0 END AS isDefaultRevision
       FROM base
       JOIN revision ON revision.id = base.id
      ORDER BY base.id;"
)
expect_value \
    "joined simple case rows" \
    "1	1
2	0
3	0" \
    "$case_rows"

string_min_max=$(run_mysql \
    "USE ${DATABASE};
     SELECT MIN(color), MAX(color), MIN(format), MAX(format) FROM colors;
     SELECT owner_id, MIN(color), MAX(color), MIN(format), MAX(format)
       FROM colors
      GROUP BY owner_id
      ORDER BY owner_id;
     SELECT MIN(color), MAX(color) FROM colors WHERE owner_id = 99;"
)
expect_value "string aggregate all rows" "Blue	red	basic	rich" "$(printf '%s\n' "$string_min_max" | sed -n '1p')"
expect_value "string aggregate group 1" "1	Blue	red	plain	rich" "$(printf '%s\n' "$string_min_max" | sed -n '2p')"
expect_value "string aggregate group 2" "2	green	green	basic	basic" "$(printf '%s\n' "$string_min_max" | sed -n '3p')"
expect_value "string aggregate empty" "NULL	NULL" "$(printf '%s\n' "$string_min_max" | sed -n '4p')"

printf '%s\n' "mysql_drupal_entity_kernel_compatibility_expectations: ok"
