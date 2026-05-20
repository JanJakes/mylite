# Baseline Non-Strict String Truncation

## Summary

This phase extends MyLite's existing descriptor-driven string DML conversion
for `CHAR` and `VARCHAR` targets. It keeps strict nonspace overflow errors, but
adds MySQL-compatible truncation diagnostics for the current ordinary DML
surface:

- non-strict `INSERT`, `REPLACE`, and matched single-table `UPDATE` truncate
  nonspace-overlength `CHAR` / `VARCHAR` string literals and record warning
  `1265`;
- `INSERT IGNORE` over the current `VALUES` and `SET` insert forms truncates
  nonspace-overlength `CHAR` / `VARCHAR` string literals and records warning
  `1265`;
- `VARCHAR` values whose only excess characters are trailing ASCII spaces
  truncate and record note `1265` regardless of strict mode or `IGNORE`;
- `CHAR` values whose only excess characters are trailing ASCII spaces keep
  the existing silent default-mode trimming behavior.

The goal is common WordPress and application DML compatibility without moving
string semantics into SQLite. MyLite descriptors remain the authority for the
declared string length, conversion, diagnostics, and bound physical values.

## Sources And Evidence

- MyLite architecture and standards:
  - `README.md`
  - `AGENTS.md`
  - `docs/architecture/engineering-standards.md`
- Existing MyLite specs:
  - `docs/specs/baseline-char-type/specs.md`
  - `docs/specs/baseline-varchar-type/specs.md`
  - `docs/specs/baseline-nonstrict-dml-coercion/specs.md`
  - `docs/specs/baseline-insert-ignore-lifecycle/specs.md`
  - `docs/specs/baseline-update-lifecycle/specs.md`
- Official MySQL 8.4 Reference Manual:
  - `CHAR` and `VARCHAR`: <https://dev.mysql.com/doc/refman/8.4/en/char.html>
  - Server SQL modes: <https://dev.mysql.com/doc/refman/8.4/en/sql-mode.html>
  - `INSERT`: <https://dev.mysql.com/doc/refman/8.4/en/insert.html>
  - `UPDATE`: <https://dev.mysql.com/doc/refman/8.4/en/update.html>
  - `REPLACE`: <https://dev.mysql.com/doc/refman/8.4/en/replace.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_nonstrict_string_truncation_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

MySQL 8.4.9 establishes these expectations for this slice:

- The default session `sql_mode` includes `STRICT_TRANS_TABLES`.
- Strict nonspace-overlength assignment into `CHAR(n)` or `VARCHAR(n)` fails
  with `1406 / 22001` and leaves the statement rolled back.
- When neither strict mode is active, nonspace-overlength `CHAR` and `VARCHAR`
  string literals are truncated to the declared character length and warning
  `1265 / 01000` is recorded for each affected value occurrence.
- `INSERT IGNORE` under strict mode truncates the same nonspace-overlength
  values and records warning `1265`.
- `REPLACE` follows the same value-conversion behavior as `INSERT`, while its
  delete-then-insert affected-row semantics remain unchanged.
- For `VARCHAR`, values with only excess trailing ASCII spaces are truncated
  and record a `Note` with code `1265`, including under strict mode.
- For `CHAR`, values with only excess trailing ASCII spaces are silently
  trimmed in the default readback mode.
- `UPDATE` records string truncation diagnostics for matched rows. If the
  truncated value equals the current stored value, affected rows remain zero
  but diagnostics are still recorded for matched rows.
- `UPDATE ... LIMIT 0` and updates that match no rows record no conversion
  diagnostics.
- MySQL counts `CHAR` and `VARCHAR` limits in characters, not bytes, under
  `utf8mb4`.

## Scope

This feature supports the string-literal value positions already admitted by
MyLite:

- `INSERT ... VALUES` and `INSERT ... SET` into persistent or shadowing
  temporary base tables;
