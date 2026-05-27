# Baseline MySQL Charset And Collation Catalog

## Summary

This phase expands MyLite's static MySQL metadata catalog for character sets
and collations. MyLite already supports a deliberately small semantic subset
for DDL, storage, scalar conversion metadata, and comparison behavior. This
feature exposes the MySQL 8.4.9 catalog rows applications commonly inspect
through:

- `SHOW CHARACTER SET` / `SHOW CHARSET`;
- `SHOW COLLATION`;
- `INFORMATION_SCHEMA.CHARACTER_SETS`;
- `INFORMATION_SCHEMA.COLLATIONS`;
- `INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY`.

The feature is metadata-only. It does not make every listed character set or
collation valid for table definitions, column definitions, conversion,
comparison, ordering, grouping, indexing, or storage. Existing admitted MyLite
character-set and collation semantics remain the authority for user data.

## Compatibility Authority

Sources:

- MySQL 8.4 Reference Manual, `SHOW CHARACTER SET`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-character-set.html>
- MySQL 8.4 Reference Manual, `SHOW COLLATION`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-collation.html>
- MySQL 8.4 Reference Manual, supported character sets and collations:
  <https://dev.mysql.com/doc/refman/8.4/en/charset-charsets.html>
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.CHARACTER_SETS`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-character-sets-table.html>
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.COLLATIONS`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-collations-table.html>
- MySQL 8.4 Reference Manual,
  `INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-collation-character-set-applicability-table.html>
- MySQL 8.4.9 runtime probes recorded by
  `packages/libmylite/tests/mysql_baseline_mysql_charset_collation_catalog_expectations.sh`.

Observed against the local `mysql:8.4.9` runtime:

- `SELECT VERSION()` reports `8.4.9`.
- `SHOW CHARACTER SET` and `INFORMATION_SCHEMA.CHARACTER_SETS` contain 41 rows.
- `SHOW COLLATION`, `INFORMATION_SCHEMA.COLLATIONS`, and
  `INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY` contain 286 rows.
- The ordered `CHARACTER_SETS` catalog hash, using
  `CHARACTER_SET_NAME|DEFAULT_COLLATE_NAME|DESCRIPTION|MAXLEN`, is
  `c7e4bc4bb3ed590bdceccdc5f51725baa3054a44bacaaa9a778d5b2ee2919039`.
- The ordered `COLLATIONS` catalog hash, using
  `COLLATION_NAME|CHARACTER_SET_NAME|ID|IS_DEFAULT|IS_COMPILED|SORTLEN|PAD_ATTRIBUTE`,
  is `1881d6661f40f230c2ffdf7f6b4baa293cd1fcfe091526a690432e15bf4082a8`.
- `@@character_set_server` is `utf8mb4` and `@@collation_server` is
  `utf8mb4_0900_ai_ci`.
- `LIKE` filters on `SHOW CHARACTER SET` and `SHOW COLLATION` match catalog
  names case-insensitively for the observed ASCII names.

This specification is independently authored from project documentation,
official MySQL documentation, MySQL 8.4.9 runtime observations, public SQLite
APIs, and existing MyLite source. It does not copy MySQL, MariaDB, Percona, or
SQLite implementation sources.

## Scope

The implementation must add:

- a separate static MySQL catalog row set for the 41 MySQL 8.4.9 character
  sets and the 286 MySQL 8.4.9 collations;
- `SHOW CHARACTER SET` / `SHOW CHARSET` rows generated from that catalog;
- `SHOW COLLATION` rows generated from that catalog;
- `INFORMATION_SCHEMA.CHARACTER_SETS`, `INFORMATION_SCHEMA.COLLATIONS`, and
  `INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY` rows generated
  from that catalog;
- existing result-column shapes, warning count `0`, and result-set row-count
  behavior for successful supported queries;
- existing `LIKE` filter behavior for `SHOW` statements;
- existing information-schema projection, alias, predicate, order, and `LIMIT`
  behavior over the wider static row sets;
- tests that verify the exact MySQL 8.4.9 row counts, representative rows,
  filtering, metadata access, and DDL non-admission boundaries.

## Non-Goals

This feature must not implement:

- DDL admission for all cataloged character sets or collations;
- storage, conversion, comparison, ordering, grouping, collation coercibility,
  or index semantics for all cataloged rows;
- `SHOW CHARACTER SET ... WHERE` or `SHOW COLLATION ... WHERE`;
- `mysql.character_sets`, `mysql.collations`, or data-dictionary base tables;
- mutable server defaults, privilege filtering, account-specific visibility,
  or plugin-dependent catalog variation;
- arbitrary information-schema joins or wider query support beyond the current
  metadata-select subset;
- SQLite metadata inspection, SQLite virtual tables, or SQLite fork changes.

## Ownership Boundary

- Public API: unchanged. Results use the existing `mylite_execute()` and
  `mylite_result` conventions.
- Parser/AST: unchanged. The current grammar already admits the supported
  `SHOW` and information-schema `SELECT` shapes.
- Runtime metadata: owns the static MySQL 8.4.9 catalog arrays and row
  generation for the supported metadata surfaces.
- Semantic charset/collation validators: continue to use the existing narrow
  MyLite-supported descriptors. Catalog visibility is not semantic support.
- Catalog module: unchanged. No persistent catalog rows are added or mutated.
- Storage/VFS/SQLite: unchanged. The metadata rows are synthetic and do not
  inspect SQLite schema text, PRAGMAs, or physical user rows.

## Query Surface

Existing grammar remains in force:

```lemon
statement ::= show_character_set_statement.
statement ::= show_collation_statement.

