# Baseline STATEMENT_DIGEST function boundary

## Scope

This slice recognizes MySQL's native `STATEMENT_DIGEST(statement)` function and
implements the parts MyLite can support without guessing MySQL's private digest
hash input:

- native one-argument lookup and arity diagnostics
- `NULL` argument result
- scalar, `DO`, and row-backed expression contexts for the supported `NULL`
  path
- `CHARSET()`, `COLLATION()`, `COERCIBILITY()`, and `COLLATE` metadata behavior
- MySQL-shaped result metadata
- deterministic unsupported diagnostics for non-`NULL` digest hash computation

It does not implement the 64-character non-`NULL` digest hash. MySQL 8.4.9 does
not hash the displayed `STATEMENT_DIGEST_TEXT()` value directly. Runtime
observation for `select 1` returns:

- `STATEMENT_DIGEST('select 1')`:
  `d1b44b0c19af710b5a679907e284acd2ddc285201794bc69a2389d77baedddae`
- `STATEMENT_DIGEST_TEXT('select 1')`: `SELECT ?`
- `SHA2(STATEMENT_DIGEST_TEXT('select 1'), 256)`:
  `66cbb3a40d4bbd150b75825ad291a6545399f3098fc1079e4d8b5bb061a6a481`

The mismatch means MyLite must not fabricate a hash from the displayed digest
text. Full hash support requires an independently specified and verified digest
token representation.

Primary references:

- MySQL 8.4 Reference Manual, Performance Schema statement summary tables:
  `https://dev.mysql.com/doc/refman/8.4/en/performance-schema-statement-summary-tables.html`
- MySQL 8.4 Reference Manual, MySQL Enterprise Firewall reference:
  `https://dev.mysql.com/doc/refman/8.4/en/firewall-reference.html`
- MySQL 8.4.9 runtime probes recorded in
  `packages/libmylite/tests/mysql_baseline_statement_digest_function_expectations.sh`

## Syntax

The parser continues to accept `STATEMENT_DIGEST` through the generic function
surface. Runtime sys-function lookup recognizes the unqualified native function:

```lemon
scalar_expression ::= generic_function_name LP expression RP.
generic_function_name ::= IDENTIFIER.
```

`STATEMENT_DIGEST` is not exposed as `sys.STATEMENT_DIGEST`.

## Semantics

`STATEMENT_DIGEST(NULL)` returns `NULL`.

`DO STATEMENT_DIGEST(NULL)` succeeds, reports zero affected rows, and appends no
warnings.

`CHARSET(STATEMENT_DIGEST(expr))` returns the connection character set.
`COLLATION(STATEMENT_DIGEST(expr))` returns the connection collation.
`COERCIBILITY(STATEMENT_DIGEST(expr))` returns `4`.

For any non-`NULL` argument, MyLite returns:

- error code `1064`
- SQLSTATE `42000`
- message containing `STATEMENT_DIGEST() hash computation is not yet supported`

This diagnostic is intentionally different from MySQL's successful hash result.
It keeps applications from observing a wrong digest while allowing syntax,
metadata, and `NULL` compatibility paths to work.

## Diagnostics

Wrong argument counts return:

- error code `1582`
- SQLSTATE `42000`
- message containing
  `Incorrect parameter count in the call to native function 'STATEMENT_DIGEST'`

The same arity diagnostic is preserved inside metadata wrappers, for example
`CHARSET(STATEMENT_DIGEST())`.

## Metadata

MySQL 8.4.9 reports `STATEMENT_DIGEST(NULL)` as:

- protocol type `VAR_STRING`
- connection collation, such as `utf8mb4_0900_ai_ci`
- display length `64 * max_bytes_per_character`
- decimals `31`
- nullable
- no flags

MyLite uses the same descriptor in scalar and row-backed projection metadata.

## MyLite Decisions

This slice is implemented with MyLite's native-function registry and SQLite UDF
callback path. No SQLite fork hook is needed: SQLite only needs to call the
registered scalar function, and the compatibility decision belongs in MyLite's
runtime layer.

The compatibility row is white, not green. The accepted surface prevents parser
and metadata failures for applications that inspect the function, but the core
non-`NULL` hash value remains unsupported until it can be specified and tested
without copying or guessing MySQL internals.
