# Baseline STATEMENT_DIGEST_TEXT function

## Scope

This slice implements the MySQL native `STATEMENT_DIGEST_TEXT(statement)`
function for statement text that MyLite can parse with its current MySQL
compatibility grammar. The function is useful for applications that inspect
Performance Schema-style statement digests or reuse the digest normalization
surface exposed by MySQL Enterprise Firewall.

`STATEMENT_DIGEST()` is intentionally not included in this support claim. MySQL
8.4.9 returns a 64-character digest hash whose input is not the displayed
`STATEMENT_DIGEST_TEXT()` value, as verified by comparing
`STATEMENT_DIGEST('select 1')` with `SHA2(STATEMENT_DIGEST_TEXT('select 1'),
256)`. MyLite must not guess that hash format.

Primary references:

- MySQL 8.4 Reference Manual, Performance Schema statement summary tables:
  `https://dev.mysql.com/doc/refman/8.4/en/performance-schema-statement-summary-tables.html`
- MySQL 8.4 Reference Manual, MySQL Enterprise Firewall reference:
  `https://dev.mysql.com/doc/refman/8.4/en/firewall-reference.html`
- MySQL 8.4.9 runtime probes recorded in
  `packages/libmylite/tests/mysql_baseline_statement_digest_text_function_expectations.sh`

## Behavior

`STATEMENT_DIGEST_TEXT(expr)` evaluates `expr` as a string containing one SQL
statement and returns MySQL-style normalized digest text:

- SQL keywords are rendered in uppercase.
- String, numeric, hexadecimal, bit, and ordinary `NULL` literal values are
  replaced with `?`; `IS NULL` and `IS NOT NULL` keep the syntactic `NULL`.
- Unquoted and quoted identifiers are rendered with MySQL backtick quoting.
- `<>` is normalized to `!=`.
- Comments are ignored.
- `IN` value lists are collapsed to `(...)`.
- A trailing statement terminator is preserved as `;`.

The function returns `NULL` when its argument is `NULL`.

The argument must parse as a single MyLite-supported SQL statement. Invalid SQL,
parameter markers inside the argument text, and multi-statement text return
MySQL-shaped digest parse diagnostics.

## Diagnostics

Wrong argument counts return:

- error code `1582`
- SQLSTATE `42000`
- message containing
  `Incorrect parameter count in the call to native function 'STATEMENT_DIGEST_TEXT'`

Invalid digest input returns:

- error code `3676`
- SQLSTATE `HY000`
- message beginning `Could not parse argument to digest function`

## Metadata

MySQL 8.4.9 reports `STATEMENT_DIGEST_TEXT()` as:

- protocol type `LONG_BLOB`
- connection collation, such as `utf8mb4_0900_ai_ci`
- display length `67108864 * max_bytes_per_character`
- decimals `31`
- nullable
- no flags

`CHARSET()` and `COLLATION()` return the connection character set and collation.
`COERCIBILITY()` returns `4`.

## MyLite Decisions

The implementation uses MyLite's parser as the acceptance gate and a small
token normalizer for rendering. It does not execute the statement or load table
data, and it does not create an independent SQL semantic engine.

The row is green in the baseline for the documented MyLite grammar subset. Full
normalization for every MySQL grammar construct remains tied to parser grammar
coverage, and `STATEMENT_DIGEST()` remains unsupported until the exact MySQL
hash input is independently specified.