- `INSERT IGNORE ... VALUES` and `INSERT IGNORE ... SET` for the current
  non-`AUTO_INCREMENT` string target subset;
- `REPLACE ... VALUES` and `REPLACE ... SET`;
- matched single-table `UPDATE ... SET column = string_literal` with the
  current optional `WHERE`, `ORDER BY`, and `LIMIT` support.

The affected target descriptors are limited to:

- `CHAR` and `CHAR(n)` descriptors in the current admitted `0..255` range;
- `VARCHAR(n)` descriptors in the current admitted range.

The admitted value syntax is unchanged: ordinary string literals only, plus the
existing `NULL` and `DEFAULT` paths from prior features. String literal decoding
continues to reject embedded `NUL` bytes and invalid UTF-8 for nonbinary string
targets.

## Non-Goals

This feature does not add:

- `TEXT` family truncation warning demotion;
- `INSERT ... SELECT`, `REPLACE ... SELECT`, duplicate-key update assignment,
  scalar subquery, or `DEFAULT(column_name)` string truncation; current
  duplicate-key update string assignments keep their existing strict
  overlength diagnostics in all SQL modes;
- `UPDATE IGNORE` grammar or runtime behavior;
- string-to-number, string-to-temporal, expression, function, user-variable,
  parameter, hex, bit, introducer, national-string, or adjacent-literal
  conversion;
- general `PAD_CHAR_TO_FULL_LENGTH`, character-set conversion, collation
  comparison, or protocol metadata changes;
- SQLite fork patches.

## Ownership Boundaries

- Public API: unchanged. `mylite_execute()` continues to own public misuse
  validation, statement diagnostics snapshots, result ownership, and cleanup.
- Statement context and diagnostics: own warning/note/error recording and the
  existing non-row result shape. Successful supported DML exposes affected rows
  through the existing public result API and does not return row data.
- Session state: owns the handle-local SQL-mode bitset. String truncation uses
  the active session strictness at execution time.
- Parser/AST: unchanged. The current grammar already admits the supported DML
  string literal value positions and target descriptors.
- Analyzer/planner/runtime: own descriptor resolution, string literal decoding,
  UTF-8 character counting, truncation decisions, MySQL diagnostics, and bound
  SQLite values.
- Catalog: remains authoritative for logical type, declared length,
  nullability, defaults, physical table names, and physical column names. This
  feature does not mutate descriptors, catalog generation, descriptor caches,
  or `sqlite_schema_generation`.
- SQLite physical storage: receives only descriptor-built prepared statements
  with quoted identifiers and bound values. SQLite does not decide MySQL string
  length, strictness, or diagnostics.
- Storage/VFS/file format: unchanged. DML writes only inside the shifted SQLite
  payload and must not touch the `.mylite` preamble.

## Grammar

No new grammar is required. The relevant existing MyLite Lemon-syntax sketch
remains:

```lemon
insert_value ::= STRING_LITERAL.
insert_value ::= NULL.
insert_value ::= DEFAULT.

replace_value ::= insert_value.

update_assignment ::= qualified_identifier EQUAL update_value.
update_value ::= STRING_LITERAL.
update_value ::= NULL.
update_value ::= DEFAULT.
```

Runtime narrows those shapes to the existing table, column, expression,
predicate, ordering, and limit subsets.

## Conversion Semantics

The descriptor-declared `CHAR` / `VARCHAR` length is interpreted as a UTF-8
character limit. Truncation must preserve complete UTF-8 code points.

For `VARCHAR`:

- in-range strings are stored unchanged;
- if all characters beyond the declared limit are ASCII spaces, the string is
  truncated to the declared limit and note `1265 / 01000` is recorded;
- otherwise, non-strict mode or `INSERT IGNORE` truncates to the declared limit
  and records warning `1265 / 01000`;
- otherwise, strict non-`IGNORE` mode raises `1406 / 22001`.

For `CHAR`:

