# Baseline Compression And Random Byte Functions

## Scope

This slice implements the MySQL 8.4.9-compatible baseline for:

- `COMPRESS(expr)`
- `UNCOMPRESS(expr)`
- `UNCOMPRESSED_LENGTH(expr)`
- `RANDOM_BYTES(expr)`

The functions are supported in direct scalar statements, `DO`, `DUAL` queries,
and row-backed projections. The implementation covers common scalar argument
forms already supported by adjacent binary-string functions: string literals,
binary literals, integer and boolean literals, `NULL`, supported session
scalars/system variables, binary casts/conversions, and descriptor columns.

AES encryption, password strength validation, replication safety warnings, and
cryptographic-strength guarantees beyond using the host operating system random
source remain outside this slice.

## References And Runtime Evidence

Normative sources:

- MySQL 8.4 Reference Manual, "Encryption and Compression Functions":
  `https://dev.mysql.com/doc/refman/8.4/en/encryption-functions.html`
- Runtime probes against local MySQL container `mylite-mysql-849`, version
  `8.4.9`.

Key observed runtime behavior:

```sql
SELECT HEX(COMPRESS('')), LENGTH(COMPRESS('')),
       UNCOMPRESS(COMPRESS('')) IS NULL,
       HEX(UNCOMPRESS(COMPRESS(''))),
       UNCOMPRESSED_LENGTH(COMPRESS(''));
-- '' | 0 | 0 | '' | 0

SELECT HEX(COMPRESS('abc')), LENGTH(COMPRESS('abc')),
       HEX(UNCOMPRESS(COMPRESS('abc'))),
       UNCOMPRESSED_LENGTH(COMPRESS('abc'));
-- 03000000789C4B4C4A0600024D0127 | 15 | 616263 | 3

SELECT COMPRESS(NULL) IS NULL, UNCOMPRESS(NULL) IS NULL,
       UNCOMPRESSED_LENGTH(NULL) IS NULL, RANDOM_BYTES(NULL) IS NULL;
-- 1 | 1 | 1 | 1

SELECT UNCOMPRESS('abc') IS NULL, UNCOMPRESSED_LENGTH('abc');
SHOW WARNINGS;
-- 1 | 0
-- Warning 1259 ZLIB: Input data corrupted
-- Warning 1259 ZLIB: Input data corrupted

SELECT LENGTH(RANDOM_BYTES(1)), LENGTH(RANDOM_BYTES(1024)),
       HEX(RANDOM_BYTES(4)) REGEXP '^[0-9A-F]{8}$';
-- 1 | 1024 | 1

SELECT LENGTH(RANDOM_BYTES('4x'));
SHOW WARNINGS;
-- 4
-- Warning 1292 Truncated incorrect INTEGER value: '4x'
```

`RANDOM_BYTES(0)`, negative lengths, lengths greater than `1024`, and
non-numeric string lengths raise error `1690` / SQLSTATE `22003` with message
`length value is out of range in 'random_bytes'`.

## Syntax

The functions are parsed as native function calls with one required argument.

```lemon
expression ::= COMPRESS LPAREN expression RPAREN.
expression ::= UNCOMPRESS LPAREN expression RPAREN.
expression ::= UNCOMPRESSED_LENGTH LPAREN expression RPAREN.
expression ::= RANDOM_BYTES LPAREN expression RPAREN.

expression ::= COMPRESS LPAREN RPAREN.
expression ::= COMPRESS LPAREN expression COMMA function_argument_list RPAREN.
expression ::= UNCOMPRESS LPAREN RPAREN.
expression ::= UNCOMPRESS LPAREN expression COMMA function_argument_list RPAREN.
expression ::= UNCOMPRESSED_LENGTH LPAREN RPAREN.
expression ::= UNCOMPRESSED_LENGTH LPAREN expression COMMA function_argument_list RPAREN.
expression ::= RANDOM_BYTES LPAREN RPAREN.
expression ::= RANDOM_BYTES LPAREN expression COMMA function_argument_list RPAREN.
```

The arity-error productions produce AST nodes that execute as MySQL native
function arity error `1582`.

## Semantics

### `COMPRESS()`

`COMPRESS(expr)` returns a binary string. `NULL` input returns `NULL`. Empty
input returns an empty binary string. Non-empty input returns MySQL's compressed
payload shape: a four-byte little-endian unsigned original byte length followed
by the zlib-compressed payload.

