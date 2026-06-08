# Baseline SET Connection Character Set

## Status

This feature originally specified a narrow connection-bootstrap slice for:

- `SET NAMES`
- `SET CHARACTER SET`
- `SET CHARSET`

It builds on the existing fixed `utf8mb4` / `utf8mb4_0900_ai_ci` connection,
server, and database character-set baseline. Later slices broadened the
runtime to track admitted session charset/collation readback values and to
accept comma-separated tail assignments; see
[baseline SET value syntax](../baseline-set-value-syntax/specs.md). This is
still not full character-set conversion or protocol character-set negotiation.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- Baseline character-set system variables:
  `docs/specs/baseline-character-set-system-variables/specs.md`,
  `docs/specs/baseline-server-character-set-system-variables/specs.md`,
  `docs/specs/baseline-database-character-set-system-variables/specs.md`
- Baseline SHOW CHARACTER SET / COLLATION:
  `docs/specs/baseline-show-character-set-collation/specs.md`
- MySQL lexer: `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold: `docs/specs/mysql-parser-scaffold/specs.md`
- MySQL 8.4 Reference Manual, `SET NAMES`:
  https://dev.mysql.com/doc/refman/8.4/en/set-names.html
- MySQL 8.4 Reference Manual, `SET CHARACTER SET`:
  https://dev.mysql.com/doc/refman/8.4/en/set-character-set.html
- MySQL 8.4 Reference Manual, connection character sets and collations:
  https://dev.mysql.com/doc/refman/8.4/en/charset-connection.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

Observed against the local `mysql:8.4.9` runtime with the client invoked using
`--default-character-set=utf8mb4`:

- `SET NAMES utf8mb4`, `SET NAMES DEFAULT`, and
  `SET NAMES utf8mb4 COLLATE utf8mb4_0900_ai_ci` succeed.
- `SET CHARACTER SET utf8mb4`, `SET CHARACTER SET DEFAULT`,
  `SET CHARSET utf8mb4`, and `SET CHARSET DEFAULT` succeed.
- The same names may be string-quoted, backtick-quoted, or written in mixed
  case.
- With the tested server defaults, the supported forms leave
  `@@character_set_client`, `@@character_set_connection`,
  `@@character_set_results`, and `@@collation_connection` at `utf8mb4`,
  `utf8mb4`, `utf8mb4`, and `utf8mb4_0900_ai_ci`.
- Successful statements return no row result set, leave `@@warning_count` and
  `@@error_count` at `0`, and make following `ROW_COUNT()` return `0`.
- `SET NAMES utf8mb4 COLLATE utf8mb4_bin` succeeds in MySQL and changes only
  `@@collation_connection` among the tested connection variables. Later MyLite
  slices admit this readback behavior for supported catalog collations.
- MySQL accepts many other supported client character sets such as `latin1`.
  Later MyLite slices admit focused metadata/readback behavior for additional
  catalog character sets while still deferring conversion semantics.
- `SET NAMES` without a value, `SET NAMES NULL`,
  `SET NAMES utf8mb4, latin1`, and
  `SET CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci` are syntax errors
  with error `1064`, SQLSTATE `42000`.
- `SET NAMES bogus` and `SET CHARACTER SET bogus` fail with error `1115`,
  SQLSTATE `42000`, reporting an unknown character set.
- `SET NAMES utf8mb4 COLLATE bogus` fails with error `1273`, SQLSTATE
  `HY000`, reporting an unknown collation.
- `SET NAMES utf8mb4 COLLATE latin1_swedish_ci` fails with error `1253`,
  SQLSTATE `42000`, because the collation does not belong to `utf8mb4`.
- `SET NAMES ucs2` fails with error `1231`, SQLSTATE `42000`, because MySQL
  cannot use `ucs2` as a client character set.

## Scope

The implementation must add:

- parser and AST support for the admitted `SET NAMES`, `SET CHARACTER SET`,
  and `SET CHARSET` forms;
- option-name parsing for unquoted identifiers, backtick-quoted identifiers,
  and single-quoted string names in this statement only;
- successful no-op execution for:
  - `SET NAMES utf8mb4`
  - `SET NAMES 'utf8mb4'`
  - ``SET NAMES `utf8mb4` ``
  - `SET NAMES DEFAULT`
  - `SET NAMES utf8mb4 COLLATE utf8mb4_0900_ai_ci`
  - `SET CHARACTER SET utf8mb4`
  - `SET CHARACTER SET DEFAULT`
  - `SET CHARSET utf8mb4`
  - `SET CHARSET DEFAULT`
- ASCII case-insensitive matching for unquoted and quoted admitted names;
- result behavior following MySQL's non-result statement convention for this
  slice: no columns, no rows, affected rows `0`, warning count `0`, and
  following `ROW_COUNT()` value `0`;
- deterministic diagnostics for missing values, unsupported character sets,
  unsupported collations, invalid charset/collation combinations, unsupported
  clauses, and unsupported general `SET` assignment syntax;
- C tests, a MySQL 8.4.9 expectation artifact, and compatibility docs.

## Non-Goals

This feature must not implement:

- general `SET` variable assignment beyond the later documented tail-assignment
  slice;
- mutable `@@character_set_client`, `@@character_set_connection`,
  `@@character_set_results`, or `@@collation_connection` state beyond
  documented session readback;
- `SET NAMES ... COLLATE DEFAULT`;
- `SET character_set_results = NULL`, persisted variables, global variables,
  or `SET_VAR` optimizer hints;
- character-set conversion, string literal introducers, string column types,
  binary/text/blob data, collation coercibility, string comparison behavior, or
  protocol character-set metadata;
- warnings for deprecated aliases such as `utf8`;
- catalog mutations, storage mutations, SQLite SQL execution, arbitrary SQLite
  pass-through, or SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns validation,
  parse/execution orchestration, result ownership, row-count state, diagnostics
  snapshot replacement, and failure cleanup.
- Statement context owns statement-boundary diagnostics and previous
  `ROW_COUNT()` state.
- Lexer/parser/AST own admitting the fixed statement grammar and preserving
  source spans. They remain independent from runtime, catalog, storage, and
  SQLite.
- Runtime execution owns validating option names against the fixed MyLite
  `utf8mb4` / `utf8mb4_0900_ai_ci` baseline and building an empty
  non-row result.
- Session state remains fixed. This feature reads the current constants but
  does not mutate charset/collation fields because every admitted form is
  semantically idempotent in the current baseline.
- Catalog, result metadata for row queries, storage, VFS, and SQLite physical
  row storage are not involved. The `.mylite` preamble and shifted SQLite
  payload must remain untouched.

## Supported SQL Grammar

Supported independent subset:

```sql
SET NAMES charset_name
SET NAMES charset_name COLLATE collation_name
SET NAMES DEFAULT
SET CHARACTER SET charset_name
SET CHARACTER SET DEFAULT
SET CHARSET charset_name
SET CHARSET DEFAULT
```

`charset_name` and `collation_name` in this slice are one option-name atom:
an identifier, backtick-quoted identifier, or single-quoted string literal.

MyLite Lemon-syntax snippet:

```lemon
statement ::= set_connection_charset_statement.

