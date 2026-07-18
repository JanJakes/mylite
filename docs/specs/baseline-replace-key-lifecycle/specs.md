# Baseline Replace Key Lifecycle

## Status

This feature extends the existing no-key `REPLACE ... VALUES` and
`REPLACE ... SET` paths to key-bearing persistent base tables. The parser
surface already exists; this slice adds descriptor-driven duplicate-key
replacement semantics for the current primary-key and unique-index descriptor
subset.

`REPLACE ... SELECT` remains no-key-only in this slice. Its later key-bearing
source materialization, generated-value, and same-table-read behavior is
specified separately in
`docs/specs/baseline-replace-select-keyed-targets/specs.md`.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing no-key replace specs:
  `docs/specs/baseline-replace-values-lifecycle/specs.md`,
  `docs/specs/baseline-replace-set-lifecycle/specs.md`, and
  `docs/specs/baseline-replace-select-lifecycle/specs.md`
- Current primary-key, unique-index, prefix-index, foreign-key, and
  auto-increment specs under `docs/specs/`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `REPLACE`:
  https://dev.mysql.com/doc/refman/8.4/en/replace.html
- MySQL 8.4 Reference Manual, `ROW_COUNT()`:
  https://dev.mysql.com/doc/refman/8.4/en/information-functions.html
- MySQL 8.4 Reference Manual, foreign-key constraints:
  https://dev.mysql.com/doc/refman/8.4/en/create-table-foreign-keys.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_replace_key_lifecycle_expectations.sh`
records the runtime probes for this feature. Observed behavior:

- On no-key tables, `REPLACE` remains insert-equivalent.
- On a primary-key conflict where at least one stored value changes,
  single-row `REPLACE` reports `ROW_COUNT() == 2`, stores one final row, and
  leaves `@@warning_count == 0`.
- On a primary-key conflict where the replacement row equals the old row across
  every stored column, `ROW_COUNT() == 1` and `@@warning_count == 0`.
- `REPLACE ... SET` follows the same key-conflict affected-row behavior as
  `REPLACE ... VALUES`.
- A row that conflicts with two different existing rows through two unique
  keys deletes both old rows, inserts the new row, and reports
  `ROW_COUNT() == 3`.
- Composite primary keys and current string or prefix unique indexes participate
  in replacement.
- `NULL` in a unique-key part does not conflict; repeated nullable unique
  `NULL` rows are inserted independently.
- Generated `AUTO_INCREMENT` values are reused after deleting the conflicting
  row and become the session `LAST_INSERT_ID()` only when the successful
  replacement row used a generated value. Explicit auto-increment replacement
  rows do not overwrite the previous `LAST_INSERT_ID()`.
- A replacement that would delete a parent row referenced by a supported
  foreign key fails with error `1451`, SQLSTATE `23000`, even when the new row
  is identical to the old row.
- A replacement in a child table fails with error `1452`, SQLSTATE `23000`, if
  the inserted replacement row has no matching parent key.

Official MySQL documentation describes the operational shape as an insert
attempt followed by repeated delete-and-retry while duplicate primary or unique
keys are found, and documents `ROW_COUNT()` as one for added rows and greater
than one for replacements. MyLite follows the verified 8.4.9 runtime edge case
where an exactly equal replacement reports one affected row.

## Scope

The implementation must add:

- key-bearing replacement for `REPLACE [LOW_PRIORITY | DELAYED] [INTO] table
  [(columns)] VALUES ...`;
- key-bearing replacement for `REPLACE [LOW_PRIORITY | DELAYED] [INTO] table
  SET column = value[, ...]`;
- descriptor-driven duplicate lookup through the current primary-key and
  unique-index descriptors, including composite keys and admitted string prefix
  unique keys;
- `NULL`-part unique-key behavior that skips conflict detection for that key;
- repeated conflict deletion when one replacement row conflicts with different
  existing rows through different unique keys;
- MySQL-compatible affected rows for inserted rows, changed replacements,
  exact no-op replacements, and multi-conflict replacements;
- existing `AUTO_INCREMENT` generation, retry, counter, and
  `LAST_INSERT_ID()` behavior for generated and explicit replacement rows;
