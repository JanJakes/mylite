#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_varchar_type_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_varchar_type_expectations: $1" >&2
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

case "$(run_mysql "SELECT @@sql_mode;")" in
    *STRICT_TRANS_TABLES*) ;;
    *) fail "expected strict default sql_mode" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

show_columns_expected=$(cat <<\EXPECTED
id	int	NO		NULL	
v	varchar(3)	YES		NULL	
n	varchar(3)	NO		NULL	
z	varchar(0)	YES		NULL	
EXPECTED
)
expect_output \
    "show columns renders varchar descriptors" \
    "$show_columns_expected" \
    "CREATE TABLE strings (id INT NOT NULL, v VARCHAR(3), n VARCHAR(3) NOT NULL, z VARCHAR(0)); "\
"SHOW COLUMNS FROM strings;" \
    "$DATABASE"

show_create_expected=$(cat <<\EXPECTED
strings	CREATE TABLE `strings` (
  `id` int NOT NULL,
  `v` varchar(3) DEFAULT NULL,
  `n` varchar(3) NOT NULL,
  `z` varchar(0) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "show create renders varchar descriptors" \
    "$show_create_expected" \
    "SHOW CREATE TABLE strings;" \
    "$DATABASE"

insert_readback_expected=$(cat <<\EXPECTED
3	0
1	[abc]	3	3	[abc]	[]
2	[a  ]	3	3	[q  ]	[]
3	NULL	NULL	NULL	[nn]	NULL
EXPECTED
)
expect_output \
    "insert values and read back varchar rows" \
    "$insert_readback_expected" \
    "INSERT INTO strings VALUES "\
"(1, 'abc', 'abc', ''), "\
"(2, 'a  ', 'q  ', ''), "\
"(3, NULL, 'nn', NULL); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT id, IF(v IS NULL, 'NULL', CONCAT('[', v, ']')), LENGTH(v), CHAR_LENGTH(v), "\
"CONCAT('[', n, ']'), IF(z IS NULL, 'NULL', CONCAT('[', z, ']')) FROM strings ORDER BY id;" \
    "$DATABASE"

escape_expected=$(cat <<\EXPECTED
612762	780A79
6471	7171
5C25	5C5F
EXPECTED
)
expect_output \
    "ordinary string literal quote forms and escapes" \
    "$escape_expected" \
    "INSERT INTO strings VALUES (4, 'a''b', 'x\\ny', ''), (5, \"dq\", \"qq\", ''); "\
"INSERT INTO strings VALUES (13, '\%', '\_', ''); "\
"SELECT HEX(v), HEX(n) FROM strings WHERE id = 4; "\
"SELECT HEX(v), HEX(n) FROM strings WHERE id = 5; "\
"SELECT HEX(v), HEX(n) FROM strings WHERE id = 13;" \
    "$DATABASE"

expect_output \
    "is null predicates over varchar" \
    "3
1,2,4,5,13" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM strings WHERE v IS NULL; "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM strings WHERE v IS NOT NULL;" \
    "$DATABASE"

update_expected=$(cat <<\EXPECTED
1	0	[xy]
0	0	[xy]
1	0	NULL
EXPECTED
)
expect_output \
    "update string assignment affected rows" \
    "$update_expected" \
    "UPDATE strings SET v = 'xy' WHERE id = 1; "\
"SELECT ROW_COUNT(), @@warning_count, CONCAT('[', v, ']') FROM strings WHERE id = 1; "\
"UPDATE strings SET v = 'xy' WHERE id = 1; "\
"SELECT ROW_COUNT(), @@warning_count, CONCAT('[', v, ']') FROM strings WHERE id = 1; "\
"UPDATE strings SET v = NULL WHERE id = 2; "\
"SELECT ROW_COUNT(), @@warning_count, IF(v IS NULL, 'NULL', v) FROM strings WHERE id = 2;" \
    "$DATABASE"

expect_error \
    "insert overlength value" \
    1406 \
    22001 \
    "Data too long for column 'v' at row 1" \
    "INSERT INTO strings VALUES (10, 'abcd', 'abc', '');" \
    "$DATABASE"

expect_error \
    "varchar zero nonempty value" \
    1406 \
    22001 \
    "Data too long for column 'z' at row 1" \
    "INSERT INTO strings VALUES (10, 'abc', 'abc', 'x');" \
    "$DATABASE"

expect_error \
    "update overlength value" \
    1406 \
    22001 \
    "Data too long for column 'v' at row 1" \
    "UPDATE strings SET v = 'abcd' WHERE id = 1;" \
    "$DATABASE"

trailing_space_expected=$(printf "%b" \
    "1\t2\n"\
"abc\t3\tabc\t3")
expect_output \
    "mysql truncates trailing-space overlength with notes" \
    "$trailing_space_expected" \
    "INSERT INTO strings VALUES (6, 'abc ', 'abc ', ''); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT v, LENGTH(v), n, LENGTH(n) FROM strings WHERE id = 6;" \
    "$DATABASE"

trailing_space_warnings_expected=$(printf "%b" \
    "Note\t1265\tData truncated for column 'v' at row 1\n"\
"Note\t1265\tData truncated for column 'n' at row 1")
expect_output \
    "mysql reports trailing-space truncation notes" \
    "$trailing_space_warnings_expected" \
    "INSERT INTO strings VALUES (9, 'abc ', 'abc ', ''); SHOW WARNINGS;" \
    "$DATABASE"

expect_error \
    "null into varchar not null" \
    1048 \
    23000 \
    "Column 'n' cannot be null" \
    "INSERT INTO strings (id, v, n, z) VALUES (20, 'ok', NULL, '');" \
    "$DATABASE"

expect_error \
    "omitted varchar not null no default" \
    1364 \
    HY000 \
    "Field 'n' doesn't have a default value" \
    "INSERT INTO strings (id, v, z) VALUES (20, 'ok', '');" \
    "$DATABASE"

expect_error \
    "update null into varchar not null" \
    1048 \
    23000 \
    "Column 'n' cannot be null" \
    "UPDATE strings SET n = NULL WHERE id = 1;" \
    "$DATABASE"

ignore_expected=$(printf "%b" \
    "2\t2\t0\n"\
"7\t[ok]\t[]\t[]\n"\
"8\tNULL\t[]\tNULL")
expect_output \
    "insert ignore adjusts varchar null and no-default failures" \
    "$ignore_expected" \
    "INSERT IGNORE INTO strings (id, v, n, z) VALUES "\
"(7, 'ok', NULL, ''), (8, NULL, DEFAULT, DEFAULT); "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SHOW WARNINGS; "\
"SELECT id, IF(v IS NULL, 'NULL', CONCAT('[', v, ']')), CONCAT('[', n, ']'), "\
"IF(z IS NULL, 'NULL', CONCAT('[', z, ']')) FROM strings WHERE id IN (7, 8) ORDER BY id;" \
    "$DATABASE"

ignore_warnings_expected=$(printf "%b" \
    "Warning\t1048\tColumn 'n' cannot be null\n"\
"Warning\t1364\tField 'n' doesn't have a default value")
expect_output \
    "insert ignore reports varchar adjustment warnings" \
    "$ignore_warnings_expected" \
    "INSERT IGNORE INTO strings (id, v, n, z) VALUES "\
"(17, 'ok', NULL, ''), (18, NULL, DEFAULT, DEFAULT); "\
"SHOW WARNINGS;" \
    "$DATABASE"

alter_expected=$(cat <<\EXPECTED
0	0
1	NULL	[]
2	NULL	[]
id	int	NO		NULL	
v	varchar(2)	YES		NULL	
n	varchar(2)	NO		NULL	
EXPECTED
)
expect_output \
    "alter add varchar nullable and not null columns" \
    "$alter_expected" \
    "CREATE TABLE add_t(id INT NOT NULL); "\
"INSERT INTO add_t VALUES (1), (2); "\
"ALTER TABLE add_t ADD COLUMN v VARCHAR(2); "\
"ALTER TABLE add_t ADD COLUMN n VARCHAR(2) NOT NULL; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT id, IF(v IS NULL, 'NULL', v), CONCAT('[', n, ']') FROM add_t ORDER BY id; "\
"SHOW COLUMNS FROM add_t;" \
    "$DATABASE"

like_expected=$(cat <<\EXPECTED
id	int	NO		NULL	
v	varchar(3)	YES		NULL	
n	varchar(3)	NO		NULL	
z	varchar(0)	YES		NULL	
EXPECTED
)
expect_output \
    "create table like clones varchar descriptors" \
    "$like_expected" \
    "CREATE TABLE strings_like LIKE strings; SHOW COLUMNS FROM strings_like;" \
    "$DATABASE"

ctas_expected=$(cat <<\EXPECTED
id	int	NO		NULL	
v	varchar(3)	YES		NULL	
n	varchar(3)	NO		NULL	
1	xy	abc
2	NULL	q  
EXPECTED
)
expect_output \
    "create table select copies varchar descriptors and rows" \
    "$ctas_expected" \
    "CREATE TABLE strings_ctas AS SELECT id, v, n FROM strings WHERE id IN (1, 2) ORDER BY id; "\
"SHOW COLUMNS FROM strings_ctas; "\
"SELECT id, IF(v IS NULL, 'NULL', v), n FROM strings_ctas ORDER BY id;" \
    "$DATABASE"

insert_select_expected=$(cat <<\EXPECTED
2	0
11	aa	pp	
12	NULL	rr	NULL
EXPECTED
)
expect_output \
    "insert select copies compatible varchar rows" \
    "$insert_select_expected" \
    "CREATE TABLE insert_src(id INT NOT NULL, v VARCHAR(3), n VARCHAR(3) NOT NULL, z VARCHAR(0)); "\
"INSERT INTO insert_src VALUES (11, 'aa', 'pp', ''), (12, NULL, 'rr', NULL); "\
"INSERT INTO strings_like SELECT * FROM insert_src; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT id, IF(v IS NULL, 'NULL', v), n, IF(z IS NULL, 'NULL', z) FROM strings_like ORDER BY id;" \
    "$DATABASE"

bounds_expected=$(cat <<\EXPECTED
v0	varchar(0)	YES		NULL	
v255	varchar(255)	YES		NULL	
EXPECTED
)
expect_output \
    "varchar admitted boundary descriptors" \
    "$bounds_expected" \
    "CREATE TABLE bounds(v0 VARCHAR(0), v255 VARCHAR(255)); SHOW COLUMNS FROM bounds;" \
    "$DATABASE"

expect_error \
    "varchar missing length" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "CREATE TABLE missing_length(v VARCHAR);" \
    "$DATABASE"

expect_error \
    "varchar negative length" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "CREATE TABLE negative_length(v VARCHAR(-1));" \
    "$DATABASE"

expect_error \
    "varchar mysql maximum exceeded" \
    1074 \
    42000 \
    "Column length too big for column 'v'" \
    "CREATE TABLE too_wide(v VARCHAR(16384));" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts varchar length beyond mylite baseline" \
    "CREATE TABLE wide_256(v VARCHAR(256)); CREATE TABLE wide_16383(v VARCHAR(16383));" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts string defaults deferred by mylite" \
    "CREATE TABLE default_string(v VARCHAR(3) DEFAULT 'xy');" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts string comparisons deferred by mylite" \
    "SELECT COUNT(*) FROM strings WHERE v = 'xy';" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts string ordering deferred by mylite" \
    "SELECT id FROM strings ORDER BY v LIMIT 2;" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts string distinct and aggregate comparisons deferred by mylite" \
    "SELECT DISTINCT v FROM strings; SELECT COUNT(DISTINCT v) FROM strings; "\
"SELECT MIN(v), MAX(v) FROM strings; SELECT v, COUNT(*) FROM strings GROUP BY v;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_varchar_type_expectations: ok"
