# Baseline Parenthesized String Defaults

## Status

This feature specifies a narrow default-expression slice for MyLite's existing
descriptor-owned `TEXT` family columns. It admits parenthesized constant string
defaults such as `TEXT DEFAULT ('abc')` for `CREATE TABLE`, `ALTER TABLE ... ADD
COLUMN`, and `ALTER TABLE ... MODIFY/CHANGE COLUMN`.

This is not a full default-expression feature. It does not add expression
evaluation beyond a single parenthesized string literal or `NULL`, and it does
not admit binary string, `JSON`, `CHAR`, or `VARCHAR` parenthesized expression
defaults.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- SQLite connection bootstrap policy:
  `docs/specs/sqlite-connection-bootstrap-policy/specs.md`
- File-backed MyLite opening VFS:
  `docs/specs/file-backed-mylite-opening-vfs/specs.md`
- MyLite file-format preamble:
  `docs/specs/mylite-file-format/specs.md`
- Baseline catalog foundation:
  `docs/specs/baseline-catalog-foundation/specs.md`
- Baseline string defaults:
  `docs/specs/baseline-string-defaults/specs.md`
- Baseline `TEXT` type:
  `docs/specs/baseline-text-type/specs.md`
- Baseline `ALTER TABLE ADD COLUMN` positioning:
  `docs/specs/baseline-alter-table-add-column-positioning/specs.md`
- Baseline `ALTER TABLE MODIFY/CHANGE COLUMN`:
  `docs/specs/baseline-alter-table-modify-column/specs.md`
- MySQL lexer:
  `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, data type default values:
  https://dev.mysql.com/doc/refman/8.4/en/data-type-defaults.html
- MySQL 8.4 Reference Manual, `CREATE TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/create-table.html
- MySQL 8.4 Reference Manual, `ALTER TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/alter-table.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_parenthesized_string_defaults_expectations.sh`
records the runtime probes for this feature. Observed behavior that shapes this
slice:

- `TEXT`, `TINYTEXT`, `MEDIUMTEXT`, and `LONGTEXT` reject a bare string default
  such as `DEFAULT 'abc'` with error `1101 / 42000`.
- The same types accept a parenthesized string literal default such as
  `DEFAULT ('abc')` and `DEFAULT ('')`.
- `SHOW CREATE TABLE` renders admitted `TEXT` family expression defaults as
  `DEFAULT (_utf8mb4'...')`.
- `SHOW COLUMNS` and `INFORMATION_SCHEMA.COLUMNS` report the decoded expression
  text in `Default` / `COLUMN_DEFAULT` and `DEFAULT_GENERATED` in `Extra`.
- Omitted-column `INSERT` and explicit `DEFAULT` materialize the decoded string
  value. `UPDATE column = DEFAULT` uses the same value.
- `CREATE TABLE ... LIKE` preserves admitted `TEXT` family expression defaults.
- `ALTER TABLE ... ADD COLUMN text_col TEXT DEFAULT ('x')` and
  `ALTER TABLE ... MODIFY COLUMN text_col TEXT DEFAULT ('x')` accept and
  preserve the expression default.
- `ALTER TABLE ... ALTER COLUMN text_col SET DEFAULT ('x')` rejects with
  `1101 / 42000`; this slice keeps that form unsupported.
- `DEFAULT (NULL)` is accepted as a generated default. DML that materializes
  that default into a `NOT NULL` column fails with `1048 / 23000`, the same as
  assigning `NULL`.
- MySQL also accepts broader expression defaults, including numeric constants
  and built-in function calls, and parenthesized expression defaults for
  `CHAR`/`VARCHAR`. MyLite defers those because they require broader expression
  evaluation and coercion timing.

## Scope

The implementation must add:

- validation and finalization for parenthesized constant string defaults on
  existing `TINYTEXT`, `TEXT`, `MEDIUMTEXT`, and `LONGTEXT` descriptors;
- parenthesized `NULL` generated defaults for `TEXT` family descriptors;
- `CREATE TABLE`, `ALTER TABLE ... ADD COLUMN`, and `ALTER TABLE ...
  MODIFY/CHANGE COLUMN` coverage through the existing column-definition
  planning paths;
- descriptor-owned storage of the decoded string default text, preserving the
  MyLite catalog as authoritative;
- descriptor materialization through omitted-column `INSERT`, explicit DML
  `DEFAULT`, `REPLACE`, and supported `UPDATE column = DEFAULT` paths;
- `SHOW COLUMNS`, `DESCRIBE`, `EXPLAIN table`, `SHOW CREATE TABLE`, and limited
  `INFORMATION_SCHEMA.COLUMNS` rendering for admitted generated `TEXT` family
  defaults;
- persistence through reopen, `CREATE TABLE ... LIKE`, table rename/drop, and
  independent file-backed handles;
- `.mylite` preamble preservation and no SQLite fork changes.

The implementation must not add:

- bare `TEXT` family string defaults;
- binary string or BLOB expression defaults;
- `JSON`, geometry, `CHAR`, or `VARCHAR` parenthesized expression defaults;
- numeric, function, arithmetic, column-reference, subquery, parameter,
  variable, or general expression defaults for `TEXT` family columns;
- `ALTER TABLE ... ALTER COLUMN text_col SET DEFAULT ('x')`;
- warning-producing truncation, charset introducer parsing, charset conversion,
  collation evaluation, or broader SQL mode behavior.

## Ownership Boundaries

- Public API: no ABI or API changes. Successful statements return through the
  existing non-row result conventions; errors use the existing diagnostics
  surface.
- Statement context: unchanged. The slice does not add per-statement state.
- Parser/AST: the existing `DEFAULT (expression)` grammar already represents
  these forms as a parenthesized expression node. No new public grammar node is
  required.
