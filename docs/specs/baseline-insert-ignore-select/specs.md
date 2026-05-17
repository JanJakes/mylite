# Baseline Insert Ignore Select

## Status

This feature closes the smallest practical gap between the existing
descriptor-backed `INSERT ... SELECT` path and the existing `INSERT IGNORE`
parser/runtime support. It admits `IGNORE` on clean table-backed
`INSERT ... SELECT` statements whose selected rows already satisfy the current
strict insert-select validation rules.

This is not full MySQL `INSERT IGNORE ... SELECT`. It does not implement
warning demotion, duplicate-row skipping, adjusted selected-row projection, or
row-scalar `INSERT IGNORE ... SELECT` sources. Those require a broader design
because the existing table-backed `INSERT ... SELECT` path validates selected
rows through an internal SQLite temporary table and inserts one planned row at a
time, while full MySQL `IGNORE` must adjust or skip only the offending rows and
report warnings.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline insert-select lifecycle:
  `docs/specs/baseline-insert-select-lifecycle/specs.md`
- Baseline insert-select dual-source extension:
  `docs/specs/baseline-insert-select-dual-source/specs.md`
- Baseline insert-ignore lifecycle:
  `docs/specs/baseline-insert-ignore-lifecycle/specs.md`
- Baseline insert modifier lifecycle:
  `docs/specs/baseline-insert-modifier-lifecycle/specs.md`
- MySQL lexer:
  `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `INSERT`:
  https://dev.mysql.com/doc/refman/8.4/en/insert.html
- MySQL 8.4 Reference Manual, `INSERT ... SELECT`:
  https://dev.mysql.com/doc/refman/8.4/en/insert-select.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Observations

The existing expectation script
`packages/libmylite/tests/mysql_baseline_insert_ignore_lifecycle_expectations.sh`
records the runtime probe that defines this slice's positive behavior:

- MySQL accepts `INSERT LOW_PRIORITY IGNORE INTO set_t(id, nn) SELECT id, nn
  FROM src;`.
- For a valid one-row source and an ordinary target with no conflicting key, it
  reports `ROW_COUNT() == 1`, `@@warning_count == 0`, and the selected row is
  inserted.

The official MySQL 8.4 `INSERT ... SELECT` syntax admits `LOW_PRIORITY` or
`HIGH_PRIORITY`, then optional `IGNORE`, before the target table. It documents
`IGNORE` for duplicate-key rows on `INSERT ... SELECT`. MyLite already rejects
table-backed `INSERT ... SELECT` into primary-key, unique-index, or
foreign-key-child targets, so this slice admits only the no-conflict clean-row
case and leaves duplicate skipping for a later feature.

Fresh local MySQL 8.4.9 probes were unavailable while writing this slice
because the local Docker runtime hung before it could list containers. No new
expectations are invented here; the behavior is constrained to the existing
repo artifact plus the official MySQL 8.4 documentation.

## Scope

The implementation must add:

- parser and AST support for `IGNORE` after no priority modifier,
  `LOW_PRIORITY`, or `HIGH_PRIORITY` on `INSERT ... SELECT`;
- support for optional `INTO` and the existing table-name and target-column
  subset;
- runtime acceptance for table-backed descriptor source `SELECT` statements
  whose selected rows are valid under the existing strict `INSERT ... SELECT`
  rules;
- `LOW_PRIORITY` and `HIGH_PRIORITY` as no-op modifiers when combined with
  `IGNORE`;
- no row result set, affected rows equal to inserted rows, and
  `warning_count == 0` for supported valid statements;
- preservation of the existing descriptor-driven target/source resolution,
  row validation, generated physical SQL, temporary-table materialization,
  physical row insertion, file-format invariants, and diagnostic ordering.

## Non-Goals

This feature must not implement:

- warning demotion or adjusted values for selected rows;
- duplicate-key skipping, `ON DUPLICATE KEY UPDATE`, key-bearing table-backed
  targets, or auto-increment table-backed targets;
- missing/default/null/range conversion differences beyond the existing strict
  `INSERT ... SELECT` behavior;
- row-scalar no-source or `FROM DUAL` `INSERT IGNORE ... SELECT`;
- `DELAYED IGNORE` on `INSERT ... SELECT`;
- `PARTITION`, `TABLE`, row constructors, joins, CTEs, query-expression
  parentheses, locking behavior beyond existing no-op source locking clauses,
  triggers, cascades, privileges, protocol insert-id metadata, or arbitrary
  SQLite pass-through;
- SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns statement dispatch,
  result-handle ownership, diagnostics reset, and public misuse behavior.
- Lexer/parser/AST own syntax admission and source spans for the optional
  `IGNORE` modifier. Parser code does not resolve names or inspect descriptors.
- Runtime planning owns deciding whether a parsed `IGNORE` is in the supported
  clean table-backed insert-select subset. Unsupported `IGNORE` shapes are
  rejected before mutation.
- Catalog descriptors remain authoritative for target and source schemas,
  table objects, column order, visibility, nullability, defaults, type ranges,
  object kind, and stable physical table names.
- SQLite remains the execution engine for source scanning, filtering, ordering,
  limiting, internal temporary-table storage, and physical row storage.
  MyLite continues to build generated SQL from descriptors and quoted physical
  identifiers, then validates and binds planned values itself.
- Storage/VFS ownership does not change. The `.mylite` preamble and shifted
  SQLite payload boundary must remain untouched.

## Supported SQL Grammar

Supported subset:

```sql
INSERT [LOW_PRIORITY | HIGH_PRIORITY] [IGNORE] [INTO]
    table_name [(column_name[, column_name] ...)]
    SELECT select_item_list
    FROM table_name [AS] alias
    [WHERE predicate]
    [ORDER BY order_key [ASC | DESC]]
    [LIMIT row_count]
