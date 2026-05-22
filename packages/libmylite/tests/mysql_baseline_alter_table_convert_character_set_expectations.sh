#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_alter_table_convert_charset_$$"
TARGET_DATABASE="${DATABASE}_target"

fail() {
    printf '%s\n' "mysql_baseline_alter_table_convert_character_set_expectations: $1" >&2
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

expect_show_create() {
    label=$1
    table=$2
    expected_create=$3

    output=$(run_mysql_with_headers "USE ${DATABASE}; SHOW CREATE TABLE ${table};")
    headers=$(printf '%s\n' "$output" | sed -n '1p')
    table_name=$(printf '%s\n' "$output" | sed -n '2p' | cut -f 1)
    create_text=$(printf '%s\n' "$output" | sed -n '2,$p' | cut -f 2-)

    expect_value "$label headers" "Table	Create Table" "$headers"
    expect_value "$label table" "$table" "$table_name"
    expect_value "$label create" "$expected_create" "$create_text"
}

expect_show_create_in_schema() {
    label=$1
    schema=$2
    table=$3
    expected_create=$4

    output=$(run_mysql_with_headers "SHOW CREATE TABLE ${schema}.${table};")
    headers=$(printf '%s\n' "$output" | sed -n '1p')
    table_name=$(printf '%s\n' "$output" | sed -n '2p' | cut -f 1)
    create_text=$(printf '%s\n' "$output" | sed -n '2,$p' | cut -f 2-)

    expect_value "$label headers" "Table	Create Table" "$headers"
    expect_value "$label table" "$table" "$table_name"
    expect_value "$label create" "$expected_create" "$create_text"
}

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
    run_mysql "DROP DATABASE IF EXISTS ${TARGET_DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup

run_mysql \
    "CREATE DATABASE ${DATABASE} DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci;
     USE ${DATABASE};
     CREATE TABLE same_charset(
       id INT,
       inherited VARCHAR(10),
       ch CHAR(3),
       body TEXT,
       explicit VARCHAR(10) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin
     ) DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;
     INSERT INTO same_charset VALUES(1, 'abc', 'xy', 'text', 'def');
     ALTER TABLE same_charset CONVERT TO CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
     SELECT @@warning_count, @@error_count, ROW_COUNT();
     CREATE TABLE charset_synonym(v VARCHAR(10));
     ALTER TABLE charset_synonym CONVERT TO CHARSET utf8mb4;
     SELECT @@warning_count, @@error_count, ROW_COUNT();
     CREATE TABLE quoted_names(v VARCHAR(10));
     ALTER TABLE quoted_names CONVERT TO CHARACTER SET \`utf8mb4\` COLLATE \`utf8mb4_bin\`;
     SELECT @@warning_count, @@error_count, ROW_COUNT();
     CREATE TABLE string_names(v VARCHAR(10));
     ALTER TABLE string_names CONVERT TO CHARACTER SET 'utf8mb4' COLLATE \"utf8mb4_bin\";
     SELECT @@warning_count, @@error_count, ROW_COUNT();
     CREATE TABLE default_target(v VARCHAR(10)) DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;
     ALTER TABLE default_target CONVERT TO CHARACTER SET DEFAULT;
     SELECT @@warning_count, @@error_count, ROW_COUNT();" >/tmp/mylite_convert_charset_mysql.out

status_lines=$(tail -n 5 /tmp/mylite_convert_charset_mysql.out)
expect_value "successful status rows" "0	0	0
0	0	0
0	0	0
0	0	0
0	0	0" "$status_lines"
rm -f /tmp/mylite_convert_charset_mysql.out

expected_same_charset="CREATE TABLE \`same_charset\` (
  \`id\` int DEFAULT NULL,
  \`inherited\` varchar(10) COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  \`ch\` char(3) COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  \`body\` text COLLATE utf8mb4_unicode_ci,
  \`explicit\` varchar(10) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci"
expected_charset_synonym="CREATE TABLE \`charset_synonym\` (
  \`v\` varchar(10) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci"
expected_quoted_names="CREATE TABLE \`quoted_names\` (
  \`v\` varchar(10) COLLATE utf8mb4_bin DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin"
expected_string_names="CREATE TABLE \`string_names\` (
  \`v\` varchar(10) COLLATE utf8mb4_bin DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin"
expected_default_target="CREATE TABLE \`default_target\` (
  \`v\` varchar(10) COLLATE utf8mb4_general_ci DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci"

expect_show_create "same charset convert" "same_charset" "$expected_same_charset"
expect_show_create "charset synonym" "charset_synonym" "$expected_charset_synonym"
expect_show_create "quoted names" "quoted_names" "$expected_quoted_names"
expect_show_create "string names" "string_names" "$expected_string_names"
expect_show_create "default target" "default_target" "$expected_default_target"

run_mysql \
    "CREATE DATABASE ${TARGET_DATABASE} DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_bin;
     CREATE TABLE ${TARGET_DATABASE}.no_selected_default(v VARCHAR(10))
       DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;
     ALTER TABLE ${TARGET_DATABASE}.no_selected_default CONVERT TO CHARACTER SET DEFAULT;
     SELECT @@warning_count, @@error_count, ROW_COUNT();
     USE ${DATABASE};
     CREATE TABLE ${TARGET_DATABASE}.selected_other_default(v VARCHAR(10))
       DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;
     ALTER TABLE ${TARGET_DATABASE}.selected_other_default CONVERT TO CHARACTER SET DEFAULT;
     SELECT @@warning_count, @@error_count, ROW_COUNT();" >/tmp/mylite_convert_charset_default.out
default_status_lines=$(cat /tmp/mylite_convert_charset_default.out)
expect_value "default resolution status rows" "0	0	0
0	0	0" "$default_status_lines"
rm -f /tmp/mylite_convert_charset_default.out

expected_no_selected_default="CREATE TABLE \`no_selected_default\` (
  \`v\` varchar(10) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci"
expected_selected_other_default="CREATE TABLE \`selected_other_default\` (
  \`v\` varchar(10) COLLATE utf8mb4_general_ci DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci"

expect_show_create_in_schema \
    "default without selected database" \
    "$TARGET_DATABASE" \
    "no_selected_default" \
    "$expected_no_selected_default"
expect_show_create_in_schema \
    "default with different selected database" \
    "$TARGET_DATABASE" \
    "selected_other_default" \
    "$expected_selected_other_default"

rows=$(run_mysql "USE ${DATABASE}; SELECT id, inherited, ch, body, explicit FROM same_charset;")
expect_value "converted rows remain readable" "1	abc	xy	text	def" "$rows"

columns=$(run_mysql \
    "SELECT COLUMN_NAME, CHARACTER_SET_NAME, COLLATION_NAME
       FROM INFORMATION_SCHEMA.COLUMNS
      WHERE TABLE_SCHEMA='${DATABASE}' AND TABLE_NAME='same_charset'
      ORDER BY ORDINAL_POSITION;")
expect_value "converted column metadata" "id	NULL	NULL
inherited	utf8mb4	utf8mb4_unicode_ci
ch	utf8mb4	utf8mb4_unicode_ci
body	utf8mb4	utf8mb4_unicode_ci
explicit	utf8mb4	utf8mb4_unicode_ci" "$columns"

table_metadata=$(run_mysql \
    "SELECT TABLE_COLLATION
       FROM INFORMATION_SCHEMA.TABLES
      WHERE TABLE_SCHEMA='${DATABASE}' AND TABLE_NAME='same_charset';")
expect_value "converted table collation" "utf8mb4_unicode_ci" "$table_metadata"

run_mysql \
    "USE ${DATABASE};
     CREATE TABLE cross_charset(v VARCHAR(10)) DEFAULT CHARSET=ascii COLLATE=ascii_general_ci;
     INSERT INTO cross_charset VALUES('abc');
     ALTER TABLE cross_charset CONVERT TO CHARACTER SET utf8mb4;
     SELECT @@warning_count, @@error_count, ROW_COUNT();
     SHOW CREATE TABLE cross_charset;" >/tmp/mylite_convert_charset_cross.out
cross_status=$(grep -m 1 '^0	0	1$' /tmp/mylite_convert_charset_cross.out || true)
case "$cross_status" in
    "0	0	1") ;;
    *) fail "cross-character-set MySQL conversion expectation not observed" ;;
esac
rm -f /tmp/mylite_convert_charset_cross.out

run_mysql \
    "USE ${DATABASE};
     CREATE TABLE binary_target(v VARCHAR(10), c CHAR(3), t TEXT);
     INSERT INTO binary_target VALUES('abc', 'xy', 'text');
     ALTER TABLE binary_target CONVERT TO CHARACTER SET binary;
     SELECT @@warning_count, @@error_count, ROW_COUNT();
     SHOW CREATE TABLE binary_target;" >/tmp/mylite_convert_charset_binary.out
binary_status=$(grep -m 1 '^0	0	1$' /tmp/mylite_convert_charset_binary.out || true)
case "$binary_status" in
    "0	0	1") ;;
    *) fail "binary MySQL conversion expectation not observed" ;;
esac
rm -f /tmp/mylite_convert_charset_binary.out

expect_error \
    "missing default database" \
    1046 \
    3D000 \
    "No database selected" \
    "ALTER TABLE same_charset CONVERT TO CHARACTER SET utf8mb4;"

expect_error \
    "unknown explicit schema" \
    1049 \
    42000 \
    "Unknown database 'nosuch_schema_${DATABASE}'" \
    "ALTER TABLE nosuch_schema_${DATABASE}.same_charset CONVERT TO CHARACTER SET utf8mb4;"

expect_error \
    "unknown table" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing' doesn't exist" \
    "USE ${DATABASE}; ALTER TABLE missing CONVERT TO CHARACTER SET utf8mb4;"

expect_error \
    "unknown charset" \
    1115 \
    42000 \
    "Unknown character set: 'nosuch_charset'" \
    "USE ${DATABASE}; ALTER TABLE same_charset CONVERT TO CHARACTER SET nosuch_charset;"

expect_error \
    "unknown collation" \
    1273 \
    HY000 \
    "Unknown collation: 'nosuch_collation'" \
    "USE ${DATABASE}; ALTER TABLE same_charset CONVERT TO CHARACTER SET utf8mb4 COLLATE nosuch_collation;"

expect_error \
    "collation mismatch" \
    1253 \
    42000 \
    "COLLATION 'ascii_bin' is not valid for CHARACTER SET 'utf8mb4'" \
    "USE ${DATABASE}; ALTER TABLE same_charset CONVERT TO CHARACTER SET utf8mb4 COLLATE ascii_bin;"

expect_error \
    "equals syntax" \
    1064 \
    42000 \
    "near '=utf8mb4'" \
    "USE ${DATABASE}; ALTER TABLE same_charset CONVERT TO CHARACTER SET=utf8mb4;"

expect_error \
    "collate default syntax" \
    1064 \
    42000 \
    "near 'DEFAULT'" \
    "USE ${DATABASE}; ALTER TABLE same_charset CONVERT TO CHARACTER SET utf8mb4 COLLATE DEFAULT;"

expect_error \
    "default collate syntax" \
    1064 \
    42000 \
    "near 'DEFAULT COLLATE utf8mb4_bin'" \
    "USE ${DATABASE}; ALTER TABLE same_charset CONVERT TO CHARACTER SET utf8mb4 DEFAULT COLLATE utf8mb4_bin;"