- MyLite stores the default visible value shape, so trailing ASCII spaces are
  removed before binding;
- if the non-trailing-space character count fits the declared limit, conversion
  succeeds silently;
- if non-trailing-space characters exceed the declared limit, non-strict mode
  or `INSERT IGNORE` truncates to the declared limit, trims any trailing ASCII
  spaces from the truncated value, and records warning `1265 / 01000`;
- otherwise, strict non-`IGNORE` mode raises `1406 / 22001`.

`VARCHAR(0)` and `CHAR(0)` follow the same rules with a declared length of
zero. Excess trailing spaces for `VARCHAR(0)` produce a note; any nonspace
content is a strict error or a non-strict/`IGNORE` warning-demoted empty value.
Excess trailing spaces for `CHAR(0)` are silent.

## UPDATE Semantics

`UPDATE` conversion is evaluated only when the statement matches at least one
row after the current `WHERE` and `LIMIT 0` checks. A conversion that succeeds
with truncation is planned once as the bound assignment value. Diagnostics are
then recorded for each matched row that participates in the statement's limit
envelope.

Affected rows remain changed-row counts after canonicalization. Updating a row
from `'abc'` to a value that truncates to `'abc'` records the truncation
diagnostic but reports zero affected rows for that row.

For ties in `ORDER BY ... LIMIT`, this feature does not add or claim a new
tie-breaker. It preserves the existing update-row selection semantics and only
adds conversion diagnostics for the number of matched rows inside the admitted
limit envelope.

## Generated SQLite Handling

No SQLite grammar extension is required. MyLite keeps generating the same
descriptor-driven physical `INSERT`, `REPLACE`, and `UPDATE` SQL:

- physical table and column identifiers are stable catalog names and are always
  quoted;
- string values are bound through prepared statements after MyLite conversion;
- generated SQL never interpolates decoded string bytes;
- row-value storage remains SQLite `TEXT` for admitted `CHAR` and `VARCHAR`
  descriptors;
- statement rollback remains managed by the existing statement transaction
  wrapper, so strict errors leave no partial rows.

This is a MyLite wrapper/runtime conversion feature, not a public SQLite
extension API change and not a targeted SQLite fork hook.

## Diagnostics

Supported diagnostics:

- `1406 / 22001`: strict nonspace-overlength string value for `CHAR` or
  `VARCHAR`;
- warning `1265 / 01000`: non-strict or `INSERT IGNORE` nonspace truncation;
- note `1265 / 01000`: `VARCHAR` excess trailing-space truncation;
- existing parse/unsupported diagnostics for non-string expressions, numeric
  values, functions, parameters, invalid UTF-8, embedded `NUL`, unsupported
  statement shapes, unknown columns, and unsupported table targets;
- existing allocation and SQLite failure diagnostics.

Successful supported in-range updates continue to report `warning_count == 0`.
Successful truncating statements report the exact warning/note count verified
against MySQL 8.4.9.

## Tests

The feature adds MySQL-runtime expectation coverage and fast C runtime tests
for:

- strict nonspace-overlength `INSERT` and `UPDATE` rollback;
- non-strict `INSERT`, `INSERT SET`, `REPLACE`, `REPLACE SET`, and matched
  `UPDATE` truncation for `CHAR` and `VARCHAR`;
- strict and non-strict `VARCHAR` excess trailing-space notes;
- silent `CHAR` excess trailing-space trimming;
- `INSERT IGNORE` overlength demotion for `CHAR` and `VARCHAR`;
- `CHAR(0)` / `VARCHAR(0)` truncation behavior;
- UTF-8 character-boundary truncation;
- update diagnostics with matched rows, no-match updates, `LIMIT 0`, and
  truncation-to-current-value affected-row behavior;
- warning/note counts, `SHOW WARNINGS`, affected rows, absence of row result
  sets, persistence after reopen, and `.mylite` preamble preservation.
