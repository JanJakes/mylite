# Baseline SHOW STATUS Security and Compression Placeholders

## Scope

This slice adds MySQL 8.4.9-shaped `SHOW STATUS` metadata for connection
compression discovery, TLS credential discovery, TLS library/SNI discovery, and
RSA public-key discovery rows that are useful to compatibility probes.

MyLite does not implement a client/server compression protocol, TLS listener,
OpenSSL dependency, LDAP SASL authentication plugin, or server-side RSA
password-key exchange for this slice. Rows are deterministic embedded
placeholders:

| Variable | Default/session/LOCAL visibility | GLOBAL visibility | MyLite value |
| --- | --- | --- | --- |
| `Caching_sha2_password_rsa_public_key` | yes | yes | empty string |
| `Compression_algorithm` | yes | no | empty string |
| `Compression_level` | yes | no | `0` |
| `Current_tls_ca` | yes | yes | empty string |
| `Current_tls_capath` | yes | yes | empty string |
| `Current_tls_cert` | yes | yes | empty string |
| `Current_tls_cipher` | yes | yes | empty string |
| `Current_tls_ciphersuites` | yes | yes | empty string |
| `Current_tls_crl` | yes | yes | empty string |
| `Current_tls_crlpath` | yes | yes | empty string |
| `Current_tls_key` | yes | yes | empty string |
| `Current_tls_version` | yes | yes | empty string |
| `Rsa_public_key` | yes | yes | empty string |
| `Tls_library_version` | yes | yes | empty string |
| `Tls_sni_server_name` | yes | no | empty string |

`Authentication_ldap_sasl_supported_methods` remains omitted because the pinned
MySQL 8.4.9 runtime used for baseline verification does not expose it without
the LDAP SASL plugin.

## Sources

- MySQL 8.4 Reference Manual, `SHOW STATUS`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-status.html>
- MySQL 8.4 Reference Manual, server status variables:
  <https://dev.mysql.com/doc/refman/8.4/en/server-status-variables.html>
- MySQL 8.4.9 runtime probes in
  `packages/libmylite/tests/mysql_baseline_show_status_expectations.sh`.

## Semantics

`SHOW STATUS LIKE 'Compression%'`, `Current_tls%`, and `Tls%`, plus
`SHOW STATUS WHERE Variable_name IN (...)` for the RSA key rows, expose the row
names and scope visibility observed from MySQL 8.4.9.

`sys.metrics` exposes the global-visible rows as lowercase `Global Status`
metrics. Session-only `Compression`, `Compression_algorithm`,
`Compression_level`, and `Tls_sni_server_name` are omitted from `sys.metrics`.

## Unsupported Behavior

This slice does not implement:

- network compression negotiation or compressed packet accounting;
- TLS handshakes, certificate/key loading, SNI tracking, or TLS library
  reporting;
- RSA key generation, loading, or authentication plugin key exchange;
- LDAP SASL authentication plugin status discovery.

The rows are placeholders only and do not imply that MyLite links against a TLS
or authentication library.
