#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_alter_table_charset_collation_$$"

fail() {
    printf '%s\n' "mysql_baseline_alter_table_default_charset_collation_expectations: $1" >&2
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
    show_label=$1
    table=$2
    expected_create=$3

    output=$(run_mysql_with_headers "USE ${DATABASE}; SHOW CREATE TABLE ${table};")
    headers=$(printf '%s\n' "$output" | sed -n '1p')
    table_name=$(printf '%s\n' "$output" | sed -n '2p' | cut -f 1)
    create_text=$(printf '%s\n' "$output" | sed -n '2,$p' | cut -f 2-)

    expect_value "$show_label headers" "Table	Create Table" "$headers"
    expect_value "$show_label table" "$table" "$table_name"
    expect_value "$show_label create" "$expected_create" "$create_text"
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

run_mysql \
    "CREATE DATABASE ${DATABASE};
     USE ${DATABASE};
     CREATE TABLE default_charset(id INT);
     CREATE TABLE default_character_set(id INT);
     CREATE TABLE default_character_set_equal(id INT);
     CREATE TABLE character_set_equal(id INT);
     CREATE TABLE charset_no_default(id INT);
     CREATE TABLE collate_only(id INT);
     CREATE TABLE default_collate(id INT);
     CREATE TABLE string_names(id INT);
     CREATE TABLE quoted_names(id INT);
     CREATE TABLE uppercase_names(id INT);
     CREATE TABLE duplicate_same(id INT);
     CREATE TABLE qualified_target(id INT);
     INSERT INTO default_charset VALUES (1), (2);
     ALTER TABLE default_charset DEFAULT CHARSET=utf8mb4;
     ALTER TABLE default_character_set DEFAULT CHARACTER SET utf8mb4;
     ALTER TABLE default_character_set_equal DEFAULT CHARACTER SET=utf8mb4;
     ALTER TABLE character_set_equal CHARACTER SET=utf8mb4;
     ALTER TABLE charset_no_default CHARSET utf8mb4;
     ALTER TABLE collate_only COLLATE=utf8mb4_0900_ai_ci;
     ALTER TABLE default_collate DEFAULT COLLATE utf8mb4_0900_ai_ci;
     ALTER TABLE string_names DEFAULT CHARSET='utf8mb4' COLLATE=\"utf8mb4_0900_ai_ci\";
     ALTER TABLE quoted_names DEFAULT CHARSET=\`utf8mb4\` COLLATE=\`utf8mb4_0900_ai_ci\`;
     ALTER TABLE uppercase_names DEFAULT CHARSET=UTF8MB4 COLLATE=UTF8MB4_0900_AI_CI;
     ALTER TABLE duplicate_same DEFAULT CHARSET=utf8mb4 CHARSET=utf8mb4;" >/dev/null

qualified_status=$(
    run_mysql \
        "ALTER TABLE ${DATABASE}.qualified_target DEFAULT CHARSET=utf8mb4;
         SELECT @@warning_count, @@error_count, ROW_COUNT();"
)
expect_value "schema-qualified alter status" "0	0	0" "$(printf '%s\n' "$qualified_status" | tail -n 1)"

expected_default_charset="CREATE TABLE \`default_charset\` (
  \`id\` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci"

expected_default_character_set="CREATE TABLE \`default_character_set\` (
  \`id\` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci"

expected_default_character_set_equal="CREATE TABLE \`default_character_set_equal\` (
  \`id\` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci"

expected_character_set_equal="CREATE TABLE \`character_set_equal\` (
  \`id\` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci"

expected_charset_no_default="CREATE TABLE \`charset_no_default\` (
  \`id\` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci"

expected_collate_only="CREATE TABLE \`collate_only\` (
  \`id\` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci"

expected_default_collate="CREATE TABLE \`default_collate\` (
  \`id\` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci"

expected_string_names="CREATE TABLE \`string_names\` (
  \`id\` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci"

expected_quoted_names="CREATE TABLE \`quoted_names\` (
  \`id\` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci"

expected_uppercase_names="CREATE TABLE \`uppercase_names\` (
  \`id\` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci"

expected_duplicate_same="CREATE TABLE \`duplicate_same\` (
  \`id\` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci"

expect_show_create "default charset" "default_charset" "$expected_default_charset"
expect_show_create "default character set" "default_character_set" "$expected_default_character_set"
expect_show_create \
    "default character set equal" \
    "default_character_set_equal" \
    "$expected_default_character_set_equal"
expect_show_create "character set equal" "character_set_equal" "$expected_character_set_equal"
expect_show_create "charset no default" "charset_no_default" "$expected_charset_no_default"
expect_show_create "collate only" "collate_only" "$expected_collate_only"
expect_show_create "default collate" "default_collate" "$expected_default_collate"
expect_show_create "string names" "string_names" "$expected_string_names"
expect_show_create "quoted names" "quoted_names" "$expected_quoted_names"
expect_show_create "uppercase names" "uppercase_names" "$expected_uppercase_names"
expect_show_create "duplicate same" "duplicate_same" "$expected_duplicate_same"

alter_status=$(run_mysql "USE ${DATABASE}; ALTER TABLE default_charset DEFAULT CHARSET=utf8mb4; SELECT @@warning_count, @@error_count, ROW_COUNT();")
expect_value "alter default charset status" "0	0	0" "$(printf '%s\n' "$alter_status" | tail -n 1)"

row_values=$(run_mysql "USE ${DATABASE}; SELECT id FROM default_charset ORDER BY id;")
expect_value "alter preserves rows" "1
2" "$row_values"

expect_error \
    "missing default database" \
    1046 \
    3D000 \
    "No database selected" \
    "ALTER TABLE default_charset DEFAULT CHARSET=utf8mb4;"

expect_error \
    "unknown explicit schema" \
    1049 \
    42000 \
    "Unknown database 'nosuch_schema_${DATABASE}'" \
    "ALTER TABLE nosuch_schema_${DATABASE}.default_charset DEFAULT CHARSET=utf8mb4;"

expect_error \
    "unknown table" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing' doesn't exist" \
    "USE ${DATABASE}; ALTER TABLE missing DEFAULT CHARSET=utf8mb4;"

expect_error \
    "charset default syntax" \
    1064 \
    42000 \
    "near 'DEFAULT'" \
    "USE ${DATABASE}; ALTER TABLE default_charset DEFAULT CHARSET=DEFAULT;"

expect_error \
    "character set default syntax" \
    1064 \
    42000 \
    "near 'DEFAULT'" \
    "USE ${DATABASE}; ALTER TABLE default_charset CHARACTER SET DEFAULT;"

expect_error \
    "collate default syntax" \
    1064 \
    42000 \
    "near 'DEFAULT'" \
    "USE ${DATABASE}; ALTER TABLE default_charset COLLATE=DEFAULT;"

expect_error \
    "unknown charset" \
    1115 \
    42000 \
    "Unknown character set: 'nosuch_charset'" \
    "USE ${DATABASE}; ALTER TABLE default_charset DEFAULT CHARSET=nosuch_charset;"

expect_error \
    "unknown collation" \
    1273 \
    HY000 \
    "Unknown collation: 'nosuch_collation'" \
    "USE ${DATABASE}; ALTER TABLE default_charset COLLATE=nosuch_collation;"

expect_error \
    "mismatched collation" \
    1253 \
    42000 \
    "COLLATION 'latin1_swedish_ci' is not valid for CHARACTER SET 'utf8mb4'" \
    "USE ${DATABASE}; ALTER TABLE default_charset DEFAULT CHARSET=utf8mb4 COLLATE=latin1_swedish_ci;"

expect_error \
    "conflicting charset" \
    1302 \
    HY000 \
    "Conflicting declarations: 'CHARACTER SET utf8mb4' and 'CHARACTER SET latin1'" \
    "USE ${DATABASE}; ALTER TABLE default_charset DEFAULT CHARSET=utf8mb4 CHARSET=latin1;"
