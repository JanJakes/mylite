#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_alter_auto_increment_expectations_$$"
MISSING_DATABASE="${DATABASE}_missing"

fail() {
    printf '%s\n' "mysql_baseline_alter_table_auto_increment_option_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --batch --raw --skip-column-names "$@"
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

expect_show_table_status_auto_increment_after_sql() {
    label=$1
    table_name=$2
    expected=$3
    sql=$4
    shift 4

    output=$(
        run_mysql "${sql} SHOW TABLE STATUS LIKE '${table_name}';" "$@" \
            | awk -F '\t' 'NF > 10 { value = $11 } END { print value }'
    )
    if [ "$output" != "$expected" ]; then
        fail "$label: expected SHOW TABLE STATUS Auto_increment [$expected], got [$output]"
    fi
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
}

trap cleanup EXIT

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

upward_expected=$(cat <<\EXPECTED
0	0	1
upward	CREATE TABLE `upward` (
  `id` int NOT NULL AUTO_INCREMENT,
  `v` int DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=10 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
10	1:10,2:20,10:30
upward	CREATE TABLE `upward` (
  `id` int NOT NULL AUTO_INCREMENT,
  `v` int DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=11 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "alter auto_increment upward reset" \
    "$upward_expected" \
    "CREATE TABLE upward (id INT AUTO_INCREMENT PRIMARY KEY, v INT); "\
"INSERT INTO upward (v) VALUES (10),(20); "\
"ALTER TABLE upward AUTO_INCREMENT = 10; "\
"SELECT ROW_COUNT(), @@warning_count, LAST_INSERT_ID(); "\
"SHOW CREATE TABLE upward; "\
"INSERT INTO upward (v) VALUES (30); "\
"SELECT LAST_INSERT_ID(), GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM upward; "\
"SHOW CREATE TABLE upward;" \
    "$DATABASE"
expect_show_table_status_auto_increment_after_sql \
    "alter auto_increment status" \
    "status_upward" \
    "10" \
    "CREATE TABLE status_upward (id INT AUTO_INCREMENT PRIMARY KEY, v INT); "\
"INSERT INTO status_upward (v) VALUES (10),(20); "\
"ALTER TABLE status_upward AUTO_INCREMENT = 10;" \
    "$DATABASE"

lower_expected=$(cat <<\EXPECTED
lowered	CREATE TABLE `lowered` (
  `id` int NOT NULL AUTO_INCREMENT,
  `v` int DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=5 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
5	1,2,5
lowered	CREATE TABLE `lowered` (
  `id` int NOT NULL AUTO_INCREMENT,
  `v` int DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=6 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
6	1,2,5,6
EXPECTED
)
expect_output \
    "alter auto_increment can lower after delete but not below max row" \
    "$lower_expected" \
    "CREATE TABLE lowered (id INT AUTO_INCREMENT PRIMARY KEY, v INT); "\
"INSERT INTO lowered (v) VALUES (1),(2),(3),(4),(5),(6),(7),(8),(9),(10); "\
"DELETE FROM lowered WHERE id > 2; "\
"ALTER TABLE lowered AUTO_INCREMENT 5; "\
"SHOW CREATE TABLE lowered; "\
"INSERT INTO lowered (v) VALUES (50); "\
"SELECT LAST_INSERT_ID(), GROUP_CONCAT(id ORDER BY id) FROM lowered; "\
"ALTER TABLE lowered AUTO_INCREMENT = 3; "\
"SHOW CREATE TABLE lowered; "\
"INSERT INTO lowered (v) VALUES (60); "\
"SELECT LAST_INSERT_ID(), GROUP_CONCAT(id ORDER BY id) FROM lowered;" \
    "$DATABASE"

zero_expected=$(cat <<\EXPECTED
zero_t	CREATE TABLE `zero_t` (
  `id` int NOT NULL AUTO_INCREMENT,
  `v` int DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
zero_t	CREATE TABLE `zero_t` (
  `id` int NOT NULL AUTO_INCREMENT,
  `v` int DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=5 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
5	5
EXPECTED
)
expect_output \
    "alter auto_increment zero and empty table" \
    "$zero_expected" \
    "CREATE TABLE zero_t (id INT AUTO_INCREMENT PRIMARY KEY, v INT); "\
"ALTER TABLE zero_t AUTO_INCREMENT=0; "\
"SHOW CREATE TABLE zero_t; "\
"ALTER TABLE zero_t AUTO_INCREMENT=5; "\
"SHOW CREATE TABLE zero_t; "\
"INSERT INTO zero_t (v) VALUES (5); "\
"SELECT LAST_INSERT_ID(), id FROM zero_t;" \
    "$DATABASE"

no_auto_expected=$(cat <<\EXPECTED
0	0
no_auto	CREATE TABLE `no_auto` (
  `id` int NOT NULL,
  `v` int DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "alter auto_increment no-op on non-auto table" \
    "$no_auto_expected" \
    "CREATE TABLE no_auto (id INT PRIMARY KEY, v INT); "\
"ALTER TABLE no_auto AUTO_INCREMENT=5; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW CREATE TABLE no_auto;" \
    "$DATABASE"
expect_show_table_status_auto_increment_after_sql \
    "alter auto_increment non-auto table status" \
    "no_auto_status" \
    "NULL" \
    "CREATE TABLE no_auto_status (id INT PRIMARY KEY, v INT); "\
"ALTER TABLE no_auto_status AUTO_INCREMENT=5;" \
    "$DATABASE"

qualified_expected=$(cat <<\EXPECTED
qualified	CREATE TABLE `qualified` (
  `id` int NOT NULL AUTO_INCREMENT,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=9 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "schema-qualified alter auto_increment succeeds" \
    "$qualified_expected" \
    "CREATE TABLE qualified (id INT AUTO_INCREMENT PRIMARY KEY); "\
"ALTER TABLE ${DATABASE}.qualified AUTO_INCREMENT=9; "\
"SHOW CREATE TABLE qualified;" \
    "$DATABASE"

expect_error \
    "missing default schema fails" \
    1046 \
    3D000 \
    "No database selected" \
    "ALTER TABLE no_default AUTO_INCREMENT=5;"

expect_error \
    "unknown schema fails" \
    1049 \
    42000 \
    "Unknown database '${MISSING_DATABASE}'" \
    "ALTER TABLE ${MISSING_DATABASE}.missing AUTO_INCREMENT=5;" \
    "$DATABASE"

expect_error \
    "unknown table fails" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing_table' doesn't exist" \
    "ALTER TABLE missing_table AUTO_INCREMENT=5;" \
    "$DATABASE"

expect_error \
    "negative literal fails" \
    1064 \
    42000 \
    "near '-1'" \
    "ALTER TABLE upward AUTO_INCREMENT=-1;" \
    "$DATABASE"

expect_error \
    "unary plus literal fails" \
    1064 \
    42000 \
    "near '+5'" \
    "ALTER TABLE upward AUTO_INCREMENT=+5;" \
    "$DATABASE"

expect_error \
    "string literal fails" \
    1064 \
    42000 \
    "near ''5''" \
    "ALTER TABLE upward AUTO_INCREMENT='5';" \
    "$DATABASE"

expect_error \
    "null literal fails" \
    1064 \
    42000 \
    "near 'NULL'" \
    "ALTER TABLE upward AUTO_INCREMENT=NULL;" \
    "$DATABASE"

expect_error \
    "missing literal fails" \
    1064 \
    42000 \
    "near ''" \
    "ALTER TABLE upward AUTO_INCREMENT;" \
    "$DATABASE"

expect_upstream_accepts \
    "MySQL accepts deferred decimal auto_increment option" \
    "CREATE TABLE deferred_decimal (id INT AUTO_INCREMENT PRIMARY KEY); "\
"ALTER TABLE deferred_decimal AUTO_INCREMENT=1.5;" \
    "$DATABASE"

expect_upstream_accepts \
    "MySQL accepts deferred duplicate auto_increment options" \
    "CREATE TABLE deferred_duplicate (id INT AUTO_INCREMENT PRIMARY KEY); "\
"ALTER TABLE deferred_duplicate AUTO_INCREMENT=5, AUTO_INCREMENT=6;" \
    "$DATABASE"

expect_upstream_accepts \
    "MySQL accepts deferred multi-action auto_increment alter" \
    "CREATE TABLE deferred_multi (id INT AUTO_INCREMENT PRIMARY KEY, v INT); "\
"ALTER TABLE deferred_multi AUTO_INCREMENT=5, ADD COLUMN n INT;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_alter_table_auto_increment_option_expectations: ok"
