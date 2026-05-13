#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_utf8mb4_legacy_collations_$$"

fail() {
    printf '%s\n' "mysql_baseline_utf8mb4_legacy_collations_expectations: $1" >&2
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
     CREATE TABLE c0900(id INT) DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;
     CREATE TABLE cgen(id INT) DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;
     CREATE TABLE cbin(id INT) DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;
     CREATE TABLE cuni(id INT) DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
     CREATE TABLE c520(id INT) DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_520_ci;
     CREATE TABLE collate_only(id INT) COLLATE=utf8mb4_unicode_ci;
     CREATE TABLE only_charset(id INT) DEFAULT CHARSET=utf8mb4;
     CREATE TABLE repeat_collate(id INT) COLLATE=utf8mb4_unicode_ci COLLATE=utf8mb4_general_ci;
     CREATE TABLE quoted_names(id INT) DEFAULT CHARSET='utf8mb4' COLLATE=\`utf8mb4_unicode_520_ci\`;
     CREATE TABLE uppercase_names(id INT) DEFAULT CHARSET=UTF8MB4 COLLATE=UTF8MB4_UNICODE_CI;" >/dev/null

expect_show_create \
    "legacy general" \
    "cgen" \
    "CREATE TABLE \`cgen\` (
  \`id\` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci"

expect_show_create \
    "legacy bin" \
    "cbin" \
    "CREATE TABLE \`cbin\` (
  \`id\` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin"

expect_show_create \
    "legacy unicode" \
    "cuni" \
    "CREATE TABLE \`cuni\` (
  \`id\` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci"

expect_show_create \
    "legacy unicode 520" \
    "c520" \
    "CREATE TABLE \`c520\` (
  \`id\` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_520_ci"

expect_show_create \
    "collate only" \
    "collate_only" \
    "CREATE TABLE \`collate_only\` (
  \`id\` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci"

expect_show_create \
    "charset default collation" \
    "only_charset" \
    "CREATE TABLE \`only_charset\` (
  \`id\` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci"

expect_show_create \
    "repeated collation last wins" \
    "repeat_collate" \
    "CREATE TABLE \`repeat_collate\` (
  \`id\` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci"

expect_show_create \
    "quoted names" \
    "quoted_names" \
    "CREATE TABLE \`quoted_names\` (
  \`id\` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_520_ci"

expect_show_create \
    "uppercase names" \
    "uppercase_names" \
    "CREATE TABLE \`uppercase_names\` (
  \`id\` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci"

run_mysql "USE ${DATABASE}; CREATE TABLE clone LIKE cuni;" >/dev/null
expect_show_create \
    "create like collation clone" \
    "clone" \
    "CREATE TABLE \`clone\` (
  \`id\` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci"

alter_status=$(run_mysql "USE ${DATABASE}; ALTER TABLE cuni DEFAULT COLLATE utf8mb4_general_ci; SELECT ROW_COUNT(), @@warning_count;")
expect_value "alter status" "0	0" "$alter_status"
expect_show_create \
    "altered collation" \
    "cuni" \
    "CREATE TABLE \`cuni\` (
  \`id\` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci"

table_collations=$(run_mysql "SELECT TABLE_NAME, TABLE_COLLATION FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA='${DATABASE}' AND TABLE_NAME IN ('c0900','cgen','cbin','c520','cuni','clone') ORDER BY TABLE_NAME;")
expect_value \
    "information schema table collations" \
    "c0900	utf8mb4_0900_ai_ci
c520	utf8mb4_unicode_520_ci
cbin	utf8mb4_bin
cgen	utf8mb4_general_ci
clone	utf8mb4_unicode_ci
cuni	utf8mb4_general_ci" \
    "$table_collations"

collation_headers="Collation	Charset	Id	Default	Compiled	Sortlen	Pad_attribute"
expect_value \
    "show collation general" \
    "${collation_headers}
utf8mb4_general_ci	utf8mb4	45		Yes	1	PAD SPACE" \
    "$(run_mysql_with_headers "SHOW COLLATION LIKE 'utf8mb4_general_ci';")"
expect_value \
    "show collation bin" \
    "${collation_headers}
utf8mb4_bin	utf8mb4	46		Yes	1	PAD SPACE" \
    "$(run_mysql_with_headers "SHOW COLLATION LIKE 'utf8mb4_bin';")"
expect_value \
    "show collation unicode" \
    "${collation_headers}
utf8mb4_unicode_ci	utf8mb4	224		Yes	8	PAD SPACE" \
    "$(run_mysql_with_headers "SHOW COLLATION LIKE 'utf8mb4_unicode_ci';")"
expect_value \
    "show collation unicode 520" \
    "${collation_headers}
utf8mb4_unicode_520_ci	utf8mb4	246		Yes	8	PAD SPACE" \
    "$(run_mysql_with_headers "SHOW COLLATION LIKE 'utf8mb4_unicode_520_ci';")"

information_schema_collations=$(run_mysql "SELECT COLLATION_NAME, CHARACTER_SET_NAME, ID, IS_DEFAULT, IS_COMPILED, SORTLEN, PAD_ATTRIBUTE FROM INFORMATION_SCHEMA.COLLATIONS WHERE COLLATION_NAME IN ('utf8mb4_general_ci','utf8mb4_bin','utf8mb4_unicode_ci','utf8mb4_unicode_520_ci','utf8mb4_0900_ai_ci') ORDER BY ID;")
expect_value \
    "information schema collations" \
    "utf8mb4_general_ci	utf8mb4	45		Yes	1	PAD SPACE
utf8mb4_bin	utf8mb4	46		Yes	1	PAD SPACE
utf8mb4_unicode_ci	utf8mb4	224		Yes	8	PAD SPACE
utf8mb4_unicode_520_ci	utf8mb4	246		Yes	8	PAD SPACE
utf8mb4_0900_ai_ci	utf8mb4	255	Yes	Yes	0	NO PAD" \
    "$information_schema_collations"

set_names_status=$(run_mysql "SET NAMES utf8mb4 COLLATE utf8mb4_unicode_ci; SELECT @@character_set_client, @@character_set_connection, @@character_set_results, @@collation_connection, ROW_COUNT(), @@warning_count;")
expect_value "set names unicode collation" "utf8mb4	utf8mb4	utf8mb4	utf8mb4_unicode_ci	0	0" "$set_names_status"

set_names_bin=$(run_mysql "SET NAMES utf8mb4 COLLATE utf8mb4_bin; SELECT @@collation_connection;")
expect_value "set names bin collation" "utf8mb4_bin" "$set_names_bin"

expect_error \
    "unknown charset" \
    1115 \
    42000 \
    "Unknown character set: 'bogus'" \
    "CREATE DATABASE IF NOT EXISTS ${DATABASE}; USE ${DATABASE}; CREATE TABLE unknown_charset(id INT) DEFAULT CHARSET=bogus COLLATE=utf8mb4_unicode_ci;"

expect_error \
    "unknown collation" \
    1273 \
    HY000 \
    "Unknown collation: 'utf8mb4_bogus_ci'" \
    "CREATE DATABASE IF NOT EXISTS ${DATABASE}; USE ${DATABASE}; CREATE TABLE unknown_collation(id INT) COLLATE=utf8mb4_bogus_ci;"

expect_error \
    "mismatched collation" \
    1253 \
    42000 \
    "COLLATION 'utf8mb4_unicode_ci' is not valid for CHARACTER SET 'latin1'" \
    "CREATE DATABASE IF NOT EXISTS ${DATABASE}; USE ${DATABASE}; CREATE TABLE mismatched(id INT) DEFAULT CHARSET=latin1 COLLATE=utf8mb4_unicode_ci;"

expect_error \
    "set names unknown collation" \
    1273 \
    HY000 \
    "Unknown collation: 'utf8mb4_bogus_ci'" \
    "SET NAMES utf8mb4 COLLATE utf8mb4_bogus_ci;"

printf '%s\n' "mysql_baseline_utf8mb4_legacy_collations_expectations: ok"
