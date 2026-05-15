#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_column_charset_collation_$$"

fail() {
    printf '%s\n' "mysql_baseline_column_charset_collation_attributes: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw --skip-column-names "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw "$@"
}

expect_value() {
    label=$1
    expected=$2
    actual=$3

    if [ "$actual" != "$expected" ]; then
        fail "$label: expected [$expected], got [$actual]"
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
        *) fail "$label: expected error $code/$state containing [$message], got [$output]" ;;
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

expect_show_create() {
    show_label=$1
    show_table=$2
    expected_create=$3

    show_output=$(run_mysql_with_headers "USE ${DATABASE}; SHOW CREATE TABLE ${show_table};")
    show_headers=$(printf '%s\n' "$show_output" | sed -n '1p')
    show_table_name=$(printf '%s\n' "$show_output" | sed -n '2p' | cut -f 1)
    show_create_text=$(printf '%s\n' "$show_output" | sed -n '2,$p' | cut -f 2-)

    expect_value "$show_label headers" "Table	Create Table" "$show_headers"
    expect_value "$show_label table" "$show_table" "$show_table_name"
    expect_value "$show_label create" "$expected_create" "$show_create_text"
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
run_mysql "CREATE DATABASE ${DATABASE} DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci;" >/dev/null

run_mysql \
    "USE ${DATABASE};
     CREATE TABLE explicit_attrs (
       id INT,
       v_charset VARCHAR(10) CHARACTER SET utf8mb4,
       v_bin VARCHAR(10) COLLATE utf8mb4_bin,
       c_general CHAR(5) CHARSET utf8mb4 COLLATE utf8mb4_general_ci,
       t_unicode TEXT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
       mt_520 MEDIUMTEXT COLLATE utf8mb4_unicode_520_ci,
       KEY key_v_bin (v_bin(3))
     ) DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;
     INSERT INTO explicit_attrs VALUES (1, 'one', 'two', 'tri', 'text', 'medium');
     CREATE TABLE inherited_bin (
       v VARCHAR(10),
       c CHAR(5),
       t TEXT
     ) DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;
     CREATE TABLE charset_only_bin (
       v VARCHAR(10) CHARACTER SET utf8mb4,
       t TEXT CHARACTER SET utf8mb4
     ) DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;
     CREATE TABLE collate_only_bin (
       v VARCHAR(10) COLLATE utf8mb4_unicode_ci,
       t TEXT COLLATE utf8mb4_general_ci
     ) DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;
     CREATE TABLE like_explicit LIKE explicit_attrs;
     CREATE TABLE ctas_explicit AS SELECT v_bin, t_unicode FROM explicit_attrs;
     CREATE TABLE alter_attrs (id INT, v VARCHAR(10)) DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;
     ALTER TABLE alter_attrs ADD COLUMN added TEXT CHARACTER SET utf8mb4;
     ALTER TABLE alter_attrs MODIFY COLUMN v VARCHAR(12) COLLATE utf8mb4_unicode_ci;
     ALTER TABLE alter_attrs CHANGE COLUMN added renamed TEXT COLLATE utf8mb4_0900_ai_ci;" >/dev/null

expected_explicit=$(cat <<\EXPECTED
CREATE TABLE `explicit_attrs` (
  `id` int DEFAULT NULL,
  `v_charset` varchar(10) CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci DEFAULT NULL,
  `v_bin` varchar(10) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin DEFAULT NULL,
  `c_general` char(5) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci DEFAULT NULL,
  `t_unicode` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `mt_520` mediumtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_520_ci,
  KEY `key_v_bin` (`v_bin`(3))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_show_create "explicit attrs" "explicit_attrs" "$expected_explicit"

expected_inherited=$(cat <<\EXPECTED
CREATE TABLE `inherited_bin` (
  `v` varchar(10) COLLATE utf8mb4_bin DEFAULT NULL,
  `c` char(5) COLLATE utf8mb4_bin DEFAULT NULL,
  `t` text COLLATE utf8mb4_bin
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin
EXPECTED
)
expect_show_create "inherited bin" "inherited_bin" "$expected_inherited"

expected_charset_only=$(cat <<\EXPECTED
CREATE TABLE `charset_only_bin` (
  `v` varchar(10) CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci DEFAULT NULL,
  `t` text CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin
EXPECTED
)
expect_show_create "charset only on bin table" "charset_only_bin" "$expected_charset_only"

expected_collate_only=$(cat <<\EXPECTED
CREATE TABLE `collate_only_bin` (
  `v` varchar(10) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `t` text CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin
EXPECTED
)
expect_show_create "collate only on bin table" "collate_only_bin" "$expected_collate_only"

show_full_expected=$(cat <<\EXPECTED
id	int	NULL	YES		NULL		select,insert,update,references	
v_charset	varchar(10)	utf8mb4_0900_ai_ci	YES		NULL		select,insert,update,references	
v_bin	varchar(10)	utf8mb4_bin	YES	MUL	NULL		select,insert,update,references	
c_general	char(5)	utf8mb4_general_ci	YES		NULL		select,insert,update,references	
t_unicode	text	utf8mb4_unicode_ci	YES		NULL		select,insert,update,references	
mt_520	mediumtext	utf8mb4_unicode_520_ci	YES		NULL		select,insert,update,references	
EXPECTED
)
expect_value \
    "show full explicit attrs" \
    "$show_full_expected" \
    "$(run_mysql "USE ${DATABASE}; SHOW FULL COLUMNS FROM explicit_attrs;")"

information_schema_expected=$(cat <<\EXPECTED
id	int	NULL	NULL	int
v_charset	varchar	utf8mb4	utf8mb4_0900_ai_ci	varchar(10)
v_bin	varchar	utf8mb4	utf8mb4_bin	varchar(10)
c_general	char	utf8mb4	utf8mb4_general_ci	char(5)
t_unicode	text	utf8mb4	utf8mb4_unicode_ci	text
mt_520	mediumtext	utf8mb4	utf8mb4_unicode_520_ci	mediumtext
EXPECTED
)
expect_value \
    "information schema explicit attrs" \
    "$information_schema_expected" \
    "$(run_mysql "SELECT COLUMN_NAME, DATA_TYPE, CHARACTER_SET_NAME, COLLATION_NAME, COLUMN_TYPE FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA='${DATABASE}' AND TABLE_NAME='explicit_attrs' ORDER BY ORDINAL_POSITION;")"

expected_like=$(cat <<\EXPECTED
CREATE TABLE `like_explicit` (
  `id` int DEFAULT NULL,
  `v_charset` varchar(10) CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci DEFAULT NULL,
  `v_bin` varchar(10) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin DEFAULT NULL,
  `c_general` char(5) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci DEFAULT NULL,
  `t_unicode` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `mt_520` mediumtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_520_ci,
  KEY `key_v_bin` (`v_bin`(3))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_show_create "like clone attrs" "like_explicit" "$expected_like"

expected_ctas=$(cat <<\EXPECTED
CREATE TABLE `ctas_explicit` (
  `v_bin` varchar(10) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin DEFAULT NULL,
  `t_unicode` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_show_create "ctas attrs" "ctas_explicit" "$expected_ctas"

expected_alter=$(cat <<\EXPECTED
CREATE TABLE `alter_attrs` (
  `id` int DEFAULT NULL,
  `v` varchar(12) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `renamed` text CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin
EXPECTED
)
expect_show_create "alter attrs" "alter_attrs" "$expected_alter"

row_values=$(run_mysql "USE ${DATABASE}; UPDATE explicit_attrs SET t_unicode = 'changed' WHERE id = 1; SELECT ROW_COUNT(), @@warning_count, v_bin, t_unicode FROM explicit_attrs;")
expect_value "row DML unchanged by metadata" "1	0	two	changed" "$row_values"

expect_error \
    "column charset equal syntax" \
    1064 \
    42000 \
    "near '=utf8mb4)'" \
    "CREATE DATABASE IF NOT EXISTS ${DATABASE}; USE ${DATABASE}; CREATE TABLE bad_equal (v VARCHAR(10) CHARACTER SET=utf8mb4);"

expect_error \
    "column collate equal syntax" \
    1064 \
    42000 \
    "near '=utf8mb4_bin)'" \
    "CREATE DATABASE IF NOT EXISTS ${DATABASE}; USE ${DATABASE}; CREATE TABLE bad_equal_collate (v VARCHAR(10) COLLATE=utf8mb4_bin);"

expect_error \
    "column charset default syntax" \
    1064 \
    42000 \
    "near 'DEFAULT)'" \
    "CREATE DATABASE IF NOT EXISTS ${DATABASE}; USE ${DATABASE}; CREATE TABLE bad_default_charset (v VARCHAR(10) CHARACTER SET DEFAULT);"

expect_error \
    "column collate default syntax" \
    1064 \
    42000 \
    "near 'DEFAULT)'" \
    "CREATE DATABASE IF NOT EXISTS ${DATABASE}; USE ${DATABASE}; CREATE TABLE bad_default_collate (v VARCHAR(10) COLLATE DEFAULT);"

expect_error \
    "unknown column charset" \
    1115 \
    42000 \
    "Unknown character set: 'nosuch_charset'" \
    "CREATE DATABASE IF NOT EXISTS ${DATABASE}; USE ${DATABASE}; CREATE TABLE unknown_charset (v VARCHAR(10) CHARACTER SET nosuch_charset);"

expect_error \
    "unknown column collation" \
    1273 \
    HY000 \
    "Unknown collation: 'nosuch_collation'" \
    "CREATE DATABASE IF NOT EXISTS ${DATABASE}; USE ${DATABASE}; CREATE TABLE unknown_collation (v VARCHAR(10) COLLATE nosuch_collation);"

expect_error \
    "mismatched column collation" \
    1253 \
    42000 \
    "COLLATION 'latin1_swedish_ci' is not valid for CHARACTER SET 'utf8mb4'" \
    "CREATE DATABASE IF NOT EXISTS ${DATABASE}; USE ${DATABASE}; CREATE TABLE mismatched (v VARCHAR(10) CHARACTER SET utf8mb4 COLLATE latin1_swedish_ci);"

expect_error \
    "int charset syntax" \
    1064 \
    42000 \
    "near 'CHARACTER SET utf8mb4)'" \
    "CREATE DATABASE IF NOT EXISTS ${DATABASE}; USE ${DATABASE}; CREATE TABLE int_charset (i INT CHARACTER SET utf8mb4);"

expect_error \
    "blob charset syntax" \
    1064 \
    42000 \
    "near 'CHARACTER SET utf8mb4)'" \
    "CREATE DATABASE IF NOT EXISTS ${DATABASE}; USE ${DATABASE}; CREATE TABLE blob_charset (b BLOB CHARACTER SET utf8mb4);"

expect_error \
    "varbinary nonbinary collation" \
    1253 \
    42000 \
    "COLLATION 'utf8mb4_bin' is not valid for CHARACTER SET 'binary'" \
    "CREATE DATABASE IF NOT EXISTS ${DATABASE}; USE ${DATABASE}; CREATE TABLE binary_collation (b VARBINARY(10) COLLATE utf8mb4_bin);"

expect_error \
    "duplicate charset syntax" \
    1064 \
    42000 \
    "near 'CHARACTER SET utf8mb4)'" \
    "CREATE DATABASE IF NOT EXISTS ${DATABASE}; USE ${DATABASE}; CREATE TABLE duplicate_charset (v VARCHAR(10) CHARACTER SET utf8mb4 CHARACTER SET utf8mb4);"

expect_error \
    "duplicate collation syntax" \
    1064 \
    42000 \
    "Multiple COLLATE clauses" \
    "CREATE DATABASE IF NOT EXISTS ${DATABASE}; USE ${DATABASE}; CREATE TABLE duplicate_collation (v VARCHAR(10) COLLATE utf8mb4_bin COLLATE utf8mb4_bin);"

expect_error \
    "misplaced charset syntax" \
    1064 \
    42000 \
    "near 'CHARACTER SET utf8mb4 DEFAULT 'x')'" \
    "CREATE DATABASE IF NOT EXISTS ${DATABASE}; USE ${DATABASE}; CREATE TABLE misplaced_charset (v VARCHAR(10) NOT NULL CHARACTER SET utf8mb4 DEFAULT 'x');"

expect_error \
    "reversed collate charset syntax" \
    1064 \
    42000 \
    "near 'CHARACTER SET utf8mb4)'" \
    "CREATE DATABASE IF NOT EXISTS ${DATABASE}; USE ${DATABASE}; CREATE TABLE reversed_attrs (v VARCHAR(10) COLLATE utf8mb4_bin CHARACTER SET utf8mb4);"

expect_upstream_accepts \
    "mysql accepts deferred enum collation" \
    "DROP TABLE IF EXISTS enum_collation; CREATE TABLE enum_collation (e ENUM('a','b') COLLATE utf8mb4_bin);" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts deferred set collation" \
    "DROP TABLE IF EXISTS set_collation; CREATE TABLE set_collation (s SET('a','b') COLLATE utf8mb4_bin);" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts deferred ascii attribute" \
    "DROP TABLE IF EXISTS ascii_attr; CREATE TABLE ascii_attr (v VARCHAR(10) ASCII);" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts deferred unicode attribute" \
    "DROP TABLE IF EXISTS unicode_attr; CREATE TABLE unicode_attr (v VARCHAR(10) UNICODE);" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts deferred binary shorthand" \
    "DROP TABLE IF EXISTS binary_attr; CREATE TABLE binary_attr (v VARCHAR(10) BINARY);" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts deferred binary charset descriptor change" \
    "DROP TABLE IF EXISTS binary_charset; CREATE TABLE binary_charset (v VARCHAR(10) CHARACTER SET binary);" \
    "$DATABASE"