- child-side and parent-side foreign-key checks for the current supported
  foreign-key descriptor subset;
- all existing descriptor conversion, default, nullability, range, temporal,
  string, binary, `BIT`, `ENUM`, `SET`, and JSON behavior already admitted by
  the no-key `REPLACE ... VALUES` and `REPLACE ... SET` paths.

## Non-Goals

This feature must not implement:

- key-bearing `REPLACE ... SELECT`;
- table aliases, partitions, `RETURNING`, `TABLE`, CTEs, joined replacement,
  subqueries, arbitrary expression values, `DEFAULT(col_name)`, or general
  SQLite pass-through beyond the existing no-key replace grammar;
- cascades, triggers, generated columns, privileges, statement-based
  replication semantics, or client changed-column protocol metadata;
- complete UCA 9.0 string-key semantics beyond the current shared collation subset;
- new SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. Successful replacement returns through the
  existing non-row result conventions with `column_count == 0`, `row_count == 0`,
  affected rows set to the MySQL-compatible replacement count, and supported
  in-range warnings counted normally.
- Statement context owns diagnostics reset, warning count, affected rows,
  `ROW_COUNT()`, transaction completion, and rollback on failure.
- Parser and AST code already own the admitted `REPLACE ... VALUES` and
  `REPLACE ... SET` syntax. No new grammar should be needed for this slice.
- Analyzer/planner code resolves schemas, tables, columns, defaults,
  auto-increment values, foreign keys, and key descriptors through MyLite
  catalog descriptors. SQLite metadata is not authority.
- Runtime execution owns conflict handling: attempt the descriptor-built
  physical insert, map SQLite unique-constraint failures back to MyLite
  descriptors, delete the conflicting physical row through descriptor key
  predicates, validate parent-side foreign keys before retrying, and retry
  until the row inserts or a real error occurs.
- Catalog owns logical descriptors and auto-increment counters. Key-bearing
  replacement must not mutate table descriptors, descriptor versions, catalog
  generation, or `sqlite_schema_generation`, except for legitimate
  auto-increment counter advancement.
- SQLite owns physical row storage and rollback durability. MyLite generates
  quoted physical SQL against stable names such as `_mylite_user_table_<id>`
  and binds all row and key values as parameters.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  Replacement writes occur only inside the shifted SQLite payload.

## Supported Grammar

This slice does not add grammar. It extends the semantics of the existing
limited forms:

```sql
REPLACE [LOW_PRIORITY | DELAYED] [INTO] table_name
    [(column_name[, column_name] ...)]
    VALUES row_value_list

REPLACE [LOW_PRIORITY | DELAYED] [INTO] table_name
    SET column_name = value[, column_name = value ...]
```

The admitted values, column-list behavior, assignment-list behavior, optional
`VALUE` synonym, `ROW(...)` handling, and modifier behavior remain exactly the
current `REPLACE ... VALUES` / `REPLACE ... SET` subset.

### MyLite Lemon-Syntax Snippet

No new Lemon productions are required. The existing replace productions remain
the intended grammar surface for this feature:

```lemon
replace_values_statement(A) ::=
    REPLACE(R) replace_modifier_opt(M) INTO table_name(T)
    insert_column_list_opt(C) insert_values_keyword(VK) insert_row_list(V). {
    A = mylite_sql_parser_make_replace_values_statement(state, R, M, T, C, VK, V);
}

replace_set_statement(A) ::=
    REPLACE(R) replace_modifier_opt(M) INTO table_name(T) SET insert_assignment_list(S). {
    A = mylite_sql_parser_make_replace_set_statement(state, R, M, T, S);
}
```

The implementation must not broaden the parser while enabling key-bearing
replacement.

## Replacement Semantics

For each planned row:

1. Build the physical `INSERT` from MyLite descriptors, quote identifiers, and
   bind all values.
2. If the insert succeeds, count one affected row and apply existing generated
   auto-increment `LAST_INSERT_ID()` behavior.
3. If SQLite reports a unique constraint failure, locate the conflicting
   MyLite key by probing the current unique key descriptors with the planned
   row values. Keys with any `NULL` replacement part do not conflict.
