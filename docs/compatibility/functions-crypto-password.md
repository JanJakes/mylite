# Crypto and password functions

Encryption, digest, password, and random byte helpers.

| Function | Status | Notes |
| --- | --- | --- |
| `AES_DECRYPT()` | ❌ | Decrypt using AES |
| `AES_ENCRYPT()` | ❌ | Encrypt using AES |
| `MD5()` | 🟡 | Scalar, row projection, documented `WHERE` predicates including row-scalar `IN`, and compatible non-key single-table `UPDATE` / duplicate-key assignments; no FIPS-mode NULL behavior or broad expression contexts |
| `RANDOM_BYTES()` | 🟡 | Scalar, row projection, documented `WHERE IS [NOT] NULL`, and compatible non-key single-table `UPDATE` / duplicate-key assignments; no replication warning parity or broad expression contexts |
| `SHA1(), SHA()` | 🟡 | Scalar, row projection, documented `WHERE` predicates including row-scalar `IN`, and compatible non-key single-table `UPDATE` / duplicate-key assignments; no broad expression contexts |
| `SHA2()` | 🟡 | Scalar, row projection, documented `WHERE` predicates including row-scalar `IN` for supported lengths, and compatible non-key single-table `UPDATE` / duplicate-key assignments; no string-to-integer length coercion or broad expression contexts |
| `VALIDATE_PASSWORD_STRENGTH()` | ❌ | Determine strength of password |

[Back to compatibility overview](../../COMPATIBILITY.md)
