# MD5, SHA, SHA1, and SHA2 Hash Functions

## Scope

This slice implements MySQL-compatible scalar checksum functions:

- `MD5(str)`
- `SHA(str)`
- `SHA1(str)`
- `SHA2(str, hash_length)`

The feature applies to no-table `SELECT`, table-backed projection, `WHERE`,
`ORDER BY`, and the supported single-table `UPDATE` and `DELETE` expression
paths. It does not implement `STATEMENT_DIGEST()`, password hashing,
enterprise encryption functions, or FIPS-mode disabling.

Primary reference: MySQL 8.4 Reference Manual, Encryption and Compression
Functions:
https://dev.mysql.com/doc/refman/8.4/en/encryption-functions.html

## MySQL-Verified Behavior

Runtime checks were performed against MySQL 8.4.9.

### Results

- `MD5(str)` returns 32 lowercase hexadecimal characters for the MD5 digest.
- `SHA(str)` and `SHA1(str)` are synonyms and return 40 lowercase hexadecimal
  characters for SHA-1.
- `SHA2(str, hash_length)` accepts `224`, `256`, `384`, `512`, and `0`; `0`
  is equivalent to `256`.
- `SHA2()` returns lowercase hexadecimal text with 56, 64, 96, or 128
  characters for the accepted bit lengths.
- `MD5(NULL)`, `SHA(NULL)`, and `SHA1(NULL)` return `NULL` without warnings.
- `SHA2(NULL, valid_length)` returns `NULL` without warnings.
- `SHA2(str, NULL)` returns `NULL` and emits warning 1583.
- `SHA2(str, invalid_length)` returns `NULL` and emits warning 1583.
- String hash-length arguments are converted using MySQL integer conversion;
  trailing garbage emits warning 1292 before hash-length validation.
- Exact numeric source arguments are converted to MySQL's exact numeric string
  form before hashing, including dropping a leading plus sign and redundant
  leading zeroes for decimal values.
- Approximate numeric source arguments use compact approximate string
  formatting before hashing.
- Binary string inputs are hashed by byte length, including embedded NUL bytes.

Representative verified values:

| Expression | Result |
| --- | --- |
| `MD5('testing')` | `ae2b1fca515949e5d54fb22b8ed95575` |
| `MD5('')` | `d41d8cd98f00b204e9800998ecf8427e` |
| `MD5(12.50)` | `4bc129be882d904ee70110a444945e9a` |
| `SHA('abc')` | `a9993e364706816aba3e25717850c26c9cd0d89d` |
| `SHA1('abc')` | `a9993e364706816aba3e25717850c26c9cd0d89d` |
| `SHA2('abc', 224)` | `23097d223405d8228642a477bda255b32aadbce4bda0b3f7e36c9da7` |
| `SHA2('abc', 256)` | `ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad` |
| `SHA2('abc', 384)` | `cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed8086072ba1e7cc2358baeca134c825a7` |
| `SHA2('abc', 512)` | `ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f` |

### Metadata

The result type is `VAR_STRING`, decimals are `31`, and flags are empty unless
the connection character set is `binary`, in which case MySQL reports the
`BINARY` flag. The character set and collation follow the connection character
set and collation, and coercibility is `4`.

Observed declared lengths under `utf8mb4`:

- `MD5()` length: `128`
- `SHA()` / `SHA1()` length: `160`
- `SHA2(..., 224)` length: `224`
- `SHA2(..., 256)` and `SHA2(..., 0)` length: `256`
- `SHA2(..., 384)` length: `384`
- `SHA2(..., 512)` length: `512`
- `SHA2()` with invalid or `NULL` literal length: `256`
- `SHA2()` with a cacheable constant length expression uses the converted
  length value; unsupported or `NULL` converted lengths declare `256`
- `SHA2()` with a nonconstant length expression: `512`

Under `latin1`, the corresponding lengths are the number of hexadecimal
characters.

### Arity And Diagnostics

Wrong arity is rejected with error 1582:

- `MD5()` and `MD5(a, b)`
- `SHA()` and `SHA(a, b)`
- `SHA1()` and `SHA1(a, b)`
- `SHA2(a)` and `SHA2(a, b, c)`

`SHA2()` invalid parameter warnings use code 1583 and message shape
`Incorrect parameters in the call to native function 'sha2'`.

## MyLite Semantics

MyLite implements the digest algorithms internally and does not add a crypto
library dependency for this scalar slice. Results are intended for MySQL
compatibility, not password storage guidance.

Argument conversion uses the same source-text conversion path as `CRC32()` so
numeric literal hashing matches MySQL's stringification behavior. Binary values
are passed with their stored byte length.

`SHA2()` validates the second argument after evaluating it with existing MySQL
integer conversion. The result is `NULL` when the converted length is not one
of `0`, `224`, `256`, `384`, or `512`; a warning is appended for invalid and
`NULL` length values.

## Parser And AST

No grammar changes are needed. These functions use ordinary function-call
syntax:

```lemon
expr ::= ident LP function_arg_list RP.
```

Supported arity is enforced during expression support validation:

- `MD5`, `SHA`, `SHA1`: exactly one argument.
- `SHA2`: exactly two arguments.

## Runtime And Storage

No storage format changes are required. Digest computation is deterministic and
has no session state. Result collation is connection-coercible text. DML paths
promote warnings according to the existing strict expression warning rules.

## Compatibility Status

This slice marks `MD5()`, `SHA()` / `SHA1()`, and `SHA2()` as partially
implemented because supported expression contexts are covered, while exact
native error exposure outside current prepare paths, FIPS-mode behavior, and
server OpenSSL availability reporting remain deferred.
