#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_digest_functions_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_digest_functions_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --binary-as-hex=1 --skip-column-names \
            --default-character-set=utf8mb4 "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --binary-as-hex=1 --default-character-set=utf8mb4 "$@"
}

run_mysql_type_info() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --default-character-set=utf8mb4 --column-type-info -vvv "$@"
}

expect_value() {
    label=$1
    expected=$2
    actual=$3

    if [ "$actual" != "$expected" ]; then
        fail "$label: expected [$expected], got [$actual]"
    fi
}

expect_output() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql "$sql" "$@")
    expect_value "$label" "$expected" "$output"
}

expect_output_with_headers() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql_with_headers "$sql" "$@")
    expect_value "$label" "$expected" "$output"
}

expect_contains() {
    label=$1
    needle=$2
    haystack=$3

    case "$haystack" in
        *"$needle"*) ;;
        *) fail "$label: expected output to contain [$needle]" ;;
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
    "CREATE DATABASE ${DATABASE} CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci; "\
"USE ${DATABASE}; SET NAMES utf8mb4; SET SESSION sql_mode = 'NO_ENGINE_SUBSTITUTION';" \
    >/dev/null

scalar_expected=$(cat <<\EXPECTED
d41d8cd98f00b204e9800998ecf8427e	62a004b95946bb97541afa471dcca73a	NULL	202cb962ac59075b964b07152d234b70	c4ca4238a0b923820dcc509a6f75849b	cfcd208495d565ef66e7dff9f98764da	6bb61e3b7bce0931da574d19d1d82c88	900150983cd24fb0d6963f7d28e17f72	da39a3ee5e6b4b0d3255bfef95601890afd80709	a9993e364706816aba3e25717850c26c9cd0d89d	40bd001563085fc35165329ea1ff5c5ecbdbbeef
EXPECTED
)
expect_output \
    "scalar digest values" \
    "$scalar_expected" \
    "SELECT MD5(''),MD5('MySQL'),MD5(NULL),MD5(123),MD5(TRUE),MD5(FALSE), "\
"MD5(-1),MD5(X'616263'),SHA1(''),SHA('abc'),SHA1(123);" \
    "$DATABASE"

sha2_expected=$(cat <<\EXPECTED
d14a028c2a3a2bc9476102bb288234c415a2b01f828ea62ac5b3e42f	e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855	38b060a751ac96384cd9327eb1b1e36a21fdb71114be07434c0cc7bf63f6e1da274edebfe76f65fbd51ad2f14898b95b	cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e	e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855	NULL	NULL	ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad	NULL
EXPECTED
)
expect_output \
    "sha2 digest values" \
    "$sha2_expected" \
    "SELECT SHA2('',224),SHA2('',256),SHA2('',384),SHA2('',512),SHA2('',0), "\
"SHA2('',NULL),SHA2(NULL,256),SHA2('abc',FALSE),SHA2('abc',TRUE);" \
    "$DATABASE"

expect_output \
    "invalid sha2 warning" \
    "NULL
Warning	1583	Incorrect parameters in the call to native function 'sha2'
1	-1" \
    "SELECT SHA2('',1); SHOW WARNINGS; SELECT @@warning_count, ROW_COUNT();" \
    "$DATABASE"

expect_output_with_headers \
    "digest metadata charset and length" \
    "CHARSET(MD5('abc'))	COLLATION(MD5('abc'))	LENGTH(MD5('abc'))	LENGTH(SHA1('abc'))	LENGTH(SHA2('abc',224))	LENGTH(SHA2('abc',256))	LENGTH(SHA2('abc',384))	LENGTH(SHA2('abc',512))
utf8mb4	utf8mb4_0900_ai_ci	32	40	56	64	96	128" \
    "SELECT CHARSET(MD5('abc')),COLLATION(MD5('abc')),LENGTH(MD5('abc')), "\
"LENGTH(SHA1('abc')),LENGTH(SHA2('abc',224)),LENGTH(SHA2('abc',256)), "\
"LENGTH(SHA2('abc',384)),LENGTH(SHA2('abc',512));" \
    "$DATABASE"

metadata_output=$(run_mysql_type_info \
    "USE ${DATABASE}; SET NAMES utf8mb4; "\
"SELECT MD5('abc') AS md5_value, SHA('abc') AS sha_value, "\
"SHA2('abc',224) AS s224, SHA2('abc',256) AS s256, "\
"SHA2('abc',512) AS s512, SHA2('abc',1) AS bad, SHA2('abc',NULL) AS n;" \
    "$DATABASE")

