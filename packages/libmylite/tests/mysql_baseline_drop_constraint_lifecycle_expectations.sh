#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_drop_constraint_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_drop_constraint_lifecycle_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
              --batch --raw --skip-column-names "$@"
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
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

primary_expected=$(cat <<\EXPECTED
2	0
0
drop_pk	CREATE TABLE `drop_pk` (
  `id` int NOT NULL,
  `v` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
1:10,2:20,1:30
EXPECTED
)
expect_output \
    "drop primary through constraint name" \
    "$primary_expected" \
    "CREATE TABLE drop_pk (id INT PRIMARY KEY, v INT); "\
"INSERT INTO drop_pk VALUES (1,10),(2,20); "\
"ALTER TABLE drop_pk DROP CONSTRAINT \`PRIMARY\`; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'drop_pk' "\
"AND CONSTRAINT_NAME = 'PRIMARY'; "\
"SHOW CREATE TABLE drop_pk; "\
"INSERT INTO drop_pk VALUES (1,30); "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY v) FROM drop_pk;" \
    "$DATABASE"

unique_expected=$(cat <<\EXPECTED
0	0
0
0
0
unique_c	CREATE TABLE `unique_c` (
  `id` int DEFAULT NULL,
  `v` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
1:10,1:20
EXPECTED
)
expect_output \
    "drop named unique constraint metadata" \
    "$unique_expected" \
    "CREATE TABLE unique_c (id INT, v INT, CONSTRAINT uq_id UNIQUE (id)); "\
"INSERT INTO unique_c VALUES (1,10); "\
"ALTER TABLE unique_c DROP CONSTRAINT uq_id; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'unique_c' "\
"AND CONSTRAINT_NAME = 'uq_id'; "\
"SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'unique_c' "\
"AND INDEX_NAME = 'uq_id'; "\
"SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'unique_c' "\
"AND CONSTRAINT_NAME = 'uq_id'; "\
"SHOW CREATE TABLE unique_c; "\
"INSERT INTO unique_c VALUES (1,20); "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY v) FROM unique_c;" \
    "$DATABASE"

unique_index_expected=$(cat <<\EXPECTED
0	0
0
EXPECTED
)
expect_output \
    "drop standalone unique index through constraint name" \
    "$unique_index_expected" \
    "CREATE TABLE unique_idx (id INT); "\
"CREATE UNIQUE INDEX uq_idx_id ON unique_idx (id); "\
"ALTER TABLE unique_idx DROP CONSTRAINT uq_idx_id; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'unique_idx' "\
"AND INDEX_NAME = 'uq_idx_id';" \
    "$DATABASE"

