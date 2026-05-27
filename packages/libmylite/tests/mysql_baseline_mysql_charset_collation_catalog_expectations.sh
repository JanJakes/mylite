#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_mysql_charset_collation_catalog_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names "$@"
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

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

expect_output \
    "catalog counts" \
    "41	286	286" \
    "SELECT (SELECT COUNT(*) FROM INFORMATION_SCHEMA.CHARACTER_SETS),
            (SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLLATIONS),
            (SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY);"

expect_output \
    "catalog hashes" \
    "c7e4bc4bb3ed590bdceccdc5f51725baa3054a44bacaaa9a778d5b2ee2919039	1881d6661f40f230c2ffdf7f6b4baa293cd1fcfe091526a690432e15bf4082a8" \
    "SET SESSION group_concat_max_len = 1000000;
     SELECT
       (SELECT SHA2(GROUP_CONCAT(CONCAT_WS('|', CHARACTER_SET_NAME, DEFAULT_COLLATE_NAME,
                                           DESCRIPTION, MAXLEN)
                                  ORDER BY CHARACTER_SET_NAME SEPARATOR '\n'), 256)
          FROM INFORMATION_SCHEMA.CHARACTER_SETS),
       (SELECT SHA2(GROUP_CONCAT(CONCAT_WS('|', COLLATION_NAME, CHARACTER_SET_NAME, ID,
                                           IS_DEFAULT, IS_COMPILED, SORTLEN, PAD_ATTRIBUTE)
                                  ORDER BY ID, COLLATION_NAME SEPARATOR '\n'), 256)
          FROM INFORMATION_SCHEMA.COLLATIONS);"

character_sets_expected=$(cat <<\EXPECTED
ascii	ascii_general_ci	US ASCII	1
binary	binary	Binary pseudo charset	1
latin1	latin1_swedish_ci	cp1252 West European	1
utf8mb4	utf8mb4_0900_ai_ci	UTF-8 Unicode	4
EXPECTED
)
expect_output \
    "representative character sets" \
    "$character_sets_expected" \
    "SELECT CHARACTER_SET_NAME, DEFAULT_COLLATE_NAME, DESCRIPTION, MAXLEN
       FROM INFORMATION_SCHEMA.CHARACTER_SETS
      WHERE CHARACTER_SET_NAME IN ('ascii','binary','latin1','utf8mb4')
      ORDER BY CHARACTER_SET_NAME;"

collations_expected=$(cat <<\EXPECTED
ascii_general_ci	ascii	11	Yes	Yes	1	PAD SPACE
binary	binary	63	Yes	Yes	1	NO PAD
latin1_swedish_ci	latin1	8	Yes	Yes	1	PAD SPACE
utf8mb4_0900_ai_ci	utf8mb4	255	Yes	Yes	0	NO PAD
utf8mb4_ja_0900_as_cs_ks	utf8mb4	304		Yes	24	NO PAD
EXPECTED
)
expect_output \
    "representative collations" \
    "$collations_expected" \
    "SELECT COLLATION_NAME, CHARACTER_SET_NAME, ID, IS_DEFAULT, IS_COMPILED,
            SORTLEN, PAD_ATTRIBUTE
       FROM INFORMATION_SCHEMA.COLLATIONS
      WHERE COLLATION_NAME IN ('ascii_general_ci','binary','latin1_swedish_ci',
                               'utf8mb4_0900_ai_ci','utf8mb4_ja_0900_as_cs_ks')
      ORDER BY COLLATION_NAME;"

expect_output \
    "applicability count and utf8mb4 subset" \
    "286	89" \
    "SELECT COUNT(*),
            SUM(CASE WHEN CHARACTER_SET_NAME = 'utf8mb4' THEN 1 ELSE 0 END)
       FROM INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY;"

expect_output \
    "show character set case-insensitive like" \
    "utf8mb4	UTF-8 Unicode	utf8mb4_0900_ai_ci	4" \
    "SHOW CHARACTER SET LIKE 'UTF8MB4';"

expect_output \
    "show catalog-only character set" \
    "latin1	cp1252 West European	latin1_swedish_ci	1" \
    "SHOW CHARACTER SET LIKE 'latin1';"

expect_output \
    "show collation case-insensitive like" \
    "utf8mb4_0900_ai_ci	utf8mb4	255	Yes	Yes	0	NO PAD" \
    "SHOW COLLATION LIKE 'UTF8MB4_0900_AI_CI';"

expect_output \
    "show catalog-only collation" \
    "utf8mb4_ja_0900_as_cs_ks	utf8mb4	304		Yes	24	NO PAD" \
    "SHOW COLLATION LIKE 'utf8mb4_ja_0900_as_cs_ks';"

status_output=$(
    run_mysql "SHOW COLLATION LIKE 'utf8mb4_ja_0900_as_cs_ks';
               SELECT @@warning_count, ROW_COUNT();" | tail -n 1
)
if [ "$status_output" != "0	-1" ]; then
    fail "successful show status: expected [0	-1], got [$status_output]"
fi

printf '%s\n' "mysql_baseline_mysql_charset_collation_catalog_expectations: ok"
