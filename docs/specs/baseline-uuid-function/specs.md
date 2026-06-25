# Baseline UUID Functions

## Status

Implemented for `baseline-uuid-function`, including `UUID()` and the
`UUID_SHORT()` baseline.

## Source Evidence

- Official MySQL 8.4 Reference Manual, miscellaneous functions:
  <https://dev.mysql.com/doc/refman/8.4/en/miscellaneous-functions.html>
- MySQL 8.4.9 runtime probes recorded in
  `packages/libmylite/tests/mysql_baseline_uuid_function_expectations.sh`.

Observed MySQL 8.4.9 behavior for this slice:

- `UUID()` returns a 36-byte lowercase string in
  `aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee` form.
- The high nibble of the third group is `1`, matching UUID version 1 output.
- The high bits of the fourth group match the RFC 4122 variant; the visible
  first character is one of `8`, `9`, `a`, or `b`.
- Two calls in the same statement normally return different values.
- `CHARSET(UUID())` returns `utf8mb3`, `COLLATION(UUID())` returns
  `utf8mb3_general_ci`, and `COERCIBILITY(UUID())` returns `4`.
- `DO UUID()` succeeds with `ROW_COUNT() = 0` and warning count `0`.
- `SELECT UUID` without parentheses resolves as an identifier and fails as an
  unknown column when no such column exists.
- `CREATE TABLE uuid (uuid INT)` succeeds; `UUID` is not reserved.
- `UUID(NULL)` and `UUID(1, 2)` fail with native-function parameter-count error
  `1582 / 42000`.
- `UUID_SHORT()` returns a nonzero unsigned integer exposed through protocol
  metadata as `LONGLONG`, binary collation id `63`, display length `21`, flags
  `NOT_NULL UNSIGNED BINARY NUM`, decimals `0`, and nullable false.
- Consecutive `UUID_SHORT()` calls in one statement increment by one.
- `CHARSET(UUID_SHORT())` and `COLLATION(UUID_SHORT())` return `binary`;
  `COERCIBILITY(UUID_SHORT())` returns `5`.
- `UUID_SHORT(NULL)` and `UUID_SHORT(1, 2)` fail with native-function
  parameter-count error `1582 / 42000`.
- Bare `UUID_SHORT` resolves as an identifier and is not reserved.

## Scope

This slice supports:

- no-source and `FROM DUAL` scalar `SELECT UUID()`;
- table-backed single-source row-scalar `SELECT UUID() FROM table`;
- `DO UUID()`;
- no-source and `FROM DUAL` scalar `SELECT UUID_SHORT()`;
- table-backed single-source row-scalar `SELECT UUID_SHORT() FROM table`;
- `DO UUID_SHORT()`;
- `UUID()` as an argument where current scalar function plumbing already admits
  session scalar values, including `IS_UUID(UUID())`, string length functions,
  `CONCAT()`, `CHARSET()`, `COLLATION()`, and `COERCIBILITY()`;
- `UUID_SHORT()` in supported numeric and metadata scalar contexts, including
  arithmetic argument plumbing, `CHARSET()`, `COLLATION()`, and
  `COERCIBILITY()`;
- bare `UUID` and `UUID_SHORT` as ordinary identifiers, not function calls;
- MySQL-shaped arity diagnostics for any nonzero argument count admitted by the
  parser.

This slice does not support:

- use of `UUID()` in DML assignments, defaults, generated columns, check
  constraints, indexes, predicates, grouping, ordering, joins, subqueries other
  than already supported scalar plumbing, prepared parameters, or stored
  programs unless a separate feature explicitly admits that context;
- cross-process or cross-host uniqueness for `UUID_SHORT()`;
- MySQL server-id configuration, real server startup-time semantics, replication
  topology, or overflow behavior for `UUID_SHORT()`;
- scalar comparison/IS expressions such as `UUID_SHORT() < UUID_SHORT()` and
  `UUID_SHORT() IS NULL` until the broader scalar comparison/IS layer admits
  function operands;
- loaded server variables that would make `UUID_SHORT()` values match a MySQL
  instance byte-for-byte;
- replication safety warnings or binary-log behavior;
- exposing or depending on a host MAC address;
- exact byte-for-byte matching of MySQL timestamp, clock sequence, or node
  values.

## Grammar

MyLite grammar is independently authored from observed behavior:

```lemon
expression(A) ::= UUID(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_zero_argument_function(
        state, T, MYLITE_SQL_AST_UUID_FUNCTION, R);
}

expression(A) ::= UUID(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_UUID_ARGUMENT_COUNT_ERROR, B, R);
}

identifier(A) ::= UUID(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}

expression(A) ::= generic_function_call(B). {
    /* generic builder specializes a case-insensitive UUID_SHORT name with
       zero arguments to MYLITE_SQL_AST_UUID_SHORT_FUNCTION. */
    A = B;
}
```

`UUID` is a nonreserved keyword. The lexer recognizes it so the parser can
distinguish `UUID()` from an identifier, but identifier grammar preserves
`CREATE TABLE uuid (uuid INT)` and `SELECT UUID` behavior.

`UUID_SHORT` is not a keyword. MyLite recognizes it inside generic function-call
construction by name, preserving ordinary identifier behavior for
`CREATE TABLE uuid_short (uuid_short INT)` and `SELECT UUID_SHORT`.

## Runtime Semantics

`UUID()` generates a MyLite-owned RFC 4122 version 1-shaped UUID string:

- canonical text length is 36 bytes plus a terminating NUL in internal buffers;
- output uses lowercase hexadecimal digits and dashes in the standard positions;
- the version nibble is `1`;
- the variant bits are the RFC 4122 `10` bit pattern;
- the timestamp is built from current UTC time in 100ns units with the UUID
  epoch offset;
