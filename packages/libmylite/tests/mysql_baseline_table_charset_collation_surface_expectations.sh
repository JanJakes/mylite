#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_table_charset_collation_$$"

fail() {
    printf '%s\n' "mysql_baseline_table_charset_collation_surface_expectations: $1" >&2
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
     CREATE TABLE default_charset(id INT) DEFAULT CHARSET=utf8mb4;
     CREATE TABLE default_character_set(id INT) DEFAULT CHARACTER SET utf8mb4;
     CREATE TABLE character_set_equal(id INT) CHARACTER SET=utf8mb4;
     CREATE TABLE charset_no_default(id INT) CHARSET utf8mb4;
     CREATE TABLE collate_only(id INT) COLLATE=utf8mb4_0900_ai_ci;
     CREATE TABLE default_collate(id INT) DEFAULT COLLATE utf8mb4_0900_ai_ci;
     CREATE TABLE string_names(id INT) DEFAULT CHARSET='utf8mb4' COLLATE=\"utf8mb4_0900_ai_ci\";
     CREATE TABLE quoted_names(id INT) DEFAULT CHARSET=\`utf8mb4\` COLLATE=\`utf8mb4_0900_ai_ci\`;
     CREATE TABLE uppercase_names(id INT) DEFAULT CHARSET=UTF8MB4 COLLATE=UTF8MB4_0900_AI_CI;
     CREATE TABLE engine_charset(id INT) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;
     CREATE TABLE reverse_order(id INT) COLLATE=utf8mb4_0900_ai_ci DEFAULT CHARSET=utf8mb4 ENGINE=InnoDB;" >/dev/null

expected_default_suffix="CREATE TABLE \`default_charset\` (
  \`id\` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci"

expected_default_character_set="CREATE TABLE \`default_character_set\` (
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

expected_engine_charset="CREATE TABLE \`engine_charset\` (
  \`id\` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci"

expected_reverse_order="CREATE TABLE \`reverse_order\` (
  \`id\` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci"

expect_show_create "default charset" "default_charset" "$expected_default_suffix"
expect_show_create "default character set" "default_character_set" "$expected_default_character_set"
expect_show_create "character set equal" "character_set_equal" "$expected_character_set_equal"
expect_show_create "charset no default" "charset_no_default" "$expected_charset_no_default"
expect_show_create "collate only" "collate_only" "$expected_collate_only"
expect_show_create "default collate" "default_collate" "$expected_default_collate"
expect_show_create "string names" "string_names" "$expected_string_names"
expect_show_create "quoted names" "quoted_names" "$expected_quoted_names"
expect_show_create "uppercase names" "uppercase_names" "$expected_uppercase_names"
expect_show_create "engine charset" "engine_charset" "$expected_engine_charset"
expect_show_create "reverse order" "reverse_order" "$expected_reverse_order"

create_status=$(run_mysql "USE ${DATABASE}; CREATE TABLE status_table(id INT) DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci; SELECT @@warning_count, ROW_COUNT();")
expect_value "create default charset/collation status" "0	0" "$(printf '%s\n' "$create_status" | tail -n 1)"

duplicate_same=$(run_mysql "USE ${DATABASE}; CREATE TABLE duplicate_same(id INT) DEFAULT CHARSET=utf8mb4 CHARSET=utf8mb4; SHOW CREATE TABLE duplicate_same;" | tail -n 1)
case "$duplicate_same" in
    *"DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci"*) ;;
    *) fail "duplicate same charset: unexpected SHOW CREATE TABLE row [$duplicate_same]" ;;
esac

expect_error \
    "unknown charset" \
    1115 \
    42000 \
    "Unknown character set: 'nosuch_charset'" \
    "CREATE DATABASE IF NOT EXISTS ${DATABASE}; USE ${DATABASE}; CREATE TABLE unknown_charset(id INT) DEFAULT CHARSET=nosuch_charset;"

expect_error \
    "unknown collation" \
    1273 \
    HY000 \
    "Unknown collation: 'nosuch_collation'" \
    "CREATE DATABASE IF NOT EXISTS ${DATABASE}; USE ${DATABASE}; CREATE TABLE unknown_collation(id INT) COLLATE=nosuch_collation;"

expect_error \
    "mismatched collation" \
    1253 \
    42000 \
    "COLLATION 'latin1_swedish_ci' is not valid for CHARACTER SET 'utf8mb4'" \
    "CREATE DATABASE IF NOT EXISTS ${DATABASE}; USE ${DATABASE}; CREATE TABLE mismatched_collation(id INT) DEFAULT CHARSET=utf8mb4 COLLATE=latin1_swedish_ci;"

expect_error \
    "charset default syntax" \
    1064 \
    42000 \
    "near 'DEFAULT'" \
    "CREATE DATABASE IF NOT EXISTS ${DATABASE}; USE ${DATABASE}; CREATE TABLE charset_default(id INT) DEFAULT CHARSET=DEFAULT;"

expect_error \
    "collate default syntax" \
    1064 \
    42000 \
    "near 'DEFAULT'" \
    "CREATE DATABASE IF NOT EXISTS ${DATABASE}; USE ${DATABASE}; CREATE TABLE collate_default(id INT) COLLATE=DEFAULT;"

expect_error \
    "conflicting charset" \
    1302 \
    HY000 \
    "Conflicting declarations: 'CHARACTER SET utf8mb4' and 'CHARACTER SET latin1'" \
    "CREATE DATABASE IF NOT EXISTS ${DATABASE}; USE ${DATABASE}; CREATE TABLE conflicting_charset(id INT) DEFAULT CHARSET=utf8mb4 CHARSET=latin1;"
