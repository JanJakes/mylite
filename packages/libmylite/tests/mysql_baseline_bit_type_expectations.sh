#!/usr/bin/env sh

set -eu

MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_bit_type_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_bit_type_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | "$MYSQL_BIN" --protocol=SOCKET --socket="$MYSQL_SOCKET" \
                -uroot --batch --raw --skip-column-names --binary-as-hex=1 "$@"
    else
        printf '%s\n' "$sql" \
            | docker exec -i "$MYSQL_CONTAINER" mysql \
                -uroot --batch --raw --skip-column-names --binary-as-hex=1 "$@"
    fi
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

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

case "$(run_mysql "SELECT @@sql_mode;")" in
    *STRICT_TRANS_TABLES*) ;;
    *) fail "expected strict default sql_mode" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

show_columns_expected=$(printf '%b\n' \
    "id\tint\tNO\t\tNULL\t" \
    "b\tbit(1)\tYES\t\tNULL\t" \
    "b1\tbit(1)\tYES\t\tNULL\t" \
    "b6\tbit(6)\tYES\t\tNULL\t" \
    "b8\tbit(8)\tYES\t\tNULL\t" \
    "b9\tbit(9)\tYES\t\tNULL\t" \
    "b64\tbit(64)\tYES\t\tNULL\t" \
    "nn\tbit(6)\tNO\t\tNULL\t")
expect_output \
    "show columns renders bit descriptors" \
    "$show_columns_expected" \
    "CREATE TABLE bits (id INT NOT NULL, b BIT, b1 BIT(1), b6 BIT(6), "\
"b8 BIT(8), b9 BIT(9), b64 BIT(64), nn BIT(6) NOT NULL); "\
"SHOW COLUMNS FROM bits;" \
    "$DATABASE"