- each handle keeps session-local generator state to make repeated calls
  monotonic even if multiple calls occur within one clock tick;
- the node is a 48-bit random value generated through SQLite's public
  `sqlite3_randomness()` API; MyLite sets the multicast bit to avoid presenting
  it as a real hardware address;
- warning count is `0` for successful generation.

MyLite does not promise global ordering, cryptographic unpredictability, real
hardware identity, or MySQL-identical time/node fields. It promises the visible
MySQL-compatible function shape above.

`UUID_SHORT()` generates a MyLite-owned unsigned 64-bit value shaped like
MySQL's documented component layout:

- an embedded server-id component in the high 8 bits, currently fixed to `1`;
- the connection's UUID_SHORT generator initialization time in seconds in the
  next 32 bits;
- a per-handle monotonically increasing 24-bit-position counter in the low bits.

The counter is stored in session state and increments on every successful
`UUID_SHORT()` evaluation. MyLite keeps the value below signed `sqlite3_int64`
range for the fixed embedded server-id component used here, so the private
SQLite UDF can return it as an integer while result metadata still reports an
unsigned MySQL `LONGLONG`.

MyLite does not claim MySQL's cross-server uniqueness contract. Embedded users
that need durable, cluster-wide identity should use application-provided keys or
`UUID()` until MyLite has a designed server-id/runtime identity model.

## SQLite Handling

No SQLite fork patch is needed. MyLite owns UUID generation in
`mylite_uuid.c`.

No-source scalar statements call the MyLite runtime helper directly. Table-backed
row-scalar statements lower `UUID()` and `UUID_SHORT()` to private SQLite scalar
functions `_mylite_uuid()` and `_mylite_uuid_short()` so SQLite evaluates them
once per output row. This keeps query execution close to SQLite and avoids
materializing a table in MyLite just to decorate rows.

SQLite generated SQL never embeds generated UUID values for row-backed
execution. It calls the registered function by stable private name. No physical
schema, catalog descriptor, `.mylite` preamble, VFS, storage format, or SQLite
schema generation state changes are made by evaluating `UUID()` or
`UUID_SHORT()`.

## Result Metadata

Scalar `UUID()` result metadata is:

- logical type: `VAR_STRING`;
- display length: `36`;
- character set/collation id: `utf8mb3_general_ci` (`33`);
- nullable: true. MySQL 8.4.9 does not mark `UUID()` protocol metadata with
  `NOT_NULL`, even though successful evaluations return a UUID string.

`CHARSET(UUID())`, `COLLATION(UUID())`, and `COERCIBILITY(UUID())` use
MyLite-owned scalar metadata and return `utf8mb3`, `utf8mb3_general_ci`, and
`4`, respectively.

Successful `SELECT` returns a row result. Successful `DO UUID()` returns the
existing non-row statement result convention with zero columns, zero rows,
`affected_rows = 0`, and warning count `0`.

Scalar `UUID_SHORT()` result metadata is:

- logical type: `LONGLONG`;
- display length: `21`;
- character set/collation id: binary (`63`);
- flags: `NOT_NULL`, `UNSIGNED`, `BINARY`, and `NUM`;
- nullable: false;
- decimals: `0`.

`CHARSET(UUID_SHORT())`, `COLLATION(UUID_SHORT())`, and
`COERCIBILITY(UUID_SHORT())` use MyLite-owned scalar metadata and return
`binary`, `binary`, and `5`, respectively. Successful `DO UUID_SHORT()` follows
the same non-row statement result convention as `DO UUID()`.

## Diagnostics

- Syntax errors remain normal parser diagnostics.
- `UUID()` with any argument count other than zero returns MySQL native function
  arity error `1582 / 42000`.
- `UUID_SHORT()` with any argument count other than zero returns MySQL native
  function arity error `1582 / 42000`.
- Bare `UUID` and `UUID_SHORT` without parentheses resolve as identifiers and,
  outside a matching table column scope, return the existing unknown-column
  diagnostic.
- Unsupported contexts return deterministic MyLite unsupported-feature
  diagnostics until those contexts are explicitly designed.
- Allocation failures return `MYLITE_NOMEM` and set the existing out-of-memory
  diagnostic.
- Unexpected SQLite UDF failures surface as runtime errors on the owning handle.

## Tests

The implementation must add:

- MySQL expectation script:
  `packages/libmylite/tests/mysql_baseline_uuid_function_expectations.sh`.
- Parser coverage for `UUID()`, spaced calls, argument-count errors, and bare
  identifier behavior.
- Runtime C coverage for:
  - shape, length, version, variant, uniqueness, warning count, and affected
    rows;
  - `FROM DUAL`, table-backed per-row values, and `DO UUID()`;
  - `IS_UUID(UUID())`, string length, `CONCAT()`, charset, collation, and
    coercibility scalar composition;
  - arity diagnostics and bare identifier unknown-column behavior;
  - `UUID_SHORT()` numeric metadata, monotonic values, `FROM DUAL`, `DO`,
    table-backed per-row values, scalar metadata functions, arity diagnostics,
    and identifier compatibility;
  - file preamble and catalog/schema-generation invariants;
  - close/reopen and independent handle state.

Verification before completion:

```sh
cmake --build --preset dev
ctest --preset dev -R 'libmylite\.(parser|lexer|runtime\.(uuid_function|uuid_conversion_functions|rand_function|charset_collation_functions|coercibility_function|string_length_functions))$' --output-on-failure
./packages/libmylite/tests/mysql_baseline_uuid_function_expectations.sh
cmake --workflow --preset check
```