The compressed stream bytes can vary with zlib implementation details, so MyLite
tests prioritize MySQL-visible length, prefix, and round-trip behavior rather
than pinning every zlib output byte except in the MySQL expectation artifact.

### `UNCOMPRESS()`

`UNCOMPRESS(expr)` returns a binary string. `NULL` input returns `NULL`. Empty
input returns an empty binary string, matching the result of uncompressing
`COMPRESS('')`. Invalid compressed input returns `NULL` and appends warning
`1259`, SQLSTATE `HY000`, message `ZLIB: Input data corrupted`. If the stored
length prefix advertises more than MySQL's `67108864` byte decompression limit,
the result is `NULL` and warning `1256` reports the excessive size instead of
attempting to inflate or allocate the advertised output.

### `UNCOMPRESSED_LENGTH()`

`UNCOMPRESSED_LENGTH(expr)` returns the original byte length stored in a
compressed string. `NULL` input returns `NULL`; empty input returns `0`. Inputs
that do not contain the four-byte length prefix return `0` and append warning
`1259`, SQLSTATE `HY000`, message `ZLIB: Input data corrupted`. When the prefix
exists, MySQL reports the low 30 bits of the stored little-endian length without
inflating the payload; MyLite follows that behavior so malformed oversized
payloads do not force decompression or large allocation.

### `RANDOM_BYTES()`

`RANDOM_BYTES(expr)` returns a volatile binary string whose length is the
integer-coerced argument. `NULL` input returns `NULL`. Valid lengths are `1`
through `1024` inclusive. Values outside that range raise error `1690`,
SQLSTATE `22003`, with message `length value is out of range in 'random_bytes'`.

The function is not constant-folded. In row-backed execution, it is invoked per
output row through a registered SQLite scalar function.

Baseline length coercion supports integer, boolean, decimal, string, and binary
literal/scalar values. Decimal values round to the nearest integer using
MySQL-compatible half-up behavior for this positive range. String values accept
a leading numeric prefix; a non-empty trailing suffix appends warning `1292`,
SQLSTATE `22007`, message `Truncated incorrect INTEGER value: '<value>'`.
Strings without a leading numeric prefix produce the `1690` range error.

## Result Metadata

Observed MySQL metadata for representative calls:

- `COMPRESS('abc')`: `VAR_STRING`, binary collation `63`, binary flag.
- `UNCOMPRESS(COMPRESS('abc'))`: `LONG_BLOB`, binary collation `63`, binary
  flag.
- `UNCOMPRESSED_LENGTH(COMPRESS('abc'))`: `LONGLONG`, numeric flag, display
  length `10`.
- `RANDOM_BYTES(4)`: `VAR_STRING`, binary collation `63`, binary flag, display
  length `1024`.

MyLite should expose binary string metadata for `COMPRESS()`, `UNCOMPRESS()`,
and `RANDOM_BYTES()`, and integer metadata for `UNCOMPRESSED_LENGTH()`.

## Architecture

The implementation belongs in MyLite runtime code and SQLite public scalar
function registrations:

- `mylite_string_compression` owns MySQL compressed payload encoding/decoding,
  zlib calls, invalid-input warnings, and `_mylite_compress`,
  `_mylite_uncompress`, `_mylite_uncompressed_length` registration.
- `mylite_random_bytes` owns secure random byte generation, length coercion, and
  `_mylite_random_bytes` registration.
- Parser/AST support is added in MyLite's Lemon grammar and AST enums.
- Direct scalar evaluation uses the same helper modules so no-source `SELECT`
  and `DO` match row-backed SQLite execution.

No SQLite fork patch or file-format change is required.

## Tests

Testing must include:

- MySQL expectation script verified against MySQL 8.4.9 for values, warnings,
  arity errors, and random length bounds.
- Parser tests for all function AST nodes and arity-error nodes.
- Runtime C tests for direct scalar, `DUAL`, `DO`, and row-backed execution.
- Warning count checks for invalid compressed values and truncated
  `RANDOM_BYTES()` string lengths.
- Error checks for invalid random-byte lengths and native arity errors.

## Known Limitations

- MyLite does not emulate MySQL replication safety warnings for
  nondeterministic functions.
- Very large compression inputs beyond a 32-bit stored length are rejected by
  MyLite runtime limits rather than attempting MySQL large-object behavior.
- The exact compressed zlib byte stream is not treated as a portable assertion
  across zlib versions; the MySQL storage format, length prefix, and round-trip
  behavior are the compatibility target.
