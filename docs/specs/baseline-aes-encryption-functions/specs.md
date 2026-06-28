# Baseline AES Encryption Functions

## Summary

This slice implements MySQL-compatible default-mode two-argument
`AES_ENCRYPT()` and `AES_DECRYPT()` behavior:

```sql
SELECT HEX(AES_ENCRYPT('text', 'key'));
SELECT CAST(AES_DECRYPT(AES_ENCRYPT('text', 'key'), 'key') AS CHAR);
```

MySQL 8.4 uses the `@@block_encryption_mode` system variable for the full AES
surface. The default mode is `aes-128-ecb`. MyLite implements that default
embedded baseline with no external crypto dependency, leaving IV, KDF, and
non-default block-mode behavior for later slices.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- MySQL 8.4 Reference Manual, encryption and compression functions:
  <https://dev.mysql.com/doc/refman/8.4/en/encryption-functions.html>
- MySQL 8.4.9 runtime observations recorded by
  `packages/libmylite/tests/mysql_baseline_aes_encryption_functions_expectations.sh`.

This specification is independently authored from official MySQL 8.4
documentation, observed MySQL 8.4.9 behavior, public SQLite APIs, and existing
MyLite source. It does not copy MySQL, MariaDB, Percona, SQLite implementation
internals, or restrictively licensed implementation sources.

## MySQL 8.4.9 Observations

Runtime probes under default `@@block_encryption_mode = 'aes-128-ecb'`
establish:

- `HEX(AES_ENCRYPT('text','key'))` returns
  `15E36637363712FC2E699B9C95B75393`.
- `HEX(AES_ENCRYPT('','key'))` returns
  `C717530F41F320757B4AA1BFAF11C42E`.
- A 16-byte plaintext produces 32 encrypted bytes because a full padding block
  is appended.
- `AES_DECRYPT(AES_ENCRYPT('text','key'),'key')` returns the original binary
  string.
- Binary plaintexts round-trip byte-for-byte.
- If any required argument is `NULL`, the function returns `NULL`.
- Scalar `AES_ENCRYPT(NULL, long_key)` still reports the long-key warning after
  evaluating the key expression.
- `AES_DECRYPT(NULL, long_key)` and invalid decrypt inputs return `NULL` while
  still reporting the long-key warning count when the key is longer than 16
  bytes.
- Invalid encrypted input or incorrect padding returns `NULL`.
- Without a KDF, MySQL folds the supplied key into a zeroed 16-byte AES key by
  XORing bytes and wrapping after 16 bytes.
- A non-KDF key longer than 16 bytes appends warning `3237`; short keys and
  exact 16-byte keys do not warn.
- Both functions report native-function parameter-count diagnostics for arity
  mismatch.
- Result metadata is binary `VAR_STRING`.

## Scope

Supported:

- `AES_ENCRYPT(str, key_str)` in default `aes-128-ecb` mode;
- `AES_DECRYPT(crypt_str, key_str)` in default `aes-128-ecb` mode;
- MySQL legacy no-KDF 16-byte key folding;
- PKCS-style padding add/remove behavior used by MySQL's default AES path;
- SQL `NULL` propagation;
- invalid decrypt input returning SQL `NULL`;
- warning `3237` for scalar calls with non-KDF keys longer than 16 bytes;
- scalar and single-table row-backed projection contexts, plus binary-safe
  predicate contexts through the current row-scalar expression envelope;
- binary `VAR_STRING` result metadata.

Deferred:

- optional `init_vector` argument;
- `hkdf` and `pbkdf2_hmac` KDF arguments;
- non-default `@@block_encryption_mode` values;
- broad arbitrary-expression/DML coverage beyond the current binary-function
  envelope;
- exact warning accumulation for row-backed SQLite callback execution.

## Ownership Boundaries

- Public API: no new ABI.
- Parser: generic function-call syntax admits the names; MyLite recognizes the
  function names in the execution layer.
- Runtime: MyLite owns default AES-128 ECB encryption/decryption, key folding,
  padding, diagnostics, warnings, and result metadata.
- SQLite: row-backed execution uses MyLite's public SQLite scalar UDF bridge.
  No targeted SQLite fork hook is required.
- Storage: no file-format or catalog change.

## Supported SQL Grammar

Existing generic function grammar admits the function names.

MyLite Lemon-syntax sketch:

```lemon
expr ::= generic_function_call.

generic_function_call ::=
    aes_function_name LPAREN expression COMMA expression RPAREN.

aes_function_name ::= AES_ENCRYPT | AES_DECRYPT.
```

## Runtime Semantics

- Exactly two arguments are supported.
- If either argument is SQL `NULL`, the result is SQL `NULL`.
- Arguments are evaluated as binary strings using the same binary argument
  conversion helper as the existing digest/compression functions.
- The AES key is initialized to 16 zero bytes, then every supplied key byte is
  XORed into `key[index % 16]`.
- Encryption appends padding from 1 to 16 bytes and encrypts every 16-byte
  block with AES-128 ECB.
- Decryption requires a non-empty encrypted byte string whose size is a
  multiple of 16 bytes. It decrypts with AES-128 ECB, validates/removes padding,
  and returns `NULL` on invalid padding.
- The long-key warning is appended for scalar calls with key byte length greater
  than 16 bytes when MySQL processes the key for the supported default-mode
  path.

## Tests

The test suite covers:

- MySQL expectation capture for scalar encrypt/decrypt values, binary
  round-trips, nulls, invalid decrypt input, long-key warnings, metadata, and
  arity diagnostics;
- MyLite runtime scalar and row-backed projection results;
- predicate use over encrypted/decrypted values through `HEX()` wrappers;
- result metadata and diagnostics.