```

The source `SELECT` subset is exactly the current table-backed
descriptor-driven `INSERT ... SELECT` source subset. No-source and `FROM DUAL`
row-scalar sources are intentionally excluded when `IGNORE` is present.

Unsupported examples:

```sql
INSERT DELAYED IGNORE INTO t SELECT id FROM src
INSERT IGNORE INTO t SELECT 1
INSERT IGNORE INTO t SELECT 1 FROM DUAL
INSERT IGNORE INTO keyed_t(id) SELECT id FROM src
INSERT IGNORE INTO t(id) SELECT id FROM src ON DUPLICATE KEY UPDATE id = 1
```

### MyLite Lemon-Syntax Snippet

This snippet describes MyLite's intended grammar extension for this feature,
not MySQL's full grammar:

```lemon
insert_select_statement(A) ::=
    INSERT(I) insert_modifier_opt(M) INTO table_name(T) insert_column_list_opt(C)
    select_statement(S). {
    A = mylite_sql_parser_make_insert_select_statement(state, I, T, C, S, M, NULL);
}
insert_select_statement(A) ::=
    INSERT(I) insert_select_ignore_prefix_opt(M) IGNORE(G) INTO table_name(T)
    insert_column_list_opt(C) select_statement(S). {
    A = mylite_sql_parser_make_insert_select_statement(
        state, I, T, C, S, M, mylite_sql_parser_make_insert_ignore_modifier(state, G)
    );
}