expect_contains "md5 metadata field" 'Field   1:  `md5_value`' "$metadata_output"
expect_contains "md5 metadata type" 'Type:       VAR_STRING' "$metadata_output"
expect_contains "md5 metadata collation" 'Collation:  utf8mb4_0900_ai_ci (255)' "$metadata_output"
expect_contains "md5 metadata length" 'Length:     128' "$metadata_output"
expect_contains "sha metadata field" 'Field   2:  `sha_value`' "$metadata_output"
expect_contains "sha metadata length" 'Length:     160' "$metadata_output"
expect_contains "sha224 metadata field" 'Field   3:  `s224`' "$metadata_output"
expect_contains "sha224 metadata length" 'Length:     224' "$metadata_output"
expect_contains "sha256 metadata field" 'Field   4:  `s256`' "$metadata_output"
expect_contains "sha256 metadata length" 'Length:     256' "$metadata_output"
expect_contains "sha512 metadata field" 'Field   5:  `s512`' "$metadata_output"
expect_contains "sha512 metadata length" 'Length:     512' "$metadata_output"
expect_contains "invalid sha2 metadata field" 'Field   6:  `bad`' "$metadata_output"
expect_contains "invalid sha2 metadata length" 'Length:     256' "$metadata_output"
expect_contains "null sha2 metadata field" 'Field   7:  `n`' "$metadata_output"
expect_contains "null sha2 metadata max length" 'Max_length: 0' "$metadata_output"
expect_contains "digest metadata warnings" '1 row in set, 2 warnings' "$metadata_output"

run_mysql \
    "CREATE TABLE t("\
"id INT, v VARCHAR(10), c CHAR(3), txt TEXT, b BINARY(3), vb VARBINARY(3), "\
"bl BLOB, bi BIGINT"\
"); "\
"INSERT INTO t VALUES "\
"(1, 'abc', 'abc', 'abc', 'abc', X'616263', X'616263', 123), "\
"(2, NULL, NULL, NULL, NULL, NULL, NULL, NULL);" \
    "$DATABASE" >/dev/null

table_expected=$(cat <<\EXPECTED
1	900150983cd24fb0d6963f7d28e17f72	a9993e364706816aba3e25717850c26c9cd0d89d	a9993e364706816aba3e25717850c26c9cd0d89d	ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad	23097d223405d8228642a477bda255b32aadbce4bda0b3f7e36c9da7	cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed8086072ba1e7cc2358baeca134c825a7	3c9909afec25354d551dae21590bb26e38d53f2173b8d3dc3eee4c047e7ab1c1eb8b85103e3be7ba613b31bb5c9c36214dc9f14a42fd7a2fdb84856bca5c44c2
2	NULL	NULL	NULL	NULL	NULL	NULL	NULL
EXPECTED
)
expect_output \
    "table digest values" \
    "$table_expected" \
    "SELECT id,MD5(v),SHA(c),SHA1(txt),SHA2(b,256),SHA2(vb,224), "\
"SHA2(bl,384),SHA2(bi,512) FROM t ORDER BY id;" \
    "$DATABASE"

predicate_expected=$(cat <<\EXPECTED
1
1
1,3
1,2,3
EXPECTED
)
expect_output \
    "table digest predicates" \
    "$predicate_expected" \
    "INSERT INTO t VALUES (3, 'def', 'def', 'def', 'def', X'646566', X'646566', 456); "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM t WHERE MD5(v) = MD5('abc'); "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM t WHERE SHA(c) = SHA('abc'); "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM t WHERE SHA2(v,256) "\
"BETWEEN SHA2('abc',256) AND SHA2('def',256); "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM t WHERE SHA2(v,NULL) IS NULL;" \
    "$DATABASE"

expect_error \
    "md5 rejects zero arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'MD5'" \
    "SELECT MD5();" \
    "$DATABASE"

expect_error \
    "sha rejects extra arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'SHA'" \
    "SELECT SHA('a','b');" \
    "$DATABASE"

expect_error \
    "sha1 rejects zero arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'SHA1'" \
    "SELECT SHA1();" \
    "$DATABASE"

expect_error \
    "sha2 rejects missing length" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'SHA2'" \
    "SELECT SHA2('a');" \
    "$DATABASE"

expect_error \
    "sha2 rejects extra arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'SHA2'" \
    "SELECT SHA2('a',256,512);" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_digest_functions_expectations: ok"
