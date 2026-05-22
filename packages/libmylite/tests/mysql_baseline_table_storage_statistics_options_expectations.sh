#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_table_storage_stats_options_$$"

fail() {
    printf '%s\n' "mysql_baseline_table_storage_statistics_options_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
}

expect_value() {
    label=$1
    expected=$2
    actual=$3

    if [ "$actual" != "$expected" ]; then
        fail "$label: expected [$expected], got [$actual]"
    fi
}

expect_contains() {
    label=$1
    haystack=$2
    needle=$3

    case "$haystack" in
        *"$needle"*) ;;
        *) fail "$label: expected output to contain [$needle], got [$haystack]" ;;
    esac
}

expect_not_contains() {
    label=$1
    haystack=$2
    needle=$3

    case "$haystack" in
        *"$needle"*) fail "$label: expected output not to contain [$needle], got [$haystack]" ;;
        *) ;;
    esac
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

run_mysql \
    "CREATE DATABASE ${DATABASE};
     USE ${DATABASE};
     CREATE TABLE rf_dynamic(id INT) ENGINE=InnoDB ROW_FORMAT=DYNAMIC;
     CREATE TABLE rf_compact(id INT) ENGINE=InnoDB ROW_FORMAT=COMPACT;
     CREATE TABLE rf_redundant(id INT) ENGINE=InnoDB ROW_FORMAT=REDUNDANT;
     CREATE TABLE rf_compressed(id INT) ENGINE=InnoDB ROW_FORMAT=COMPRESSED;
     CREATE TABLE rf_default(id INT) ENGINE=InnoDB ROW_FORMAT=DEFAULT;
     CREATE TABLE kbs(id INT) ENGINE=InnoDB KEY_BLOCK_SIZE=8;
     CREATE TABLE pack_checksum(id INT) ENGINE=InnoDB PACK_KEYS=0 CHECKSUM=2;
     CREATE TABLE stats(id INT) ENGINE=InnoDB
         STATS_PERSISTENT=1 STATS_AUTO_RECALC=0 STATS_SAMPLE_PAGES=7;
     CREATE TABLE defaults(id INT) ENGINE=InnoDB
         PACK_KEYS=DEFAULT STATS_PERSISTENT=DEFAULT STATS_AUTO_RECALC=DEFAULT
         STATS_SAMPLE_PAGES=DEFAULT CHECKSUM=0 KEY_BLOCK_SIZE=0 ROW_FORMAT=DEFAULT;
     CREATE TABLE duplicates(id INT) ENGINE=InnoDB
         ROW_FORMAT=COMPACT ROW_FORMAT=DYNAMIC
         STATS_PERSISTENT=0 STATS_PERSISTENT=1;
     CREATE TABLE comma_options(id INT) ENGINE=InnoDB, ROW_FORMAT=REDUNDANT, PACK_KEYS=1;
     CREATE TABLE source_like(id INT) ENGINE=InnoDB
         PACK_KEYS=1 STATS_PERSISTENT=1 CHECKSUM=1 ROW_FORMAT=COMPACT;
     CREATE TABLE clone_like LIKE source_like;" >/dev/null

show_dynamic=$(run_mysql "SHOW CREATE TABLE ${DATABASE}.rf_dynamic;")
expect_contains "dynamic SHOW CREATE" "$show_dynamic" "ROW_FORMAT=DYNAMIC"

show_compact=$(run_mysql "SHOW CREATE TABLE ${DATABASE}.rf_compact;")
expect_contains "compact SHOW CREATE" "$show_compact" "ROW_FORMAT=COMPACT"

show_redundant=$(run_mysql "SHOW CREATE TABLE ${DATABASE}.rf_redundant;")
expect_contains "redundant SHOW CREATE" "$show_redundant" "ROW_FORMAT=REDUNDANT"

show_compressed=$(run_mysql "SHOW CREATE TABLE ${DATABASE}.rf_compressed;")
expect_contains "compressed SHOW CREATE" "$show_compressed" "ROW_FORMAT=COMPRESSED"

