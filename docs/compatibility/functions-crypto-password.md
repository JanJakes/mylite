# Crypto and password functions

Encryption, digest, password, and random byte helpers.

| Function | Status | Notes |
| --- | --- | --- |
| `AES_DECRYPT()` | ❌ | Decrypt using AES |
| `AES_ENCRYPT()` | ❌ | Encrypt using AES |
| `MD5()` | 🟡 | Scalar, row projection, and documented `WHERE` predicates including row-scalar `IN`; no FIPS-mode NULL behavior or broad expression contexts |
| `RANDOM_BYTES()` | 🟡 | Scalar, row projection, and documented `WHERE IS [NOT] NULL`; no replication warning parity or broad expression contexts |
| `SHA1(), SHA()` | 🟡 | Scalar, row projection, and documented `WHERE` predicates including row-scalar `IN`; no broad expression contexts |
| `SHA2()` | 🟡 | Scalar, row projection, and documented `WHERE` predicates including row-scalar `IN` for supported lengths; no string-to-integer length coercion or broad expression contexts |
| `VALIDATE_PASSWORD_STRENGTH()` | ❌ | Determine strength of password |

[Back to compatibility overview](../../COMPATIBILITY.md)
