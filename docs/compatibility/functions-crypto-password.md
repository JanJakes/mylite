# Crypto and password functions

Encryption, digest, password, and random byte helpers.

| Function | Status | Notes |
| --- | --- | --- |
| `AES_DECRYPT()` | ❌ | Decrypt using AES |
| `AES_ENCRYPT()` | ❌ | Encrypt using AES |
| `MD5()` | 🟡 | Baseline lowercase hexadecimal digest for supported scalar and row-projection arguments; no FIPS-mode NULL behavior |
| `RANDOM_BYTES()` | 🟡 | Baseline volatile binary output for lengths `1..1024`, `NULL`, range errors, and simple integer coercion |
| `SHA1(), SHA()` | 🟡 | Baseline lowercase hexadecimal SHA-1 digest for supported scalar and row-projection arguments |
| `SHA2()` | 🟡 | Baseline lowercase hexadecimal SHA-2 digest for lengths `224`, `256`, `384`, `512`, and `0`; unsupported scalar numeric lengths return `NULL` with warning `1583`; no string-to-integer length coercion |
| `VALIDATE_PASSWORD_STRENGTH()` | ❌ | Determine strength of password |

[Back to compatibility overview](../../COMPATIBILITY.md)