- Analyzer/planner: column-definition planning validates that the admitted
  expression is exactly a parenthesized `STRING` or `NULL` literal and that the
  target descriptor is a `TEXT` family descriptor.
- Catalog module: existing default-kind and default-text fields remain
  authoritative. Parenthesized string defaults reuse text default storage with
  `TEXT` family descriptor context identifying the value as generated for
  rendering.
- Result builder: existing `SHOW` and `INFORMATION_SCHEMA` builders render
  descriptor metadata. They must show `DEFAULT_GENERATED` for admitted `TEXT`
  family generated defaults.
- Storage/VFS: unchanged. The `.mylite` preamble and shifted SQLite payload
  invariants are preserved.
- SQLite physical storage: generated physical columns remain ordinary rowid
  SQLite tables with `TEXT` storage. Generated SQLite SQL quotes identifiers and
  string defaults; DML values continue through prepared statements and bound
  values.

## Syntax

The existing independently authored MyLite grammar already admits the needed
column-default shape:

```lemon
column_default_value(A) ::= LPAREN(L) expression(E) RPAREN(R). {
    A = mylite_sql_parser_make_parenthesized_expression(state, L, E, R);
}
```

This feature narrows semantic acceptance after parsing:

```text
text_family_generated_default:
    DEFAULT ( string_literal )
  | DEFAULT ( NULL )
```

The rule applies only where the host statement supplies a full column
definition: `CREATE TABLE`, `ALTER TABLE ... ADD COLUMN`, and `ALTER TABLE ...
MODIFY/CHANGE COLUMN`.

## Semantics

For `DEFAULT ('text')`, MyLite decodes the SQL string literal with the same
string-literal decoder used for `TEXT` DML values. Embedded `NUL` remains
unsupported, invalid UTF-8 remains rejected for current `TEXT` descriptors, and
the decoded byte length must fit the target `TEXT` family limit.

For `DEFAULT (NULL)`, MyLite stores a generated null default. The DDL succeeds
even for a `NOT NULL` column. Later DML default materialization follows the
existing null-assignment path, so materializing that default into a `NOT NULL`
column fails with `Column 'name' cannot be null` unless an existing
`IGNORE`/implicit-default path explicitly adjusts it.

`SHOW CREATE TABLE` renders generated `TEXT` family string defaults with a
charset introducer matching MySQL's observed default `utf8mb4` rendering:
`DEFAULT (_utf8mb4'decoded')`. `SHOW COLUMNS` and
`INFORMATION_SCHEMA.COLUMNS.COLUMN_DEFAULT` report `_utf8mb4'decoded'` and
`DEFAULT_GENERATED` for string generated defaults, and `NULL` plus
`DEFAULT_GENERATED` for generated null defaults.

`ALTER TABLE ... ALTER COLUMN text_col SET DEFAULT ('x')` remains rejected for
this slice. This keeps MyLite aligned with observed MySQL behavior for `TEXT`
family columns while allowing full column-definition DDL to carry the generated
default.

## Diagnostics

- Bare `TEXT` family defaults, including `TEXT DEFAULT 'x'`, continue to return
  MyLite's deterministic invalid-default diagnostic.
- Parenthesized `TEXT` family defaults whose inner expression is not a single
  string or `NULL` literal return the same deterministic invalid-default
  diagnostic.
- Parenthesized defaults on `CHAR`/`VARCHAR`, binary string, `JSON`, `ENUM`,
  `SET`, `BIT`, `YEAR`, and other deferred descriptors continue through the
  existing invalid-default or unsupported diagnostics.
- Unsupported `ALTER TABLE ... ALTER COLUMN ... SET DEFAULT ('x')` on `TEXT`
  family columns returns the existing deterministic invalid-default diagnostic.
- SQLite failures, allocation failures, schema resolution errors, reserved
  names, unknown tables, and unknown columns continue through existing paths.

## Performance And SQLite Fit

No query path is reimplemented. This is catalog and DDL planning work: default
metadata is validated once, persisted in the MyLite catalog, and materialized
only when DML requests a default value. Physical row writes stay on the existing
SQLite prepared-statement path. No SQLite fork or extension point is required.

## Tests

Add or extend fast C tests under `packages/libmylite/tests/` to cover:

- `CREATE TABLE` with `TINYTEXT`, `TEXT`, `MEDIUMTEXT`, and `LONGTEXT`
  `DEFAULT ('...')` and `DEFAULT ('')`;
- `DEFAULT (NULL)` on nullable and `NOT NULL` `TEXT` family columns, including
  DML materialization diagnostics for the `NOT NULL` case;
- `SHOW COLUMNS`, `DESCRIBE`, `SHOW CREATE TABLE`, and
  `INFORMATION_SCHEMA.COLUMNS` rendering;
- omitted-column `INSERT`, explicit `DEFAULT`, `REPLACE`, and supported
  `UPDATE column = DEFAULT`;
- `ALTER TABLE ... ADD COLUMN ... DEFAULT ('x')` and `ALTER TABLE ...
  MODIFY/CHANGE COLUMN ... DEFAULT ('x')`;
- rejected `ALTER TABLE ... ALTER COLUMN text_col SET DEFAULT ('x')`;
- rejected bare `TEXT DEFAULT 'x'`, unsupported numeric/function/expression
  defaults, unsupported binary/JSON defaults, and still-deferred
  `VARCHAR DEFAULT ('x')`;
- persistence after close/reopen, `CREATE TABLE ... LIKE`, table rename/drop,
  independent file-backed handles, and `.mylite` preamble preservation;
- existing string-default, text-type, DML default, modify/change column, SHOW,
  information-schema, catalog, storage, and lifecycle tests.