show_create_expected=$(cat <<\EXPECTED
bits	CREATE TABLE `bits` (
  `id` int NOT NULL,
  `b` bit(1) DEFAULT NULL,
  `b1` bit(1) DEFAULT NULL,
  `b6` bit(6) DEFAULT NULL,
  `b8` bit(8) DEFAULT NULL,
  `b9` bit(9) DEFAULT NULL,
  `b64` bit(64) DEFAULT NULL,
  `nn` bit(6) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "show create renders bit descriptors" \
    "$show_create_expected" \
    "SHOW CREATE TABLE bits;" \
    "$DATABASE"

information_schema_expected=$(cat <<\EXPECTED
b	bit	bit(1)	1	NULL	NULL	NULL	YES	NULL
b1	bit	bit(1)	1	NULL	NULL	NULL	YES	NULL
b6	bit	bit(6)	6	NULL	NULL	NULL	YES	NULL
b8	bit	bit(8)	8	NULL	NULL	NULL	YES	NULL
b9	bit	bit(9)	9	NULL	NULL	NULL	YES	NULL
b64	bit	bit(64)	64	NULL	NULL	NULL	YES	NULL
nn	bit	bit(6)	6	NULL	NULL	NULL	NO	NULL
EXPECTED
)
expect_output \
    "information schema renders bit metadata" \
    "$information_schema_expected" \
    "SELECT COLUMN_NAME, DATA_TYPE, COLUMN_TYPE, NUMERIC_PRECISION, NUMERIC_SCALE, "\
"CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, IS_NULLABLE, COLUMN_DEFAULT "\
"FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA='${DATABASE}' "\
"AND TABLE_NAME='bits' AND COLUMN_NAME <> 'id' ORDER BY ORDINAL_POSITION;" \
    "$DATABASE"

readback_expected=$(cat <<\EXPECTED
1	0x01	1	0x01	1	0x05	1	0x0A	1	0x0001	2	0x0000000000000001	8	0x3F	1
2	0x00	1	0x00	1	0x05	1	0x0A	1	0x0101	2	0xFFFFFFFFFFFFFFFF	8	0x00	1
3	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	0x01	1
EXPECTED
)
expect_output \
    "bit values read back as fixed-width bytes" \
    "$readback_expected" \
    "INSERT INTO bits VALUES "\
"(1, b'1', b'1', b'101', b'1010', b'1', b'1', b'111111'), "\
"(2, 0, 0, 5, 10, 257, 18446744073709551615, 0), "\
"(3, NULL, NULL, NULL, NULL, NULL, NULL, 1); "\
"SELECT id, b, LENGTH(b), b1, LENGTH(b1), b6, LENGTH(b6), b8, LENGTH(b8), "\
"b9, LENGTH(b9), b64, LENGTH(b64), nn, LENGTH(nn) FROM bits ORDER BY id;" \
    "$DATABASE"

update_expected=$(cat <<\EXPECTED
0	0	0x05	5
1	0	0x20	32
EXPECTED
)
expect_output \
    "bit update changed-row semantics" \
    "$update_expected" \
    "UPDATE bits SET b6 = b'101' WHERE id = 1; "\
"SELECT ROW_COUNT(), @@warning_count, b6, b6+0 FROM bits WHERE id = 1; "\
"UPDATE bits SET b6 = b'100000' WHERE id = 1; "\
"SELECT ROW_COUNT(), @@warning_count, b6, b6+0 FROM bits WHERE id = 1;" \
    "$DATABASE"

predicates_expected=$(cat <<\EXPECTED
eq_bit	1
lt_bit	2
null_safe	1
is_null	3
is_not_null	1,2
between_bits	2
in_bits	1,2
asc_ids	3,2,1
desc_ids	1,2,3
EXPECTED
)
expect_output \
    "bit predicates and ordering" \
    "$predicates_expected" \
    "SELECT 'eq_bit', GROUP_CONCAT(id ORDER BY id) FROM bits WHERE b6 = b'100000'; "\
"SELECT 'lt_bit', GROUP_CONCAT(id ORDER BY id) FROM bits WHERE b6 < b'100000'; "\
"SELECT 'null_safe', GROUP_CONCAT(id ORDER BY id) FROM bits WHERE b6 <=> b'100000'; "\
"SELECT 'is_null', GROUP_CONCAT(id ORDER BY id) FROM bits WHERE b6 IS NULL; "\
"SELECT 'is_not_null', GROUP_CONCAT(id ORDER BY id) FROM bits WHERE b6 IS NOT NULL; "\
"SELECT 'between_bits', GROUP_CONCAT(id ORDER BY id) FROM bits WHERE b6 BETWEEN b'1' AND b'101'; "\
"SELECT 'in_bits', GROUP_CONCAT(id ORDER BY id) FROM bits WHERE b6 IN (b'100000', b'101', NULL); "\
"SELECT 'asc_ids', GROUP_CONCAT(id ORDER BY b6 ASC, id ASC) FROM bits; "\
"SELECT 'desc_ids', GROUP_CONCAT(id ORDER BY b6 DESC, id ASC) FROM bits;" \
    "$DATABASE"

defaults_expected=$(
    printf '%b\n' \
        "id\tint\tYES\t\tNULL\t" \
        "b\tbit(6)\tYES\t\tb'101'\t" \
        "i\tbit(6)\tYES\t\tb'101'\t" \
        "t\tbit(1)\tYES\t\tb'1'\t" \
        "f\tbit(1)\tYES\t\tb'0'\t"
    cat <<\EXPECTED
defaults_t	CREATE TABLE `defaults_t` (
  `id` int DEFAULT NULL,
  `b` bit(6) DEFAULT b'101',
  `i` bit(6) DEFAULT b'101',
  `t` bit(1) DEFAULT b'1',
  `f` bit(1) DEFAULT b'0'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
1	0x05	5	0x05	5	0x01	1	0x00	0
EXPECTED
)
expect_output \
    "bit literal defaults render and materialize" \
    "$defaults_expected" \
    "CREATE TABLE defaults_t (id INT, b BIT(6) DEFAULT b'101', "\
"i BIT(6) DEFAULT 5, t BIT(1) DEFAULT TRUE, f BIT(1) DEFAULT FALSE); "\
"SHOW COLUMNS FROM defaults_t; SHOW CREATE TABLE defaults_t; "\
"INSERT INTO defaults_t(id) VALUES (1); "\
"SELECT id, b, b+0, i, i+0, t, t+0, f, f+0 FROM defaults_t;" \
    "$DATABASE"

literal_inputs_expected=$(cat <<\EXPECTED
0x41	65	1
0x31	49	1
0x30	48	1
0x01	1	1
0x01	1	1
0x00	0	1
0x00	0	1
0x00	0	1
EXPECTED
)
expect_output \
    "bit string hex and empty bit literals" \
    "$literal_inputs_expected" \
    "CREATE TABLE literal_inputs (b BIT(8)); "\
"INSERT INTO literal_inputs VALUES ('A'), ('1'), ('0'), (X'01'), "\
"(X'0001'), (X'0000'), (b''), (b'00000000'); "\
"SELECT b, b+0, LENGTH(b) FROM literal_inputs;" \
    "$DATABASE"

ignore_expected=$(printf '%b\n' \
    "4" \
    "Warning\t1406\tData too long for column 'b1' at row 1" \
    "Warning\t1406\tData too long for column 'b6' at row 1" \
    "Warning\t1048\tColumn 'b1' cannot be null" \
    "Warning\t1048\tColumn 'b6' cannot be null" \
    "1\t0x01\t1\t0x3F\t63" \
    "2\t0x00\t0\t0x00\t0" \
    "3\t0x01\t1\t0x3F\t63" \
    "1" \
    "Warning\t1364\tField 'b' doesn't have a default value" \
    "1\t0x00\t0")
expect_output \
    "insert ignore clips bit values and records warnings" \
    "$ignore_expected" \
    "CREATE TABLE ignore_bits (id INT, b1 BIT(1) NOT NULL, b6 BIT(6) NOT NULL); "\
"INSERT IGNORE INTO ignore_bits(id, b1, b6) VALUES "\
"(1, b'10', b'1000000'), (2, NULL, NULL), (3, 1, 63); "\
"SHOW COUNT(*) WARNINGS; SHOW WARNINGS; "\
"SELECT id, b1, b1+0, b6, b6+0 FROM ignore_bits ORDER BY id; "\
"CREATE TABLE omitted_bits (id INT, b BIT(6) NOT NULL); "\
"INSERT IGNORE INTO omitted_bits(id) VALUES (1); "\
"SHOW COUNT(*) WARNINGS; SHOW WARNINGS; "\
"SELECT id, b, b+0 FROM omitted_bits;" \
    "$DATABASE"

ordered_update_expected=$(cat <<\EXPECTED
2	0
1	0x07
2	0x00
3	0x00
4	0x07
EXPECTED
)
expect_output \
    "ordered limited update over bit column" \
    "$ordered_update_expected" \
    "CREATE TABLE ordered_update (id INT, b BIT(3), marker BIT(3)); "\
"INSERT INTO ordered_update VALUES (1, b'001', 0), (2, b'010', 0), "\
"(3, b'101', 0), (4, NULL, 0); "\
"UPDATE ordered_update SET marker = b'111' ORDER BY b ASC LIMIT 2; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT id, marker FROM ordered_update ORDER BY id;" \
    "$DATABASE"

expect_error \
    "bit zero width rejected" \
    3013 \
    HY000 \
    "Invalid size for column 'b'." \
    "CREATE TABLE bad_bit_zero (b BIT(0));" \
    "$DATABASE"

expect_error \
    "bit oversized width rejected" \
    1439 \
    42000 \
    "Display width out of range for column 'b' (max = 64)" \
    "CREATE TABLE bad_bit_wide (b BIT(65));" \
    "$DATABASE"

expect_error \
    "bit empty width syntax rejected" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "CREATE TABLE bad_bit_empty (b BIT());" \
    "$DATABASE"

expect_error \
    "bit over-width bit literal rejected" \
    1406 \
    22001 \
    "Data too long for column 'b1' at row 1" \
    "CREATE TABLE strict_bits (b1 BIT(1), b6 BIT(6), b8 BIT(8)); "\
"INSERT INTO strict_bits(b1) VALUES (b'10');" \
    "$DATABASE"

expect_error \
    "bit over-width integer rejected" \
    1406 \
    22001 \
    "Data too long for column 'b6' at row 1" \
    "INSERT INTO strict_bits(b6) VALUES (64);" \
    "$DATABASE"

expect_error \
    "bit negative integer rejected" \
    1406 \
    22001 \
    "Data too long for column 'b6' at row 1" \
    "INSERT INTO strict_bits(b6) VALUES (-1);" \
    "$DATABASE"

expect_error \
    "bit unsigned 64 overflow rejected" \
    1264 \
    22003 \
    "Out of range value for column 'b8' at row 1" \
    "INSERT INTO strict_bits(b8) VALUES (18446744073709551616);" \
    "$DATABASE"

expect_error \
    "bit not null rejected" \
    1048 \
    23000 \
    "Column 'b1' cannot be null" \
    "CREATE TABLE not_null_bits (b1 BIT(1) NOT NULL); "\
"INSERT INTO not_null_bits VALUES (NULL);" \
    "$DATABASE"

expect_error \
    "bit invalid default rejected" \
    1067 \
    42000 \
    "Invalid default value for 'b'" \
    "CREATE TABLE bad_default (b BIT(6) DEFAULT b'1000000');" \
    "$DATABASE"

bit_expression_defaults_expected=$(cat <<\EXPECTED
b	bit(6)	YES		(1 + 2)	DEFAULT_GENERATED
m	bit(6)	YES		(7 % 4)	DEFAULT_GENERATED
n	bit(6)	YES		NULL	DEFAULT_GENERATED
nn	bit(6)	NO		NULL	DEFAULT_GENERATED
bit_expr	CREATE TABLE `bit_expr` (
  `id` int DEFAULT NULL,
  `b` bit(6) DEFAULT ((1 + 2)),
  `m` bit(6) DEFAULT ((7 % 4)),
  `n` bit(6) DEFAULT (NULL),
  `nn` bit(6) NOT NULL DEFAULT (NULL)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
b	(1 + 2)	DEFAULT_GENERATED
m	(7 % 4)	DEFAULT_GENERATED
n	NULL	DEFAULT_GENERATED
nn	NULL	DEFAULT_GENERATED
1	0x03	3	0x03	3	1	0x07	7
b	bit(6)	YES		(2 * 5)	DEFAULT_GENERATED
1	0x03	3	7
2	0x0A	10	1
1	0x0A	10	0x03	3	1	0x07	7
4	0x0A	10	0x03	3	1	0x02	2
5	0x0A	10	0x03	3	1	0x05	5
EXPECTED
)
expect_output \
    "bit expression defaults render and materialize" \
    "$bit_expression_defaults_expected" \
    "CREATE TABLE bit_expr (id INT, b BIT(6) DEFAULT (1 + 2), "\
"m BIT(6) DEFAULT (MOD(7,4)), n BIT(6) DEFAULT (NULL), "\
"nn BIT(6) NOT NULL DEFAULT (NULL)); "\
"SHOW COLUMNS FROM bit_expr WHERE Field <> 'id'; "\
"SHOW CREATE TABLE bit_expr; "\
"SELECT COLUMN_NAME, COLUMN_DEFAULT, EXTRA FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA='${DATABASE}' AND TABLE_NAME='bit_expr' "\
"AND COLUMN_NAME <> 'id' ORDER BY ORDINAL_POSITION; "\
"INSERT INTO bit_expr (id, nn) VALUES (1, b'111'); "\
"SELECT id, b, b+0, m, m+0, n IS NULL, nn, nn+0 FROM bit_expr; "\
"ALTER TABLE bit_expr ALTER COLUMN b SET DEFAULT (2 * 5); "\
"SHOW COLUMNS FROM bit_expr LIKE 'b'; "\
"INSERT INTO bit_expr (id, nn) VALUES (2, b'001'); "\
"SELECT id, b, b+0, nn+0 FROM bit_expr ORDER BY id; "\
"UPDATE bit_expr SET b = DEFAULT, m = DEFAULT, n = DEFAULT WHERE id = 1; "\
"INSERT INTO bit_expr (id, b, m, n, nn) "\
"VALUES (4, DEFAULT, DEFAULT, DEFAULT, b'010'); "\
"REPLACE INTO bit_expr (id, b, m, n, nn) "\
"VALUES (5, DEFAULT, DEFAULT, DEFAULT, b'101'); "\
"SELECT id, b, b+0, m, m+0, n IS NULL, nn, nn+0 FROM bit_expr "\
"WHERE id IN (1, 4, 5) ORDER BY id;" \
    "$DATABASE"

expect_output \
    "MySQL accepts out-of-range bit expression default at DDL time" \
    "b	bit(6)	YES		64	DEFAULT_GENERATED" \
    "CREATE TABLE bit_range (b BIT(6) DEFAULT (64)); SHOW COLUMNS FROM bit_range;" \
    "$DATABASE"

expect_error \
    "MySQL rejects out-of-range bit expression default on materialization" \
    1406 \
    22001 \
    "Data too long for column 'b' at row 1" \
    "INSERT INTO bit_range () VALUES ();" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts deferred fractional bit width" \
    "CREATE TABLE upstream_fractional_bit_width (b BIT(1.2));" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts deferred hex and string bit defaults" \
    "CREATE TABLE upstream_hex_string_default (h BIT(6) DEFAULT X'05', s BIT(8) DEFAULT 'A');" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_bit_type_expectations: ok"