4. Compare the conflicting physical row with the planned replacement row across
   all descriptor columns. If all physical values are equal, remember that this
   is an exact replacement for affected-row accounting.
5. Delete the conflicting row using a generated descriptor key predicate:
   `DELETE FROM <physical_table> WHERE key_part_1 = ? [AND ...]`. Prefix key
   parts use the same descriptor expression and parameter expression as current
   unique-key lookup.
6. Immediately validate parent-side foreign keys for the target table. If
   deleting the old row would leave a referencing child row without a parent,
   fail with the current MySQL-compatible parent-row diagnostic and roll back
   the statement.
7. Retry the insert. A changed replacement counts deleted rows plus the
   successful inserted row. An exact replacement counts only the successful
   inserted row, matching observed MySQL 8.4.9 behavior.
8. If another unique key conflicts with a different existing row, repeat the
   delete-and-retry loop. The loop is bounded by the number of loaded unique
   descriptors for the target table.

After all rows are processed, existing child-side foreign-key validation runs
for the target table so inserted replacement rows with missing parent keys fail
and roll back the statement.

## Physical SQLite Handling

MyLite continues to generate standard SQLite statements on the MyLite-owned
physical table:

```sql
INSERT INTO "_mylite_user_table_<id>" ("col1", "col2", ...)
VALUES (?1, ?2, ...)

DELETE FROM "_mylite_user_table_<id>"
WHERE <descriptor-key-part-1> = ?1 [AND ...]
```

All physical identifiers are quoted. All values are bound parameters. SQLite
schema text, `PRAGMA` metadata, trigger behavior, cascades, and SQLite
`INSERT OR REPLACE` are not used as MySQL compatibility authority. This slice
uses MyLite wrapper/translation on top of public SQLite prepared statements and
does not need a SQLite fork hook.

Generated MyLite user tables remain rowid tables, but this feature does not
depend on rowid ordering or expose rowid behavior.

## Diagnostics

Existing no-key `REPLACE` diagnostics remain unchanged for syntax errors,
missing default schema, unknown schema, unknown table, reserved `_mylite_*`
names, unsupported object kinds, duplicate target columns, unknown target
columns, column-count mismatches, unsupported values, conversion failures,
integer/decimal/temporal/string/binary/`BIT` range failures, `NULL` into
`NOT NULL`, required-column failures, allocation failures, physical SQLite
failures, and public API misuse.

New key-bearing behavior must:

- stop returning `REPLACE into primary-key tables is not supported` and
  `REPLACE into unique-index tables is not supported` for `VALUES` and `SET`;
- keep `REPLACE ... SELECT` key-bearing behavior outside this slice;
- report duplicate-key diagnostics only for impossible stale-descriptor or
  physical failures after MyLite cannot map a unique constraint to a descriptor
  conflict;
- report parent-side foreign-key failures as error `1451`, SQLSTATE `23000`;
- report child-side foreign-key failures as error `1452`, SQLSTATE `23000`;
- preserve warning `3005` for admitted `DELAYED` replacements and zero warnings
  for supported in-range non-delayed replacements.

## Tests

Add MySQL-runtime and MyLite tests covering:

- no-key behavior remains insert-equivalent;
- primary-key changed replacement and exact no-op replacement affected rows;
- `REPLACE ... SET` key-bearing changed and exact no-op replacement;
- multi-row replacement where a later row replaces an earlier row in the same
  statement;
- replacement through a unique secondary key;
- replacement that deletes two rows through two different unique indexes;
- composite primary-key replacement;
- `VARCHAR` primary-key and prefix unique-key replacement under the current
  ASCII key restriction;
- nullable unique-key `NULL` parts inserting independent rows;
- generated and explicit `AUTO_INCREMENT` replacement behavior, including
  `LAST_INSERT_ID()`;
- child-side and parent-side foreign-key behavior;
- persistence across close/reopen, table rename, and independent file-backed
  handles where practical;
- unchanged catalog generation, unchanged SQLite schema generation except for
  normal statement cache effects, and preserved `.mylite` preamble bytes;
- existing replace, insert, primary-key, unique-index, foreign-key,
  auto-increment, parser, and full check workflows still passing.
