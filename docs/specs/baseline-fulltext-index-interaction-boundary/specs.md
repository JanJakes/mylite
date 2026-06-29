# Baseline FULLTEXT Index Interaction Boundary Specification

## Summary

MyLite supports `FULLTEXT` indexes as descriptor metadata, not as executable
full-text search indexes. This slice documents and tests that boundary so the
detailed compatibility matrix reflects the current behavior:

- supported `FULLTEXT` definitions are validated, stored, cloned, renamed,
  dropped, made visible or invisible, and exposed through supported metadata
  surfaces;
- DML against tables with `FULLTEXT` descriptors continues to use ordinary row
  storage and ignores those descriptors for search planning;
- `MATCH ... AGAINST` syntax remains a parser-admitted placeholder that returns
  a deterministic unsupported diagnostic, even when a matching `FULLTEXT`
  descriptor exists;
- no SQLite b-tree index, SQLite FTS virtual table, trigger-maintained shadow
  table, tokenizer, rank calculation, or optimizer hook is introduced.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing metadata-only fulltext slices:
  `docs/specs/baseline-fulltext-index-metadata/specs.md`,
  `docs/specs/baseline-add-fulltext-indexes/specs.md`
- Existing full-text parser placeholder slice:
  `docs/specs/parser-corpus-view-fulltext-utility-placeholders/specs.md`
- MySQL 8.4 Reference Manual, column indexes:
  <https://dev.mysql.com/doc/refman/8.4/en/column-indexes.html>
- MySQL 8.4 Reference Manual, boolean full-text searches:
  <https://dev.mysql.com/doc/refman/8.4/en/fulltext-boolean.html>
- MySQL 8.4 Reference Manual, InnoDB full-text index tables:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-innodb-ft-index-table-table.html>
- Observed MySQL 8.4.9 metadata behavior recorded by
  `packages/libmylite/tests/mysql_baseline_fulltext_index_metadata_expectations.sh`
  and
  `packages/libmylite/tests/mysql_baseline_add_fulltext_indexes_expectations.sh`.

This specification is independently authored from official MySQL 8.4
documentation, observed MySQL 8.4.9 runtime behavior, public SQLite APIs, and
existing MyLite code. It does not copy MySQL, MariaDB, Percona, SQLite
implementation internals, or restrictively licensed implementation sources.

## MySQL Behavior Relevant to This Boundary

MySQL treats `FULLTEXT` as a search-index class for character and text columns.
Full-text queries use `MATCH ... AGAINST`, parser modes, tokenization, stopword
state, inverted-index metadata, ranking, and optimizer behavior that is
specific to supported storage engines such as InnoDB and MyISAM. InnoDB boolean
full-text queries require a matching `FULLTEXT` index over the searched columns.

Those search semantics are intentionally outside this MyLite baseline. The
verified behavior for this slice is the descriptor metadata and deterministic
unsupported-search boundary, not MySQL-equivalent full-text result sets.

## Scope

Supported:

- descriptor-only interaction between supported `FULLTEXT` index DDL and
  supported metadata surfaces;
- DML on tables with fulltext descriptors, with ordinary storage unaffected by
  the descriptors;
- `SHOW COLUMNS`, `SHOW CREATE TABLE`, `SHOW INDEX`, limited
  `INFORMATION_SCHEMA.COLUMNS`, and limited
  `INFORMATION_SCHEMA.STATISTICS` reflecting `FULLTEXT` descriptors;
- `CREATE TABLE ... LIKE`, `DROP INDEX`, `ALTER TABLE ... DROP INDEX`,
  `ALTER TABLE ... RENAME INDEX`, and `ALTER TABLE ... ALTER INDEX
  VISIBLE|INVISIBLE` operating on `FULLTEXT` descriptors without physical
  SQLite index DDL;
- parenthesized and shorthand `MATCH ... AGAINST` syntax reaching deterministic
  unsupported diagnostics rather than SQLite expression fallback.

Deferred:

- executable full-text search;
- `MATCH ... AGAINST` result values, comparisons, ranking, modes, query
  expansion, boolean operators, stopword semantics, or `EXPLAIN` behavior;
- optimizer enforcement that a `FULLTEXT` index must exist for searched
  columns;
- SQLite FTS virtual tables, shadow data synchronization, tokenizer plugins, or
  generated rank expressions;
- MySQL `INFORMATION_SCHEMA.INNODB_FT_*` inverted-index content beyond the
  existing placeholder metadata surfaces;
- privilege-sensitive metadata and storage-engine-specific performance
  behavior.

## Runtime Contract

MyLite uses catalog descriptors as the authority for `FULLTEXT` index metadata.
Creating, adding, renaming, dropping, cloning, and toggling visibility of a
supported fulltext descriptor mutates only MyLite catalog rows. It must not
create a physical SQLite index or FTS object.

Ordinary DML against a table with a fulltext descriptor is unchanged. Inserts,
updates, deletes, and scalar predicates operate over the generated row table;
no fulltext side table is maintained.

When the parser admits a `MATCH ... AGAINST` expression, execution must stop
before generated SQLite SQL is run. Shorthand full-text syntax classified by
the parser-corpus placeholder surface uses the unsupported-utility diagnostic.
Parenthesized `MATCH(...) AGAINST(...)` inside an otherwise supported `SELECT`
shape currently reaches the limited predicate planner and returns its
deterministic unsupported-predicate diagnostic:

```sql
SELECT id FROM posts WHERE MATCH(body) AGAINST ('alpha');
```

Expected MyLite result:

- statement fails;
- diagnostic code `1064`;
- SQLSTATE `42000`;
- message contains `SELECT supports only descriptor column WHERE predicates`.

## SQLite Integration

This slice does not require a targeted SQLite fork patch. Public SQLite FTS
virtual tables are not used because adopting them would need a separate design
for MySQL tokenization, parser plugins, stopwords, document-id metadata,
transactional synchronization, ranking, query modes, and file-format impact.

The current implementation is therefore a MyLite catalog/runtime boundary:
metadata is MyLite-owned, data remains in SQLite row tables, and unsupported
full-text search syntax is rejected before SQLite can reinterpret it.

## Tests

Existing MySQL 8.4.9 expectation scripts cover `FULLTEXT` descriptor metadata,
warnings, validation, and unsupported options:

- `packages/libmylite/tests/mysql_baseline_fulltext_index_metadata_expectations.sh`
- `packages/libmylite/tests/mysql_baseline_add_fulltext_indexes_expectations.sh`

Existing MyLite runtime tests cover:

- descriptor metadata and persistence;
- no physical SQLite index for fulltext descriptors;
- add/drop/rename/clone/visibility operations;
- DML preserving ordinary row storage;
- deterministic unsupported `MATCH ... AGAINST` execution on a table that has
  fulltext metadata, without falling through to SQLite.

## Compatibility Status

The `FULLTEXT index interaction` detailed row is `🟡`: MyLite implements the
metadata and deterministic unsupported-search boundary, but not MySQL full-text
search, optimizer, ranking, tokenizer, or inverted-index behavior.