show_default=$(run_mysql "SHOW CREATE TABLE ${DATABASE}.rf_default;")
expect_not_contains "default ROW_FORMAT omitted" "$show_default" "ROW_FORMAT="

show_kbs=$(run_mysql "SHOW CREATE TABLE ${DATABASE}.kbs;")
expect_contains "KEY_BLOCK_SIZE SHOW CREATE" "$show_kbs" "KEY_BLOCK_SIZE=8"
expect_not_contains "KEY_BLOCK_SIZE does not add ROW_FORMAT" "$show_kbs" "ROW_FORMAT="

show_pack_checksum=$(run_mysql "SHOW CREATE TABLE ${DATABASE}.pack_checksum;")
expect_contains "PACK_KEYS SHOW CREATE" "$show_pack_checksum" "PACK_KEYS=0"
expect_contains "CHECKSUM nonzero normalizes" "$show_pack_checksum" "CHECKSUM=1"

show_stats=$(run_mysql "SHOW CREATE TABLE ${DATABASE}.stats;")
expect_contains "stats SHOW CREATE order" \
    "$show_stats" "STATS_PERSISTENT=1 STATS_AUTO_RECALC=0 STATS_SAMPLE_PAGES=7"

show_defaults=$(run_mysql "SHOW CREATE TABLE ${DATABASE}.defaults;")
expect_not_contains "defaults omit PACK_KEYS" "$show_defaults" "PACK_KEYS="
expect_not_contains "defaults omit STATS" "$show_defaults" "STATS_"
expect_not_contains "defaults omit CHECKSUM" "$show_defaults" "CHECKSUM="
expect_not_contains "defaults omit KEY_BLOCK_SIZE" "$show_defaults" "KEY_BLOCK_SIZE="
expect_not_contains "defaults omit ROW_FORMAT" "$show_defaults" "ROW_FORMAT="

show_duplicates=$(run_mysql "SHOW CREATE TABLE ${DATABASE}.duplicates;")
expect_contains "duplicate options last wins row format" "$show_duplicates" "ROW_FORMAT=DYNAMIC"
expect_contains "duplicate options last wins stats" "$show_duplicates" "STATS_PERSISTENT=1"
expect_not_contains "duplicate first stats omitted" "$show_duplicates" "STATS_PERSISTENT=0"

show_comma=$(run_mysql "SHOW CREATE TABLE ${DATABASE}.comma_options;")
expect_contains "comma options row format" "$show_comma" "ROW_FORMAT=REDUNDANT"
expect_contains "comma options pack keys" "$show_comma" "PACK_KEYS=1"

show_clone=$(run_mysql "SHOW CREATE TABLE ${DATABASE}.clone_like;")
expect_contains "LIKE clone pack keys" "$show_clone" "PACK_KEYS=1"
expect_contains "LIKE clone stats" "$show_clone" "STATS_PERSISTENT=1"
expect_contains "LIKE clone checksum" "$show_clone" "CHECKSUM=1"
expect_contains "LIKE clone row format" "$show_clone" "ROW_FORMAT=COMPACT"

