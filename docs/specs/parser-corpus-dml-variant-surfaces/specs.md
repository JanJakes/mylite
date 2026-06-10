# Parser Corpus DML Variant Surfaces

This slice reduces remaining MySQL server-test parser-corpus failures where
MySQL accepts DML statement shapes that MyLite's current descriptor-driven DML
executors either do not support or support only in a narrower canonical form.

Primary MySQL references:

- https://dev.mysql.com/doc/refman/8.4/en/delete.html
- https://dev.mysql.com/doc/refman/8.4/en/update.html
- https://dev.mysql.com/doc/refman/8.4/en/insert.html
- https://dev.mysql.com/doc/refman/8.4/en/replace.html

The behavior in this slice is independently specified from the official
MySQL 8.4 documentation and the local MySQL 8.4.9 runtime probes captured by
`mysql_parser_corpus_dml_variant_surfaces_expectations.sh`.

## MySQL 8.4.9 Runtime Observations

The expectation script verifies representative accepted MySQL 8.4.9 syntax:

- ordinary single-table `DELETE` accepts `LOW_PRIORITY`, `QUICK`, and
  `IGNORE` modifiers before `FROM`;
- multi-table `DELETE` accepts a target list with a single source table, the
  `DELETE FROM target USING source` spelling, modifier-prefixed variants of
  those forms, and `target.*` target spellings;
- single-table `DELETE` accepts more than one `ORDER BY` key;
- `UPDATE IGNORE` is accepted on comma-joined updates, `UPDATE ... JOIN ...
  USING (...)` is accepted, and single-table `UPDATE` accepts more than one
  `ORDER BY` key;
- `INSERT ... VALUES` and `INSERT ... SET` may use bare column identifiers in
  value positions;
- `INSERT ... ON DUPLICATE KEY UPDATE` accepts duplicate-update assignments
  that read the current target column value;
- `REPLACE ... SELECT ... UNION ALL SELECT ...` is valid.

Observed representative probe:

```sql
CREATE TABLE t (id INT PRIMARY KEY, v INT, other INT, dt DATETIME, da DATETIME);
CREATE TABLE u (id INT PRIMARY KEY, v INT);
DELETE LOW_PRIORITY FROM t WHERE id = 1;
DELETE QUICK FROM t WHERE id = 1;
DELETE IGNORE FROM t WHERE id = 1;
DELETE t FROM t WHERE id = 1;
DELETE LOW_PRIORITY QUICK t FROM t WHERE id = 1;
DELETE FROM a USING t AS a WHERE a.id = 1;
DELETE LOW_PRIORITY QUICK FROM a USING t AS a WHERE a.id = 1;
DELETE t.*, u.* FROM t, u WHERE t.id = u.id;
DELETE FROM t ORDER BY id, v DESC LIMIT 1;
UPDATE IGNORE t, u SET t.v = u.v WHERE t.id = u.id;
UPDATE t LEFT JOIN u USING(id) SET t.v = u.v;
UPDATE t SET v = 10 ORDER BY id, v DESC LIMIT 1;
INSERT INTO t (id, v) VALUES (id, v);
INSERT INTO t SET id = 7, v = 70, other = v;
INSERT INTO t VALUES (1, 20, 200, '2000-01-01', '2000-01-01')
  ON DUPLICATE KEY UPDATE v = v;
REPLACE INTO t (id, v)
  SELECT id, v FROM u UNION ALL SELECT id + 10, v FROM u;
```

MySQL returns the rows and affected-row counts recorded in the expectation
script.

## Scope

In scope:

- executable no-op modifier support for `DELETE LOW_PRIORITY` and
  `DELETE QUICK` in the existing single-table delete subset;
- parser fallback classification for valid but unsupported DML variants that
  still fail the normal parser:
  - `DELETE IGNORE` modifier forms;
  - single-source multi-table `DELETE target FROM source`;
  - `DELETE FROM target USING source` forms outside the current joined-delete
    grammar;
  - `target.*` multi-table delete target spellings;
  - multi-key single-table `DELETE ... ORDER BY`;
  - `UPDATE` variants with joined modifiers, `USING` join conditions, or
    multi-key `ORDER BY` in forms that fail normal parsing;
  - `INSERT` / `REPLACE` value positions that contain column identifiers and
    currently fail the normal DML value grammar;
  - duplicate-key update assignments that read a target column directly and
    currently fail the normal duplicate-update value grammar;
  - `REPLACE ... SELECT` compound query sources;