insert_select_ignore_prefix_opt(A) ::= . {
    A = NULL;
}
insert_select_ignore_prefix_opt(A) ::= LOW_PRIORITY(T). {
    A = mylite_sql_parser_make_insert_low_priority_modifier(state, T);
}
insert_select_ignore_prefix_opt(A) ::= HIGH_PRIORITY(T). {
    A = mylite_sql_parser_make_insert_high_priority_modifier(state, T);
}
```

The existing no-`IGNORE` productions still use `insert_modifier_opt`, so
`INSERT DELAYED ... SELECT` remains in the existing MyLite compatibility slice.
The `IGNORE` productions use a narrower prefix so `DELAYED IGNORE` remains
unsupported for `INSERT ... SELECT`.

## Semantics

### Name Resolution

Target and source resolution reuse the existing `INSERT ... SELECT` policy:
unqualified names require a selected default schema, schema-qualified names use
the named schema, reserved `_mylite_*` names are rejected, target resolution
precedes source resolution, and descriptor table objects are required. Unknown
schemas, unknown tables, unknown columns, duplicate target columns, column-count
mismatch, unsupported object kinds, and read-only persistent writes keep their
existing diagnostics. `IGNORE` does not demote these errors in this slice.

### Runtime Acceptance

If the parsed statement contains `IGNORE`, runtime planning accepts it only
when the source is the table-backed descriptor `SELECT` path. Row-scalar
no-source and `FROM DUAL` sources are rejected with a deterministic unsupported
diagnostic because those paths reuse the normal one-row insert planner where
full `IGNORE` would need duplicate/default/null/range warning demotion.

Table-backed execution keeps the current algorithm:

1. plan target descriptors;
2. reject primary-key, unique-index, and foreign-key-child targets as the
   existing table-backed `INSERT ... SELECT` path already does;
3. plan the descriptor source `SELECT`;
4. materialize the selected rows into an internal SQLite temporary table;
5. stream validation from the temporary table;
6. insert rows with prepared SQLite statements and descriptor-bound values.

For admitted valid rows, `IGNORE` has no extra visible effect: the statement
succeeds, inserts the same rows as ordinary `INSERT ... SELECT`, reports the
inserted row count, and records zero warnings.

If existing strict table-backed insert-select validation fails, the statement
fails with the existing strict error. MyLite must not silently adjust selected
values or continue after row-level errors until a later feature specifies and
tests MySQL-compatible demotion.

### SQLite Handling

No new SQLite extension point is needed. This remains MyLite wrapper/runtime
behavior over public SQLite prepared statements. Generated SQL keeps the
existing descriptor-built shapes, quoted physical identifiers, bound predicate
and limit parameters, internal temporary table, and stable physical target
table name. SQLite schema text is not metadata authority.

## Diagnostics

Supported valid statements return through the existing non-row result API
conventions with zero warnings.

Unsupported or failing cases use existing diagnostics where they already
exist:

- syntax errors for invalid modifier order or unsupported grammar shapes;
- missing default schema, unknown schema, unknown table, reserved target/source
  names, unsupported object kind, and read-only target errors from descriptor
  resolution;
- unknown target/source/order/predicate columns and column-count mismatch from
  current insert-select planning;
- strict `NULL`, omitted required column, and range errors from current
  insert-select validation;
- unsupported errors for row-scalar `INSERT IGNORE ... SELECT` sources and
  key-bearing table-backed targets;
- physical SQLite, allocation, and public API misuse errors through existing
  paths.

## Tests

Add focused parser and runtime coverage:

- parse `INSERT IGNORE INTO target(cols) SELECT ... FROM source`;
- parse `INSERT LOW_PRIORITY IGNORE ... SELECT` and `INSERT HIGH_PRIORITY
  IGNORE ... SELECT` with modifier and ignore AST nodes;
- keep invalid modifier order rejected;
- accept a valid table-backed `INSERT IGNORE ... SELECT` into a no-key target,
  verify affected rows, warning count, absence of result rows, and inserted
  data;
- accept the MySQL-observed `LOW_PRIORITY IGNORE` table-backed form;
- reject row-scalar no-source or `FROM DUAL` `INSERT IGNORE ... SELECT`
  deterministically;
- preserve strict validation errors for omitted required columns, selected
  `NULL` into `NOT NULL`, and out-of-range selected values;
- verify persistence through close/reopen and existing parser/runtime insert
  lifecycle tests.

The MySQL-runtime expectation for the positive `LOW_PRIORITY IGNORE` table
source case already lives in
`packages/libmylite/tests/mysql_baseline_insert_ignore_lifecycle_expectations.sh`.
Fresh expectation generation must be rerun once Docker/MySQL 8.4.9 is usable
again.
