#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_drop_foreign_key_expectations_$$"
MISSING_DATABASE="${DATABASE}_missing"

fail() {
    printf '%s\n' "mysql_baseline_drop_foreign_key_lifecycle_expectations: $1" >&2
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

expect_upstream_accepts() {
    label=$1
    sql=$2
    shift 2

    set +e
    output=$(run_mysql "$sql" "$@" 2>&1)
    status_code=$?
    set -e

    if [ "$status_code" -ne 0 ]; then
        fail "$label: expected MySQL to accept deferred behavior, got [$output]"
    fi
}

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
    run_mysql "DROP DATABASE IF EXISTS ${MISSING_DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

drop_named_expected=$(cat <<\EXPECTED
0	0
child	CREATE TABLE `child` (
  `id` int NOT NULL,
  `pid` int DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `fk_child_parent` (`pid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0
0
0
1
EXPECTED
)
expect_output \
    "drop named foreign key preserves child index and removes metadata" \
    "$drop_named_expected" \
    "CREATE TABLE parent(id INT PRIMARY KEY) ENGINE=InnoDB; "\
"CREATE TABLE child(id INT PRIMARY KEY, pid INT, "\
"CONSTRAINT fk_child_parent FOREIGN KEY(pid) REFERENCES parent(id)) ENGINE=InnoDB; "\
"ALTER TABLE child DROP FOREIGN KEY fk_child_parent; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW CREATE TABLE child; "\
"SELECT COUNT(*) FROM INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS "\
"WHERE CONSTRAINT_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'child'; "\
"SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'child' "\
"AND CONSTRAINT_TYPE = 'FOREIGN KEY'; "\
"SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'child' "\
"AND REFERENCED_TABLE_NAME IS NOT NULL; "\
"INSERT INTO child VALUES (1, 999); "\
"SELECT COUNT(*) FROM child;" \
    "$DATABASE"

schema_qualified_expected=$(cat <<\EXPECTED
0	0
0
1
EXPECTED
)
expect_output \
    "schema-qualified drop foreign key works without default schema" \
    "$schema_qualified_expected" \
    "CREATE TABLE ${DATABASE}.qualified_parent(id INT PRIMARY KEY) ENGINE=InnoDB; "\
"CREATE TABLE ${DATABASE}.qualified_child(id INT PRIMARY KEY, pid INT, "\
"CONSTRAINT fk_qualified_parent FOREIGN KEY(pid) "\
"REFERENCES ${DATABASE}.qualified_parent(id)) ENGINE=InnoDB; "\
"ALTER TABLE ${DATABASE}.qualified_child DROP FOREIGN KEY fk_qualified_parent; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT COUNT(*) FROM INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS "\
"WHERE CONSTRAINT_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'qualified_child'; "\
"INSERT INTO ${DATABASE}.qualified_child VALUES (1, 999); "\
"SELECT COUNT(*) FROM ${DATABASE}.qualified_child;"

case_expected=$(cat <<\EXPECTED
0	0
case_child	CREATE TABLE `case_child` (
  `id` int NOT NULL,
  `pid` int DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `MiXeD_FK` (`pid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "drop foreign key names case-insensitively" \
    "$case_expected" \
    "CREATE TABLE case_parent(id INT PRIMARY KEY) ENGINE=InnoDB; "\
"CREATE TABLE case_child(id INT PRIMARY KEY, pid INT, "\
"CONSTRAINT \`MiXeD_FK\` FOREIGN KEY(pid) REFERENCES case_parent(id)) ENGINE=InnoDB; "\
"ALTER TABLE case_child DROP FOREIGN KEY \`mixed_fk\`; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW CREATE TABLE case_child;" \
    "$DATABASE"

generated_expected=$(cat <<\EXPECTED
unnamed_child_ibfk_1
0	0
unnamed_child	CREATE TABLE `unnamed_child` (
  `id` int NOT NULL,
  `pid` int DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `pid` (`pid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "drop generated foreign key name" \
    "$generated_expected" \
    "CREATE TABLE generated_parent(id INT PRIMARY KEY) ENGINE=InnoDB; "\
"CREATE TABLE unnamed_child(id INT PRIMARY KEY, pid INT, "\
"FOREIGN KEY(pid) REFERENCES generated_parent(id)) ENGINE=InnoDB; "\
"SELECT CONSTRAINT_NAME FROM INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS "\
"WHERE CONSTRAINT_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'unnamed_child'; "\
"ALTER TABLE unnamed_child DROP FOREIGN KEY unnamed_child_ibfk_1; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW CREATE TABLE unnamed_child;" \
    "$DATABASE"

drop_then_drop_index_expected=$(cat <<\EXPECTED
0	0
index_child	CREATE TABLE `index_child` (
  `id` int NOT NULL,
  `pid` int DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "drop preserved child index after foreign key drop" \
    "$drop_then_drop_index_expected" \
    "CREATE TABLE index_parent(id INT PRIMARY KEY) ENGINE=InnoDB; "\
"CREATE TABLE index_child(id INT PRIMARY KEY, pid INT, "\
"CONSTRAINT fk_index_parent FOREIGN KEY(pid) REFERENCES index_parent(id)) ENGINE=InnoDB; "\
"ALTER TABLE index_child DROP FOREIGN KEY fk_index_parent; "\
"ALTER TABLE index_child DROP INDEX fk_index_parent; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW CREATE TABLE index_child;" \
    "$DATABASE"

expect_error \
    "missing default schema fails" \
    1046 \
    3D000 \
    "No database selected" \
    "ALTER TABLE no_default DROP FOREIGN KEY fk;"

expect_error \
    "unknown schema fails" \
    1049 \
    42000 \
    "Unknown database '${MISSING_DATABASE}'" \
    "ALTER TABLE ${MISSING_DATABASE}.missing DROP FOREIGN KEY fk;" \
    "$DATABASE"

expect_error \
    "unknown table fails" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing_table' doesn't exist" \
    "ALTER TABLE missing_table DROP FOREIGN KEY fk;" \
    "$DATABASE"

expect_error \
    "unknown foreign key fails" \
    1091 \
    42000 \
    "Can't DROP 'missing_fk'; check that column/key exists" \
    "CREATE TABLE missing_fk_parent(id INT PRIMARY KEY) ENGINE=InnoDB; "\
"CREATE TABLE missing_fk_child(id INT PRIMARY KEY, pid INT, "\
"CONSTRAINT fk_present FOREIGN KEY(pid) REFERENCES missing_fk_parent(id)) ENGINE=InnoDB; "\
"ALTER TABLE missing_fk_child DROP FOREIGN KEY missing_fk;" \
    "$DATABASE"

expect_error \
    "if exists is syntax error" \
    1064 \
    42000 \
    "SQL syntax" \
    "CREATE TABLE if_exists_parent(id INT PRIMARY KEY) ENGINE=InnoDB; "\
"CREATE TABLE if_exists_child(id INT PRIMARY KEY, pid INT, "\
"CONSTRAINT fk_if_exists FOREIGN KEY(pid) REFERENCES if_exists_parent(id)) ENGINE=InnoDB; "\
"ALTER TABLE if_exists_child DROP FOREIGN KEY IF EXISTS fk_if_exists;" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts multi-action drop foreign key" \
    "CREATE TABLE multi_parent(id INT PRIMARY KEY) ENGINE=InnoDB; "\
"CREATE TABLE multi_child(id INT PRIMARY KEY, pid INT, "\
"CONSTRAINT fk_multi FOREIGN KEY(pid) REFERENCES multi_parent(id)) ENGINE=InnoDB; "\
"ALTER TABLE multi_child DROP FOREIGN KEY fk_multi, DROP INDEX fk_multi;" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts alter options with drop foreign key" \
    "CREATE TABLE option_parent(id INT PRIMARY KEY) ENGINE=InnoDB; "\
"CREATE TABLE option_child(id INT PRIMARY KEY, pid INT, "\
"CONSTRAINT fk_option FOREIGN KEY(pid) REFERENCES option_parent(id)) ENGINE=InnoDB; "\
"ALTER TABLE option_child DROP FOREIGN KEY fk_option, ALGORITHM=INPLACE, LOCK=NONE;" \
    "$DATABASE"

cleanup

printf '%s\n' "mysql_baseline_drop_foreign_key_lifecycle_expectations: ok"