set_connection_charset_statement ::= SET NAMES set_names_target.
set_connection_charset_statement ::= SET CHARACTER SET set_character_set_target.
set_connection_charset_statement ::= SET CHARSET set_character_set_target.

set_names_target ::= DEFAULT.
set_names_target ::= option_name set_names_collate_opt.

set_names_collate_opt ::= .
set_names_collate_opt ::= COLLATE option_name.

set_character_set_target ::= DEFAULT.
set_character_set_target ::= option_name.

option_name ::= identifier.
option_name ::= STRING.
```

The original grammar deliberately did not admit comma-separated assignments,
`COLLATE` after `SET CHARACTER SET`, global/session scopes, parameters,
expressions, functions, introducers, numeric literals, `NULL`, or multiple
`SET` clauses. Comma-separated tail assignments after admitted connection
character-set statements are specified by the later baseline SET value syntax
slice.

## Runtime Semantics

Successful statements are no-ops over the current fixed connection baseline:

| Statement family | Accepted values | Visible effect |
| --- | --- | --- |
| `SET NAMES` | `utf8mb4`, `DEFAULT` | Connection variables remain `utf8mb4`, `utf8mb4`, `utf8mb4`, `utf8mb4_0900_ai_ci` |
| `SET NAMES ... COLLATE` | `utf8mb4_0900_ai_ci` only | Same fixed values |
| `SET CHARACTER SET` / `SET CHARSET` | `utf8mb4`, `DEFAULT` | Same fixed values |

`DEFAULT` maps to MyLite's fixed embedded defaults, currently `utf8mb4` and
`utf8mb4_0900_ai_ci`.

Successful statements:

- return a non-row result object with zero columns and zero rows;
- report `affected_rows == 0`;
- report `warning_count == 0`;
- make following `ROW_COUNT()` return `0`;
- clear previous diagnostics like other successful nondiagnostic statements;
- do not change selected schema, catalog generation, descriptor caches,
  SQLite schema generation, or physical storage.

## Diagnostics

Diagnostics are deterministic and intentionally narrow:

- syntax errors use the existing MySQL-style parse error `1064`, SQLSTATE
  `42000`;
- unsupported character-set names use MySQL error `1115`, SQLSTATE `42000`,
  with `Unknown character set: '<name>'`;
- unsupported collation names use MySQL error `1273`, SQLSTATE `HY000`, with
  `Unknown collation: '<name>'`;
- a known but intentionally deferred non-`utf8mb4` character set may be
  reported as unsupported through the same unknown-character-set diagnostic in
  this slice;
- a collation that is known to be invalid for `utf8mb4` may use MySQL error
  `1253`, SQLSTATE `42000`, when MyLite recognizes the mismatch;
- allocation failures use the existing `MYLITE_NOMEM` path;
- public API misuse remains unchanged.

The runtime must validate before producing a successful result, and failed
statements must not mutate session, catalog, or storage state.

## Physical SQLite Handling

No SQLite SQL is generated for this feature. The statement is handled entirely
inside MyLite runtime state. It does not prepare SQLite statements, inspect
SQLite schema text, register SQLite functions or collations, touch row data,
or require a SQLite fork patch.

## Tests

Add a focused runtime test under `packages/libmylite/tests/`, plus parser
coverage in the existing parser test.

Required coverage:

- successful `SET NAMES` with unquoted, string-quoted, backtick-quoted,
  mixed-case, `DEFAULT`, and explicit default `COLLATE`;
- successful `SET CHARACTER SET`, `SET CHARSET`, and `DEFAULT`;
- values of `@@character_set_client`, `@@character_set_connection`,
  `@@character_set_results`, and `@@collation_connection` after successful
  statements;
- result shape, affected rows, warning count, and following `ROW_COUNT()`;
- previous diagnostics clearing;
- close/reopen and independent-handle behavior;
- `.mylite` preamble preservation and no SQLite schema-generation change;
- syntax rejections for missing values, `NULL`, comma-separated forms,
  `COLLATE` after `SET CHARACTER SET`, general variable assignments, and
  expression/parameter/function values;
- deterministic diagnostics for unsupported character sets and collations,
  including `bogus`, `latin1`, `ucs2`, `utf8mb4_bin`, and
  `latin1_swedish_ci` where relevant;
- existing lexer, parser, scalar system-variable, character-set/collation,
  runtime lifecycle, file-format, and check workflows still pass.

## Compatibility Documentation

Update `COMPATIBILITY.md`, `docs/compatibility/sql-set-statements.md`, and
runtime/character-set detail docs only for the exact fixed subset. Do not
claim full `SET`, mutable character-set variables, arbitrary character sets,
arbitrary collations, string conversion, protocol charset negotiation, or
collation comparison semantics.
