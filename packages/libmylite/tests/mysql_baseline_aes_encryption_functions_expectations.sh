#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_aes_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_aes_encryption_functions_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names "$@"
}

run_mysql_typed() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --column-type-info --table "$@"
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

expect_contains() {
    label=$1
    needle=$2
    haystack=$3

    case "$haystack" in
        *"$needle"*) ;;
        *) fail "$label: expected output to contain [$needle], got [$haystack]" ;;
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

expect_output \
    "default block encryption mode" \
    "aes-128-ecb" \
    "SELECT @@block_encryption_mode;"

scalar_expected=$(cat <<EXPECTED
15E36637363712FC2E699B9C95B75393	C717530F41F320757B4AA1BFAF11C42E	72ED6F8AD19A085C32094E16EFC34A08C717530F41F320757B4AA1BFAF11C42E	16	32
EXPECTED
)
expect_output \
    "aes scalar encryption" \
    "$scalar_expected" \
    "SELECT HEX(AES_ENCRYPT('text','key')), HEX(AES_ENCRYPT('','key')), "\
"HEX(AES_ENCRYPT('1234567890123456','key')), "\
"LENGTH(AES_ENCRYPT('text','key')), LENGTH(AES_ENCRYPT('1234567890123456','key'));"

roundtrip_expected=$(cat <<EXPECTED
text	000102	1	1	1	1	1
EXPECTED
)
expect_output \
    "aes decrypt round trips and nulls" \
    "$roundtrip_expected" \
    "SELECT CAST(AES_DECRYPT(AES_ENCRYPT('text','key'),'key') AS CHAR), "\
"HEX(AES_DECRYPT(AES_ENCRYPT(X'000102','key'),'key')), "\
"AES_ENCRYPT(NULL,'key') IS NULL, AES_ENCRYPT('text',NULL) IS NULL, "\
"AES_DECRYPT(NULL,'key') IS NULL, AES_DECRYPT(AES_ENCRYPT('text','key'),NULL) IS NULL, "\
"AES_DECRYPT(X'00','key') IS NULL;"

expect_output \
    "wrong key invalid padding returns null" \
    "1" \
    "SELECT AES_DECRYPT(AES_ENCRYPT('text','key'),'other') IS NULL;"

expect_output \
    "null input long-key warning count" \
    "1	1" \
    "SELECT AES_ENCRYPT(NULL,'12345678901234567890') IS NULL, @@warning_count;"

expect_output \
    "null decrypt input keeps long-key warning count" \
    "1	1" \
    "SELECT AES_DECRYPT(NULL,'12345678901234567890') IS NULL, @@warning_count;"

expect_output \
    "invalid decrypt length keeps long-key warning count" \
    "1	1" \
    "SELECT AES_DECRYPT(X'00','12345678901234567890') IS NULL, @@warning_count;"

warning_expected=$(cat <<EXPECTED
0A51A05D4FA1AC9807BF1783EF411CA6	1
Warning	3237	AES key size should be 16 bytes length or secure KDF methods hkdf or pbkdf2_hmac should be used, please provide exact AES key size or use KDF methods for better security.
EXPECTED
)
expect_output \
    "long key warning" \
    "$warning_expected" \
    "SELECT HEX(AES_ENCRYPT('text','12345678901234567890')), @@warning_count; SHOW WARNINGS;"

cleanup
run_mysql "CREATE DATABASE ${DATABASE} DEFAULT CHARACTER SET utf8mb4 "\
"COLLATE utf8mb4_0900_ai_ci;" >/dev/null

table_expected=$(cat <<EXPECTED
1	15E36637363712FC2E699B9C95B75393	74657874	0
2	D02622E2FA847819D4165A3544B807C5	000102	0
3	NULL	NULL	1
EXPECTED
)
expect_output \
    "aes table-backed projection" \
    "$table_expected" \
"CREATE TABLE aes_probe(id INT PRIMARY KEY, payload VARBINARY(32), key_value VARBINARY(32)); "\
"INSERT INTO aes_probe VALUES (1, 'text', 'key'), (2, X'000102', 'key'), (3, NULL, 'key'); "\
"SELECT id, HEX(AES_ENCRYPT(payload, key_value)), HEX(AES_DECRYPT(AES_ENCRYPT(payload, "\
"key_value), key_value)), AES_DECRYPT(AES_ENCRYPT(payload, key_value), key_value) IS NULL "\
"FROM aes_probe ORDER BY id;" \
    "$DATABASE"

metadata_output=$(run_mysql_typed \
    "SELECT AES_ENCRYPT('text','key') AS enc_value, "\
"AES_DECRYPT(AES_ENCRYPT('text','key'),'key') AS dec_value;")
expect_contains "metadata encryption type" 'Type:       VAR_STRING' "$metadata_output"
expect_contains "metadata encryption collation" 'Collation:  binary (63)' "$metadata_output"
expect_contains "metadata encryption flags" 'Flags:      BINARY' "$metadata_output"

expect_error \
    "aes_encrypt rejects zero arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count in the call to native function 'AES_ENCRYPT'" \
    "SELECT AES_ENCRYPT();"

expect_error \
    "aes_encrypt null input still evaluates key expression" \
    1582 \
    "42000" \
    "Incorrect parameter count in the call to native function 'AES_ENCRYPT'" \
    "SELECT AES_ENCRYPT(NULL, AES_ENCRYPT('x'));"

expect_error \
    "aes_decrypt rejects one argument" \
    1582 \
    "42000" \
    "Incorrect parameter count in the call to native function 'AES_DECRYPT'" \
    "SELECT AES_DECRYPT('x');"

printf '%s\n' "mysql_baseline_aes_encryption_functions_expectations: ok"
