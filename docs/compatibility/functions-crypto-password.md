# Crypto and password functions

Encryption, digest, password, and random byte helpers.

| Function | Status | Notes |
| --- | --- | --- |
| `AES_DECRYPT()` | 🟡 | Default two-argument `aes-128-ecb` baseline with binary metadata, NULL/invalid-input behavior, and scalar/row-backed documented contexts; no IV, KDF, or non-default block modes |
| `AES_ENCRYPT()` | 🟡 | Default two-argument `aes-128-ecb` baseline with MySQL key folding, padding, binary metadata, and scalar/row-backed documented contexts; no IV, KDF, or non-default block modes |
| `MD5()` | 🟡 | Scalar, row projection, documented `WHERE` predicates including row-scalar `IN`, and compatible non-key single-table `UPDATE` / duplicate-key assignments; no FIPS-mode NULL behavior or broad expression contexts |
| `RANDOM_BYTES()` | 🟡 | Scalar, row projection, documented `WHERE IS [NOT] NULL`, and compatible non-key single-table `UPDATE` / duplicate-key assignments; no replication warning parity or broad expression contexts |
| `SHA1(), SHA()` | 🟡 | Scalar, row projection, documented `WHERE` predicates including row-scalar `IN`, and compatible non-key single-table `UPDATE` / duplicate-key assignments; no broad expression contexts |
| `SHA2()` | 🟡 | Scalar, row projection, documented `WHERE` predicates including row-scalar `IN` for supported lengths, and compatible non-key single-table `UPDATE` / duplicate-key assignments; no string-to-integer length coercion or broad expression contexts |
| `VALIDATE_PASSWORD_STRENGTH()` | ✅ | Component-absent baseline returns `0` for non-`NULL` inputs and `NULL` for `NULL`, including scalar, row-backed, metadata, and arity coverage; no installed-component policy scoring |

[Back to compatibility overview](../../COMPATIBILITY.md)
