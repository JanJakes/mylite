#!/usr/bin/env sh

set -eu

MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_show_index_where_$$"
TAB=$(printf '\t')

fail() {
    printf '%s\n' "mysql_baseline_show_index_where_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | "$MYSQL_BIN" \
                --protocol=SOCKET \
                --socket="$MYSQL_SOCKET" \
                -uroot \
                --batch \
                --raw \
                --skip-column-names \
                --default-character-set=utf8mb4 \
                "$@"
    else
        printf '%s\n' "$sql" \
            | docker exec -i "$MYSQL_CONTAINER" mysql \
                --protocol=TCP \
                -h127.0.0.1 \
                -uroot \
                --batch \
                --raw \
                --skip-column-names \
                --default-character-set=utf8mb4 \
                "$@"
    fi
}

run_mysql_with_headers() {
    sql=$1
    shift
    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | "$MYSQL_BIN" \
                --protocol=SOCKET \
                --socket="$MYSQL_SOCKET" \
                -uroot \
                --batch \
                --raw \
                --default-character-set=utf8mb4 \
                "$@"
    else
        printf '%s\n' "$sql" \
            | docker exec -i "$MYSQL_CONTAINER" mysql \
                --protocol=TCP \
                -h127.0.0.1 \
                -uroot \
                --batch \
                --raw \
                --default-character-set=utf8mb4 \
                "$@"
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

expect_value() {
    label=$1
    expected=$2
    actual=$3

    if [ "$actual" != "$expected" ]; then
        fail "$label: expected [$expected], got [$actual]"
    fi
}

normalize_tsv() {
    sed "s/${TAB}/|/g"
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
     CREATE TABLE indexed (
         id INT NOT NULL,
         v VARCHAR(20),
         txt TEXT,
         n INT,
         PRIMARY KEY (id),
         KEY k_v (v),
         UNIQUE KEY u_n (n),
         KEY k_prefix (txt(3)),
         FULLTEXT KEY ft_txt (txt)
     ) ENGINE=InnoDB;" >/dev/null

expected_columns="Table${TAB}Non_unique${TAB}Key_name${TAB}Seq_in_index${TAB}Column_name${TAB}Collation${TAB}Cardinality${TAB}Sub_part${TAB}Packed${TAB}Null${TAB}Index_type${TAB}Comment${TAB}Index_comment${TAB}Visible${TAB}Expression"
headers=$(run_mysql_with_headers "SHOW INDEX FROM ${DATABASE}.indexed WHERE Key_name = 'k_v';" | sed -n '1p')
expect_value "headers" "$expected_columns" "$headers"

key_v=$(run_mysql "SHOW INDEX FROM ${DATABASE}.indexed WHERE Key_name = 'k_v';" | normalize_tsv)
expect_value \
    "key name filter" \
    "indexed|1|k_v|1|v|A|0|NULL|NULL|YES|BTREE|||YES|NULL" \
    "$key_v"

primary_unique=$(
    run_mysql \
        "SHOW KEYS FROM ${DATABASE}.indexed WHERE Expression <=> NULL
         AND Key_name IN ('PRIMARY','u_n');" \
        | normalize_tsv
)
expect_value \
    "null-safe and in filter" \
    "indexed|0|PRIMARY|1|id|A|0|NULL|NULL||BTREE|||YES|NULL
indexed|0|u_n|1|n|A|0|NULL|NULL|YES|BTREE|||YES|NULL" \
    "$primary_unique"

like_or=$(
    run_mysql \
        "SHOW INDEX FROM ${DATABASE}.indexed WHERE Key_name LIKE 'k\\_%'
         OR Column_name = 'ID';" \
        | normalize_tsv
)
expect_value \
    "like or case-insensitive filter" \
    "indexed|0|PRIMARY|1|id|A|0|NULL|NULL||BTREE|||YES|NULL
indexed|1|k_v|1|v|A|0|NULL|NULL|YES|BTREE|||YES|NULL
indexed|1|k_prefix|1|txt|A|0|3|NULL|YES|BTREE|||YES|NULL" \
    "$like_or"

prefix=$(run_mysql "SHOW INDEX FROM ${DATABASE}.indexed WHERE Sub_part <=> '3';" | normalize_tsv)
expect_value \
    "prefix sub-part filter" \
    "indexed|1|k_prefix|1|txt|A|0|3|NULL|YES|BTREE|||YES|NULL" \
    "$prefix"

backticked_numeric=$(
    run_mysql \
        "SHOW INDEX FROM ${DATABASE}.indexed WHERE \`Column_name\` IN ('id','v')
         AND Non_unique = '1';" \
        | normalize_tsv
)
expect_value \
    "backticked column and numeric string filter" \
    "indexed|1|k_v|1|v|A|0|NULL|NULL|YES|BTREE|||YES|NULL" \
    "$backticked_numeric"

fulltext=$(
    run_mysql \
        "SHOW INDEX FROM ${DATABASE}.indexed WHERE NOT (Visible = 'NO')
         AND Index_type = 'FULLTEXT';" \
        | normalize_tsv
)
expect_value \
    "not and fulltext filter" \
    "indexed|1|ft_txt|1|txt|NULL|0|NULL|NULL|YES|FULLTEXT|||YES|NULL" \
    "$fulltext"

sub_part_in=$(
    run_mysql "SHOW INDEX FROM ${DATABASE}.indexed WHERE Sub_part IN (NULL, '3');" \
        | normalize_tsv
)
expect_value \
    "sub part in with null" \
    "indexed|1|k_prefix|1|txt|A|0|3|NULL|YES|BTREE|||YES|NULL" \
    "$sub_part_in"

status=$(
    run_mysql \
        "SHOW INDEX FROM ${DATABASE}.indexed WHERE Cardinality >= '0' AND Seq_in_index = '1';
         SELECT @@warning_count, @@error_count, ROW_COUNT();" \
        | tail -n 1 \
        | normalize_tsv
)
expect_value "status after successful where" "0|0|-1" "$status"

no_match_status=$(
    run_mysql \
        "USE ${DATABASE}; SHOW INDEX FROM indexed WHERE Key_name = 'missing';
         SELECT @@warning_count, @@error_count, ROW_COUNT();" \
        | tail -n 1 \
        | normalize_tsv
)
expect_value "status after empty where" "0|0|-1" "$no_match_status"

numeric_key_name_status=$(
    run_mysql \
        "SHOW INDEX FROM ${DATABASE}.indexed WHERE Key_name = 1;
         SELECT @@warning_count, @@error_count, ROW_COUNT();" \
        | tail -n 1 \
        | normalize_tsv
)
expect_value "mysql numeric key-name comparison warnings" "6|0|-1" "$numeric_key_name_status"

regexp_rows=$(
    run_mysql "SHOW INDEX FROM ${DATABASE}.indexed WHERE Key_name REGEXP 'k.*';" \
        | normalize_tsv
)
expect_value \
    "mysql regexp accepted upstream" \
    "indexed|1|k_v|1|v|A|0|NULL|NULL|YES|BTREE|||YES|NULL
indexed|1|k_prefix|1|txt|A|0|3|NULL|YES|BTREE|||YES|NULL" \
    "$regexp_rows"

expect_error \
    "unknown where column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'where clause'" \
    "SHOW INDEX FROM ${DATABASE}.indexed WHERE missing = 'x';"

expect_error \
    "qualified where column" \
    1054 \
    42S22 \
    "Unknown column 'indexes.Key_name' in 'where clause'" \
    "SHOW INDEX FROM ${DATABASE}.indexed WHERE indexes.Key_name = 'k_v';"

expect_error \
    "order by after where" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW INDEX FROM ${DATABASE}.indexed WHERE Key_name = 'k_v' ORDER BY Key_name;"

expect_error \
    "like clause" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW INDEX FROM ${DATABASE}.indexed LIKE 'k%';"

printf '%s\n' "baseline-show-index-where MySQL 8.4.9 expectations verified"
