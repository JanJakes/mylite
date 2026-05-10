#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_alter_column_visibility_expectations_$$"
MISSING_DATABASE="${DATABASE}_missing"

fail() {
    printf '%s\n' "mysql_baseline_alter_column_visibility_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    set +e
    output=$(printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --batch --raw --skip-column-names "$@")
    status_code=$?
    set -e
    printf '%s\n' "$output" | sed 's/[[:blank:]]*$//'
    return "$status_code"
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
        fail "$label: expected MySQL to accept deferred syntax, got [$output]"
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

expect_error \
    "set invisible without selected schema" \
    1046 \
    3D000 \
    "No database selected" \
    "ALTER TABLE numbers ALTER n SET INVISIBLE;"

expect_error \
    "set invisible qualified unknown schema" \
    1049 \
    42000 \
    "Unknown database '${MISSING_DATABASE}'" \
    "ALTER TABLE ${MISSING_DATABASE}.numbers ALTER n SET INVISIBLE;"

expect_error \
    "set invisible unknown table" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing_numbers' doesn't exist" \
    "ALTER TABLE missing_numbers ALTER n SET INVISIBLE;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE ${DATABASE}.qualified_numbers (id INT NOT NULL, n INT DEFAULT 4); "\
"INSERT INTO ${DATABASE}.qualified_numbers (id, n) VALUES (1, 2);" \
    >/dev/null
qualified_expected=$(cat <<'EXPECTED'
0	0
n	int	YES		4	INVISIBLE
qualified_numbers	CREATE TABLE `qualified_numbers` (
  `id` int NOT NULL,
  `n` int DEFAULT '4' /*!80023 INVISIBLE */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "schema-qualified set invisible without selected schema" \
    "$qualified_expected" \
    "ALTER TABLE ${DATABASE}.qualified_numbers ALTER COLUMN n SET INVISIBLE; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW COLUMNS FROM ${DATABASE}.qualified_numbers LIKE 'n'; "\
"SHOW CREATE TABLE ${DATABASE}.qualified_numbers;"

visibility_expected=$(cat <<'EXPECTED'
0	0
id	int	NO		NULL
v	int	YES		7	INVISIBLE
n	int	YES		NULL
numbers	CREATE TABLE `numbers` (
  `id` int NOT NULL,
  `v` int DEFAULT '7' /*!80023 INVISIBLE */,
  `n` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
1	3
1	2	3
1:2:3,2:7:8
1	0
1:2:3,2:7:8,3:10:NULL
0	0
v	int	YES		7
1:2:3,2:7:8,3:10:NULL
EXPECTED
)
expect_output \
    "visibility metadata and DML behavior" \
    "$visibility_expected" \
    "CREATE TABLE numbers (id INT NOT NULL, v INT DEFAULT 7, n INT DEFAULT NULL); "\
"INSERT INTO numbers (id, v, n) VALUES (1, 2, 3); "\
"ALTER TABLE numbers ALTER COLUMN v SET INVISIBLE; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW COLUMNS FROM numbers; "\
"SHOW CREATE TABLE numbers; "\
"SELECT * FROM numbers; "\
"SELECT id, v, n FROM numbers; "\
"INSERT INTO numbers VALUES (2, 8); "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', v, ':', IFNULL(n, 'NULL')) ORDER BY id) FROM numbers; "\
"INSERT INTO numbers (id, v) VALUES (3, 9); "\
"UPDATE numbers SET v = 10 WHERE id = 3; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', v, ':', IFNULL(n, 'NULL')) ORDER BY id) FROM numbers; "\
"ALTER TABLE numbers ALTER v SET VISIBLE; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW COLUMNS FROM numbers LIKE 'v'; "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', v, ':', IFNULL(n, 'NULL')) ORDER BY id) FROM numbers;" \
    "$DATABASE"

explicit_reference_expected=$(cat <<'EXPECTED'
1	3
1	3
3	1
2	NULL
1	0
1:2:3,2:7:NULL
EXPECTED
)
expect_output \
    "invisible columns remain explicit predicate and order keys" \
    "$explicit_reference_expected" \
    "CREATE TABLE explicit_refs (id INT NOT NULL, v INT DEFAULT 7, n INT DEFAULT NULL); "\
"INSERT INTO explicit_refs (id, v, n) VALUES (1, 2, 3), (2, 7, NULL), (3, 4, 1); "\
"ALTER TABLE explicit_refs ALTER v SET INVISIBLE; "\
"SELECT * FROM explicit_refs WHERE v = 2; "\
"SELECT * FROM explicit_refs ORDER BY v; "\
"DELETE FROM explicit_refs WHERE v = 4; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', v, ':', IFNULL(n, 'NULL')) ORDER BY id) FROM explicit_refs;" \
    "$DATABASE"

dropped_default_expected=$(cat <<'EXPECTED'
drop_default_visibility	CREATE TABLE `drop_default_visibility` (
  `id` int DEFAULT NULL,
  `v` int /*!80023 INVISIBLE */,
  `nn` int NOT NULL /*!80023 INVISIBLE */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
id	int	YES		NULL
v	int	YES		NULL	INVISIBLE
nn	int	NO		NULL	INVISIBLE
EXPECTED
)
expect_output \
    "invisible columns preserve dropped-default metadata" \
    "$dropped_default_expected" \
    "CREATE TABLE drop_default_visibility (id INT, v INT DEFAULT NULL, nn INT NOT NULL DEFAULT 1); "\
"ALTER TABLE drop_default_visibility ALTER v DROP DEFAULT; "\
"ALTER TABLE drop_default_visibility ALTER v SET INVISIBLE; "\
"ALTER TABLE drop_default_visibility ALTER nn DROP DEFAULT; "\
"ALTER TABLE drop_default_visibility ALTER nn SET INVISIBLE; "\
"SHOW CREATE TABLE drop_default_visibility; "\
"SHOW COLUMNS FROM drop_default_visibility;" \
    "$DATABASE"

expect_error \
    "set invisible unknown column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'numbers'" \
    "ALTER TABLE numbers ALTER missing SET INVISIBLE;" \
    "$DATABASE"

expect_error \
    "table-qualified column visibility target syntax error" \
    1064 \
    42000 \
    ".v SET INVISIBLE" \
    "ALTER TABLE numbers ALTER numbers.v SET INVISIBLE;" \
    "$DATABASE"

expect_error \
    "last visible column cannot become invisible" \
    4028 \
    HY000 \
    "A table must have at least one visible column." \
    "CREATE TABLE one_visible (id INT); ALTER TABLE one_visible ALTER id SET INVISIBLE;" \
    "$DATABASE"

expect_error \
    "last visible column cannot be dropped when other columns are invisible" \
    4028 \
    HY000 \
    "A table must have at least one visible column." \
    "CREATE TABLE drop_visible_guard (id INT, hidden INT); "\
"ALTER TABLE drop_visible_guard ALTER hidden SET INVISIBLE; "\
"ALTER TABLE drop_visible_guard DROP COLUMN id;" \
    "$DATABASE"

expect_output \
    "modify and change without visibility attribute make columns visible" \
    "modify_col	int	YES		7
changed_col	int	YES		8" \
    "CREATE TABLE redefine_visibility (id INT, modify_col INT DEFAULT 7, change_col INT DEFAULT 8); "\
"ALTER TABLE redefine_visibility ALTER modify_col SET INVISIBLE; "\
"ALTER TABLE redefine_visibility ALTER change_col SET INVISIBLE; "\
"ALTER TABLE redefine_visibility MODIFY modify_col INT DEFAULT 7; "\
"ALTER TABLE redefine_visibility CHANGE change_col changed_col INT DEFAULT 8; "\
"SHOW COLUMNS FROM redefine_visibility LIKE 'modify_col'; "\
"SHOW COLUMNS FROM redefine_visibility LIKE 'changed_col';" \
    "$DATABASE"

expect_error \
    "invisible not-null no-default omitted insert" \
    1364 \
    HY000 \
    "Field 'b' doesn't have a default value" \
    "CREATE TABLE required_hidden (a INT, b INT NOT NULL); "\
"ALTER TABLE required_hidden ALTER b SET INVISIBLE; "\
"INSERT INTO required_hidden (a) VALUES (1);" \
    "$DATABASE"

expect_output \
    "visible no-op reports zero rows" \
    "0	0
n	int	YES		NULL" \
    "CREATE TABLE visible_noop (id INT, n INT); "\
"ALTER TABLE visible_noop ALTER n SET VISIBLE; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW COLUMNS FROM visible_noop LIKE 'n';" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts multi-action visibility changes deferred by MyLite" \
    "CREATE TABLE multi_visibility (id INT, v INT, n INT); "\
"ALTER TABLE multi_visibility ALTER v SET INVISIBLE, ALTER n SET INVISIBLE;" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts create-table invisible columns deferred by MyLite" \
    "CREATE TABLE created_invisible (a INT INVISIBLE, b INT);" \
    "$DATABASE"