- deterministic unsupported-utility diagnostics for recognized placeholder
  DML variants.

Out of scope:

- `DELETE IGNORE` warning/error demotion semantics;
- executable `target.*` deletion, multi-physical-target deletion, more than two
  joined delete sources, or single-source multi-table delete lowering;
- joined-update modifiers, `USING` join execution, multi-key DML ordering, or
  arbitrary joined-update grammar;
- executable identifier-backed `INSERT` values, column-to-column DML
  assignment evaluation, or general row expression planning;
- `REPLACE ... SELECT` compound source execution;
- triggers, privileges, optimizer behavior, storage-engine scheduling, or
  SQLite fork hooks.

## MyLite Grammar Snippet

These MyLite-owned Lemon snippets describe the intended grammar direction. The
current implementation uses normal grammar where already safe and a
post-failure placeholder classifier for broader shapes.

```lemon
delete_statement ::=
    DELETE FROM delete_table_reference
    where_clause_opt order_clause_opt delete_limit_clause_opt.

delete_statement ::=
    DELETE LOW_PRIORITY FROM delete_table_reference
    where_clause_opt order_clause_opt delete_limit_clause_opt.

delete_statement ::=
    DELETE DELETE_QUICK_MODIFIER FROM delete_table_reference
    where_clause_opt order_clause_opt delete_limit_clause_opt.

delete_statement ::=
    DELETE LOW_PRIORITY DELETE_QUICK_MODIFIER FROM delete_table_reference
    where_clause_opt order_clause_opt delete_limit_clause_opt.

joined_delete_statement ::=
    DELETE delete_target_list FROM table_source where_clause_opt.

joined_delete_statement ::=
    DELETE FROM delete_target USING table_source where_clause_opt.

delete_target ::= table_name.
delete_target ::= table_name DOT STAR.

update_statement ::=
    UPDATE update_modifier_list table_references SET update_assignment_list
    where_clause_opt order_clause_opt update_limit_clause_opt.

insert_value ::= identifier.
duplicate_update_value ::= qualified_identifier.
replace_select_statement ::= REPLACE replace_target query_expression.
```

`DELETE LOW_PRIORITY` and `DELETE QUICK` are admitted by normal grammar and
execute through the existing single-table delete planner as embedded no-ops.
`DELETE_QUICK_MODIFIER` is MyLite's context-mapped parser token for `QUICK` in
the `DELETE` modifier prefix, keeping ordinary `QUICK` identifier behavior out
of that grammar conflict.
The wider snippets above remain placeholder-only in this slice.

## Runtime Behavior

`DELETE LOW_PRIORITY` and `DELETE QUICK` preserve existing single-table delete
semantics, diagnostics, affected rows, warnings, foreign-key actions, catalog
touching, and transaction handling. MyLite treats those modifiers as no-ops.

Recognized DML variant surfaces that are not executable become
`MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT` after normal parsing fails. At
execution time they return MyLite's existing deterministic unsupported-utility
diagnostic. Malformed DML tails remain syntax errors.

## Tests

Tests cover:

- MySQL 8.4.9 expectation probes for accepted DML variant syntax and observed
  affected rows/result values;
- parser support for executable `DELETE LOW_PRIORITY` and `DELETE QUICK`;
- parser placeholder admission for valid unsupported DML variants;
- malformed-tail regression tests to prevent over-broad placeholder
  classification;
- runtime execution for `DELETE LOW_PRIORITY` and `DELETE QUICK`;
- runtime unsupported diagnostics for placeholder DML variants;
- parser corpus benchmark movement over
  `build/perf-data/mysql-server-tests-queries.csv`.

## Compatibility Status

This slice improves DML syntax compatibility by making `DELETE LOW_PRIORITY`
and `DELETE QUICK` real no-op modifiers and by converting recognized
MySQL-valid but unsupported DML variants from syntax errors into explicit
unsupported placeholders. It does not broaden executable multi-table DML,
identifier-backed value evaluation, or general expression semantics.