show_character_set_statement ::= SHOW CHARACTER SET show_like_clause_opt.
show_character_set_statement ::= SHOW CHARSET show_like_clause_opt.
show_collation_statement ::= SHOW COLLATION show_like_clause_opt.

show_like_clause_opt ::= .
show_like_clause_opt ::= LIKE STRING.
```

Existing information-schema `SELECT` support remains in force for the three
tables:

```sql
SELECT select_list
FROM INFORMATION_SCHEMA.CHARACTER_SETS [AS alias]
[WHERE supported_metadata_predicate]
[ORDER BY one_metadata_column [ASC | DESC]]
[LIMIT row_count]

SELECT select_list
FROM INFORMATION_SCHEMA.COLLATIONS [AS alias]
[WHERE supported_metadata_predicate]
[ORDER BY one_metadata_column [ASC | DESC]]
[LIMIT row_count]

SELECT select_list
FROM INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY [AS alias]
[WHERE supported_metadata_predicate]
[ORDER BY one_metadata_column [ASC | DESC]]
[LIMIT row_count]
```

No joins, subqueries, grouping, expression projections, expression predicates,
or writable metadata tables are added by this phase.

## Row Semantics

`SHOW CHARACTER SET` emits MySQL's four visible columns:

| Column | Value source |
| --- | --- |
| `Charset` | catalog character-set name |
| `Description` | catalog description |
| `Default collation` | catalog default collation |
| `Maxlen` | catalog maximum byte length |

`INFORMATION_SCHEMA.CHARACTER_SETS` emits the same catalog using MySQL's
information-schema column names:

| Column | Value source |
| --- | --- |
| `CHARACTER_SET_NAME` | catalog character-set name |
| `DEFAULT_COLLATE_NAME` | catalog default collation |
| `DESCRIPTION` | catalog description |
| `MAXLEN` | catalog maximum byte length |

`SHOW COLLATION` and `INFORMATION_SCHEMA.COLLATIONS` emit:

| Column | Value source |
| --- | --- |
| `COLLATION_NAME` / `Collation` | catalog collation name |
| `CHARACTER_SET_NAME` / `Charset` | owning character-set name |
| `ID` / `Id` | MySQL 8.4.9 collation id |
| `IS_DEFAULT` / `Default` | `Yes` for default collation, otherwise empty string |
| `IS_COMPILED` / `Compiled` | `Yes` for the observed catalog rows |
| `SORTLEN` / `Sortlen` | observed sort length |
| `PAD_ATTRIBUTE` / `Pad_attribute` | `PAD SPACE` or `NO PAD` |

`INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY` emits one row per
collation with `COLLATION_NAME` and `CHARACTER_SET_NAME`.

Rows are static for the MySQL 8.4.9 target runtime. Applications that need a
stable order must request `ORDER BY`, matching the existing information-schema
query contract.

## Semantic Support Boundary

Only the pre-existing MyLite semantic subset remains admitted for DDL and
runtime behavior. In particular:

- `utf8mb4`, `ascii`, and `binary` remain the current admitted character-set
  families in the supported DDL paths, with the existing per-statement limits.
- Existing admitted collations such as `utf8mb4_0900_ai_ci`,
  `utf8mb4_0900_bin`, legacy `utf8mb4` rows, `ascii_general_ci`,
  `ascii_bin`, and `binary` keep their current behavior.
- Catalog-only rows such as `latin1`, `ucs2`, `utf16`, or
  `utf8mb4_ja_0900_as_cs_ks` remain rejected by DDL paths unless a later
  feature explicitly implements their semantics.

This split prevents discovery queries from failing while avoiding false claims
about conversion or comparison support.

## Diagnostics

No new SQL diagnostics are introduced. Existing diagnostics remain authoritative
for:

- unsupported `SHOW ... WHERE` forms;
- unsupported or malformed `SHOW ... LIKE` patterns;
- unknown information-schema projection, predicate, or order columns;
- writable `information_schema` access;
- unsupported DDL character sets and collations;
- allocation failure and public API misuse.

Successful supported metadata reads emit no warnings.

## Performance And Storage

The wider catalog is a fixed in-process array: 41 character-set rows and 286
collation rows. Metadata queries materialize only the requested synthetic row
set inside the existing result builder. No user table rows are scanned, no
SQLite catalog is inspected, and no `.mylite` file-format or VFS behavior
changes are required.

## Tests

MySQL expectation coverage must verify:

- exact MySQL 8.4.9 counts and catalog hashes;
- representative character-set rows across single-byte, multibyte, Unicode,
  and binary families;
- representative collation rows across early ids, legacy pad-space rows, 0900
  no-pad rows, default rows, and high-id rows;
- `SHOW` case-insensitive `LIKE` filters and no-match status;
- information-schema `COUNT(*)`, predicates, ordering, and applicability rows.

MyLite C coverage must verify:

- `SHOW CHARACTER SET` row count and representative rows;
- `SHOW COLLATION` row count and representative rows;
- `INFORMATION_SCHEMA.CHARACTER_SETS`, `COLLATIONS`, and
  `COLLATION_CHARACTER_SET_APPLICABILITY` row counts;
- representative information-schema projections, predicates, ordering, and
  aliases;
- existing system-view `TABLES` and `COLUMNS` metadata still matches;
- catalog-only character sets and collations are not admitted by representative
  DDL validation paths;
- independent handles, reopen behavior, result-state behavior, and existing
  diagnostics remain stable.