foreign_key_expected=$(cat <<\EXPECTED
0	0
0
0
1
child	CREATE TABLE `child` (
  `id` int NOT NULL,
  `pid` int DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `fk_child_parent` (`pid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "drop foreign key through constraint name" \
    "$foreign_key_expected" \
    "CREATE TABLE parent (id INT PRIMARY KEY); "\
"CREATE TABLE child (id INT PRIMARY KEY, pid INT, "\
"CONSTRAINT fk_child_parent FOREIGN KEY (pid) REFERENCES parent(id)); "\
"ALTER TABLE child DROP CONSTRAINT fk_child_parent; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'child' "\
"AND CONSTRAINT_TYPE = 'FOREIGN KEY'; "\
"SELECT COUNT(*) FROM INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS "\
"WHERE CONSTRAINT_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'child'; "\
"SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'child' "\
"AND INDEX_NAME = 'fk_child_parent'; "\
"SHOW CREATE TABLE child;" \
    "$DATABASE"

check_expected=$(cat <<\EXPECTED
0	0
0
0
1
EXPECTED
)
expect_output \
    "drop check through constraint name" \
    "$check_expected" \
    "CREATE TABLE checked (id INT PRIMARY KEY, v INT, CONSTRAINT chk_v CHECK (v > 0)); "\
"ALTER TABLE checked DROP CONSTRAINT chk_v; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'checked' "\
"AND CONSTRAINT_TYPE = 'CHECK'; "\
"SELECT COUNT(*) FROM INFORMATION_SCHEMA.CHECK_CONSTRAINTS "\
"WHERE CONSTRAINT_SCHEMA = '${DATABASE}' AND CONSTRAINT_NAME = 'chk_v'; "\
"INSERT INTO checked VALUES (1, -1); "\
"SELECT COUNT(*) FROM checked;" \
    "$DATABASE"

ambiguous_expected="2"
expect_output \
    "ambiguous failure preserves metadata" \
    "$ambiguous_expected" \
    "CREATE TABLE ambiguous_c (id INT, CONSTRAINT c UNIQUE (id), CONSTRAINT c CHECK (id > 0)); "\
"SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'ambiguous_c' "\
"AND CONSTRAINT_NAME = 'c';" \
    "$DATABASE"
expect_error \
    "ambiguous drop constraint" \
    3939 \
    HY000 \
    "Table has multiple constraints with the name 'c'" \
    "ALTER TABLE ambiguous_c DROP CONSTRAINT c;" \
    "$DATABASE"

expect_error \
    "unknown constraint" \
    3940 \
    HY000 \
    "Constraint 'no_such_name' does not exist." \
    "CREATE TABLE unknown_c (id INT); ALTER TABLE unknown_c DROP CONSTRAINT no_such_name;" \
    "$DATABASE"

expect_error \
    "nonunique index is not a constraint" \
    3940 \
    HY000 \
    "Constraint 'k_id' does not exist." \
    "CREATE TABLE nonunique_c (id INT, KEY k_id (id)); ALTER TABLE nonunique_c DROP CONSTRAINT k_id;" \
    "$DATABASE"

expect_error \
    "unquoted primary is syntax error" \
    1064 \
    42000 \
    "near 'PRIMARY'" \
    "CREATE TABLE primary_keyword (id INT PRIMARY KEY); ALTER TABLE primary_keyword DROP CONSTRAINT PRIMARY;" \
    "$DATABASE"

expect_error \
    "if exists before name is syntax error" \
    1064 \
    42000 \
    "near 'IF EXISTS" \
    "CREATE TABLE if_exists_before (id INT, CONSTRAINT chk_if CHECK (id > 0)); ALTER TABLE if_exists_before DROP CONSTRAINT IF EXISTS chk_if;" \
    "$DATABASE"

expect_error \
    "if exists after name is syntax error" \
    1064 \
    42000 \
    "near 'IF EXISTS" \
    "CREATE TABLE if_exists_after (id INT, CONSTRAINT chk_if_after CHECK (id > 0)); ALTER TABLE if_exists_after DROP CONSTRAINT chk_if_after IF EXISTS;" \
    "$DATABASE"

expect_error \
    "auto increment primary key dependency" \
    1075 \
    42000 \
    "there can be only one auto column" \
    "CREATE TABLE ai_bad (id INT AUTO_INCREMENT PRIMARY KEY, v INT); ALTER TABLE ai_bad DROP CONSTRAINT \`PRIMARY\`;" \
    "$DATABASE"

expect_error \
    "referenced primary key dependency" \
    1553 \
    HY000 \
    "needed in a foreign key constraint" \
    "CREATE TABLE parent_ref (id INT PRIMARY KEY); "\
"CREATE TABLE child_ref (id INT PRIMARY KEY, pid INT, "\
"CONSTRAINT fk_ref FOREIGN KEY (pid) REFERENCES parent_ref(id)); "\
"ALTER TABLE parent_ref DROP CONSTRAINT \`PRIMARY\`;" \
    "$DATABASE"

option_expected=$(cat <<\EXPECTED
0	0
0
EXPECTED
)
expect_output \
    "drop constraint option tail" \
    "$option_expected" \
    "CREATE TABLE option_c (id INT, CONSTRAINT uq_option UNIQUE (id)); "\
"ALTER TABLE option_c DROP CONSTRAINT uq_option, ALGORITHM=INPLACE, LOCK=NONE; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'option_c' "\
"AND INDEX_NAME = 'uq_option';" \
    "$DATABASE"

cleanup