metadata=$(
    run_mysql \
        "SELECT TABLE_NAME, ROW_FORMAT, CHECKSUM, CREATE_OPTIONS
         FROM INFORMATION_SCHEMA.TABLES
         WHERE TABLE_SCHEMA = '${DATABASE}'
           AND TABLE_NAME IN
               ('rf_dynamic','rf_compact','rf_redundant','rf_compressed','rf_default',
                'kbs','pack_checksum','stats','defaults','duplicates','comma_options','clone_like')
         ORDER BY TABLE_NAME;"
)
expected_metadata=$(
    printf '%s\t%s\t%s\t%s\n' "clone_like" "Compact" "NULL" \
        "row_format=COMPACT stats_persistent=1 pack_keys=1 checksum=1"
    printf '%s\t%s\t%s\t%s\n' "comma_options" "Redundant" "NULL" \
        "row_format=REDUNDANT pack_keys=1"
    printf '%s\t%s\t%s\t%s\n' "defaults" "Dynamic" "NULL" ""
    printf '%s\t%s\t%s\t%s\n' "duplicates" "Dynamic" "NULL" \
        "row_format=DYNAMIC stats_persistent=1"
    printf '%s\t%s\t%s\t%s\n' "kbs" "Compressed" "NULL" "KEY_BLOCK_SIZE=8"
    printf '%s\t%s\t%s\t%s\n' "pack_checksum" "Dynamic" "NULL" "pack_keys=0 checksum=1"
    printf '%s\t%s\t%s\t%s\n' "rf_compact" "Compact" "NULL" "row_format=COMPACT"
    printf '%s\t%s\t%s\t%s\n' "rf_compressed" "Compressed" "NULL" "row_format=COMPRESSED"
    printf '%s\t%s\t%s\t%s\n' "rf_default" "Dynamic" "NULL" ""
    printf '%s\t%s\t%s\t%s\n' "rf_dynamic" "Dynamic" "NULL" "row_format=DYNAMIC"
    printf '%s\t%s\t%s\t%s\n' "rf_redundant" "Redundant" "NULL" "row_format=REDUNDANT"
    printf '%s\t%s\t%s\t%s\n' "stats" "Dynamic" "NULL" \
        "stats_sample_pages=7 stats_auto_recalc=0 stats_persistent=1"
)
expect_value "information_schema option metadata" "$expected_metadata" "$metadata"

status=$(run_mysql "SHOW TABLE STATUS FROM ${DATABASE} WHERE Name IN ('kbs','stats','rf_compact');")
expect_contains "SHOW TABLE STATUS compressed row format" "$status" "kbs	InnoDB	10	Compressed"
expect_contains "SHOW TABLE STATUS stats options" \
    "$status" "stats_sample_pages=7 stats_auto_recalc=0 stats_persistent=1"
expect_contains "SHOW TABLE STATUS compact row format" "$status" "rf_compact	InnoDB	10	Compact"

success=$(run_mysql "USE ${DATABASE}; CREATE TABLE status_ok(id INT) ROW_FORMAT=DYNAMIC; SELECT ROW_COUNT(), @@warning_count;")
expect_value "successful status" "0	0" "$success"

expect_error \
    "ROW_FORMAT fixed rejected" \
    1031 \
    HY000 \
    "Table storage engine for 'bad_fixed' doesn't have this option" \
    "USE ${DATABASE}; CREATE TABLE bad_fixed(id INT) ENGINE=InnoDB ROW_FORMAT=FIXED;"

expect_error \
    "KEY_BLOCK_SIZE invalid rejected" \
    1031 \
    HY000 \
    "Table storage engine for 'bad_key_block' doesn't have this option" \
    "USE ${DATABASE}; CREATE TABLE bad_key_block(id INT) ENGINE=InnoDB KEY_BLOCK_SIZE=7;"

expect_error \
    "KEY_BLOCK_SIZE dynamic conflict rejected" \
    1031 \
    HY000 \
    "Table storage engine for 'bad_conflict' doesn't have this option" \
    "USE ${DATABASE}; CREATE TABLE bad_conflict(id INT) ENGINE=InnoDB KEY_BLOCK_SIZE=8 ROW_FORMAT=DYNAMIC;"

expect_error \
    "PACK_KEYS invalid syntax" \
    1064 \
    42000 \
    "near '2'" \
    "USE ${DATABASE}; CREATE TABLE bad_pack(id INT) ENGINE=InnoDB PACK_KEYS=2;"

expect_error \
    "STATS_SAMPLE_PAGES zero rejected" \
    1064 \
    42000 \
    "valid range for stats_sample_pages" \
    "USE ${DATABASE}; CREATE TABLE bad_pages(id INT) ENGINE=InnoDB STATS_SAMPLE_PAGES=0;"

expect_error \
    "ROW_FORMAT string syntax" \
    1064 \
    42000 \
    "near ''DYNAMIC''" \
    "USE ${DATABASE}; CREATE TABLE bad_row_string(id INT) ENGINE=InnoDB ROW_FORMAT='DYNAMIC';"

printf '%s\n' "mysql_baseline_table_storage_statistics_options_expectations: ok"
