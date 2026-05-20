#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_parenthesized_string_defaults_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_parenthesized_string_defaults_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift

    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | "$MYSQL_BIN" --no-defaults --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                --batch --raw --skip-column-names "$@"
    else
        printf '%s\n' "$sql" \
            | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --batch --raw \
                --skip-column-names "$@"
    fi
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
}

trap cleanup EXIT

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

show_create_expected=$(cat <<\EXPECTED
text_expr	CREATE TABLE `text_expr` (
  `id` int NOT NULL,
  `tt` tinytext DEFAULT (_utf8mb4'tiny'),
  `t` text DEFAULT (_utf8mb4'abc'),
  `empty_text` text DEFAULT (_utf8mb4''),
  `mt` mediumtext DEFAULT (_utf8mb4'medium'),
  `lt` longtext DEFAULT (_utf8mb4'long'),
  `nullable` text DEFAULT (NULL),
  `nn` text NOT NULL DEFAULT (_utf8mb4'required')
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "show create renders text-family expression defaults" \
    "$show_create_expected" \
    "CREATE TABLE text_expr ("\
"id INT NOT NULL, "\
"tt TINYTEXT DEFAULT ('tiny'), "\
"t TEXT DEFAULT ('abc'), "\
"empty_text TEXT DEFAULT (''), "\
"mt MEDIUMTEXT DEFAULT ('medium'), "\
"lt LONGTEXT DEFAULT ('long'), "\
"nullable TEXT DEFAULT (NULL), "\
"nn TEXT NOT NULL DEFAULT ('required')); "\
"SHOW CREATE TABLE text_expr;" \
    "$DATABASE"

show_columns_expected=$(cat <<\EXPECTED
id	int	NO		NULL	
tt	tinytext	YES		_utf8mb4\'tiny\'	DEFAULT_GENERATED
t	text	YES		_utf8mb4\'abc\'	DEFAULT_GENERATED
empty_text	text	YES		_utf8mb4\'\'	DEFAULT_GENERATED
mt	mediumtext	YES		_utf8mb4\'medium\'	DEFAULT_GENERATED
lt	longtext	YES		_utf8mb4\'long\'	DEFAULT_GENERATED
nullable	text	YES		NULL	DEFAULT_GENERATED
nn	text	NO		_utf8mb4\'required\'	DEFAULT_GENERATED
EXPECTED
)
expect_output \
    "show columns renders text-family expression defaults" \
    "$show_columns_expected" \
    "SHOW COLUMNS FROM text_expr;" \
    "$DATABASE"

expect_output \
    "describe renders text-family expression defaults" \
    "$show_columns_expected" \
    "DESCRIBE text_expr;" \
    "$DATABASE"

expect_output \
    "explain table renders text-family expression defaults" \
    "$show_columns_expected" \
    "EXPLAIN text_expr;" \
    "$DATABASE"

information_schema_expected=$(cat <<\EXPECTED
id	NULL	
tt	_utf8mb4\'tiny\'	DEFAULT_GENERATED
t	_utf8mb4\'abc\'	DEFAULT_GENERATED
empty_text	_utf8mb4\'\'	DEFAULT_GENERATED
mt	_utf8mb4\'medium\'	DEFAULT_GENERATED
lt	_utf8mb4\'long\'	DEFAULT_GENERATED
nullable	NULL	DEFAULT_GENERATED
nn	_utf8mb4\'required\'	DEFAULT_GENERATED
EXPECTED
)
expect_output \
    "information schema renders text-family expression defaults" \
    "$information_schema_expected" \
    "SELECT COLUMN_NAME, COLUMN_DEFAULT, EXTRA FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'text_expr' ORDER BY ORDINAL_POSITION;" \
    "$DATABASE"

dml_expected=$(cat <<\EXPECTED
1	tiny	abc	[]	medium	long	1	required
2	tiny	abc	[]	medium	long	1	required
EXPECTED
)
expect_output \
    "dml materializes text-family expression defaults" \
    "$dml_expected" \
    "INSERT INTO text_expr (id) VALUES (1); "\
"INSERT INTO text_expr VALUES (2, DEFAULT, DEFAULT, DEFAULT, DEFAULT, DEFAULT, DEFAULT, DEFAULT); "\
"SELECT id, tt, t, CONCAT('[', empty_text, ']'), mt, lt, nullable IS NULL, nn "\
"FROM text_expr ORDER BY id;" \
    "$DATABASE"

update_expected=$(cat <<\EXPECTED
0	0
1	abc	1
EXPECTED
)
expect_output \
    "update default reuses text-family expression defaults" \
    "$update_expected" \
    "UPDATE text_expr SET t = DEFAULT, nullable = DEFAULT WHERE id = 1; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT id, t, nullable IS NULL FROM text_expr WHERE id = 1;" \
    "$DATABASE"

replace_expected=$(cat <<\EXPECTED
1	0
1	rep	1	required
EXPECTED
)
expect_output \
    "replace materializes text-family expression defaults" \
    "$replace_expected" \
    "CREATE TABLE text_replace (id INT NOT NULL, t TEXT DEFAULT ('rep'), "\
"nullable TEXT DEFAULT (NULL), nn TEXT NOT NULL DEFAULT ('required')); "\
"REPLACE INTO text_replace (id) VALUES (1); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT id, t, nullable IS NULL, nn FROM text_replace;" \
    "$DATABASE"

like_expected=$(cat <<\EXPECTED
text_like	CREATE TABLE `text_like` (
  `id` int NOT NULL,
  `tt` tinytext DEFAULT (_utf8mb4'tiny'),
  `t` text DEFAULT (_utf8mb4'abc'),
  `empty_text` text DEFAULT (_utf8mb4''),
  `mt` mediumtext DEFAULT (_utf8mb4'medium'),
  `lt` longtext DEFAULT (_utf8mb4'long'),
  `nullable` text DEFAULT (NULL),
  `nn` text NOT NULL DEFAULT (_utf8mb4'required')
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "create table like preserves text-family expression defaults" \
    "$like_expected" \
    "CREATE TABLE text_like LIKE text_expr; SHOW CREATE TABLE text_like;" \
    "$DATABASE"

alter_expected=$(cat <<\EXPECTED
1	NULL	add
2	mod	add
alter_text	CREATE TABLE `alter_text` (
  `id` int NOT NULL,
  `a` text DEFAULT (_utf8mb4'mod'),
  `b` text DEFAULT (_utf8mb4'add')
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "alter add and modify preserve text-family expression defaults" \
    "$alter_expected" \
    "CREATE TABLE alter_text (id INT NOT NULL, a TEXT); "\
"ALTER TABLE alter_text ADD COLUMN b TEXT DEFAULT ('add'); "\
"INSERT INTO alter_text (id) VALUES (1); "\
"ALTER TABLE alter_text MODIFY COLUMN a TEXT DEFAULT ('mod'); "\
"INSERT INTO alter_text (id) VALUES (2); "\
"SELECT id, a, b FROM alter_text ORDER BY id; "\
"SHOW CREATE TABLE alter_text;" \
    "$DATABASE"

expect_error \
    "bare text default is rejected" \
    1101 \
    42000 \
    "can't have a default value" \
    "CREATE TABLE bad_bare (a TEXT DEFAULT 'abc');" \
    "$DATABASE"

expect_error \
    "alter set default text expression is rejected" \
    1101 \
    42000 \
    "can't have a default value" \
    "CREATE TABLE alter_set_text (a TEXT); "\
"ALTER TABLE alter_set_text ALTER COLUMN a SET DEFAULT ('set');" \
    "$DATABASE"

expect_error \
    "generated null default into not-null text is rejected on insert" \
    1048 \
    23000 \
    "Column 'a' cannot be null" \
    "CREATE TABLE bad_notnull_null (a TEXT NOT NULL DEFAULT (NULL)); "\
"INSERT INTO bad_notnull_null () VALUES ();" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts deferred numeric text expression default" \
    "CREATE TABLE bad_numeric (a TEXT DEFAULT (123));" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts deferred function text expression default" \
    "CREATE TABLE bad_func (a TEXT DEFAULT (CONCAT('a','b')));" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts deferred varchar expression default" \
    "CREATE TABLE bad_varchar_expr (a VARCHAR(3) DEFAULT ('abc'));" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts deferred blob expression default" \
    "CREATE TABLE bad_blob_expr (a BLOB DEFAULT ('abc'));" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts deferred json expression default" \
    "CREATE TABLE bad_json_expr (a JSON DEFAULT ('{}'));" \
    "$DATABASE"
