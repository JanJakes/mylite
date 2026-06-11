# Parser Corpus Admin And SET Residuals

This slice admits remaining MySQL 8.4.9 administration, table-maintenance,
`DESCRIBE`, and `SET` parser-corpus surfaces that are either directly reusable
with existing MyLite runtime paths or safely represented as explicit embedded
placeholders.

## Sources

- MySQL 8.4 Reference Manual, `SET` syntax:
  <https://dev.mysql.com/doc/refman/8.4/en/set-variable.html>
- MySQL 8.4 Reference Manual, `SHOW` statements:
  <https://dev.mysql.com/doc/refman/8.4/en/show.html>
- MySQL 8.4 Reference Manual, `DESCRIBE` / `EXPLAIN` statements:
  <https://dev.mysql.com/doc/refman/8.4/en/explain.html>
- MySQL 8.4 Reference Manual, `ANALYZE TABLE`:
  <https://dev.mysql.com/doc/refman/8.4/en/analyze-table.html>
- MySQL 8.4 Reference Manual, `OPTIMIZE TABLE`:
  <https://dev.mysql.com/doc/refman/8.4/en/optimize-table.html>
- Runtime evidence:
  `packages/libmylite/tests/mysql_parser_corpus_admin_set_residuals_expectations.sh`
  against MySQL 8.4.9.

## Scope

Implemented in this slice:

- `ANALYZE TABLES ...` and `OPTIMIZE TABLES ...` are accepted as plural aliases
  for the existing table-maintenance AST and runtime.
- `DESCRIBE table_name column_name` and `DESCRIBE table_name 'pattern'` are
  accepted and execute through the existing `SHOW COLUMNS` metadata path.
  The unquoted column-name form is treated as a literal LIKE pattern for the
  `Field` column, matching MySQL's descriptor behavior.
- `DESCRIBE SELECT ...` and `EXPLAIN ANALYZE` DML variants are admitted through
  MyLite's existing deterministic EXPLAIN placeholder result surface. MyLite
  does not yet synthesize MySQL-shaped optimizer rows.
- `SHOW EXTENDED COLUMNS`, `SHOW EXTENDED FULL COLUMNS`, and
  `SHOW EXTENDED INDEX` are accepted and execute through the same metadata
  paths as the non-`EXTENDED` forms because MyLite has no hidden generated
  columns or hidden indexes in this subset.
- `SHOW ENGINE engine LOGS|MUTEX` and similar valid-but-unimplemented SHOW
  server surfaces are admitted as unsupported utility placeholders.
- `SET` residuals containing persisted-variable forms, scoped system-variable
  reads in assignment values, function expression values, quoted
  `optimizer_switch` values, and assignment-operator variants are admitted
  through existing supported SET execution when possible or as explicit
  unsupported utility placeholders when full runtime semantics are not yet
  implemented.

Out of scope:

- Legacy MySQL syntax removed from MySQL 8.4, such as `SHOW MASTER STATUS` and
  `SHOW SLAVE STATUS`, remains outside MySQL 8.4 compatibility. The corpus may
  still contain those statements, but this slice does not make them successful
  MyLite syntax.
- Real `EXPLAIN` / `DESCRIBE SELECT` optimizer row synthesis.
- Server log or mutex result sets for non-InnoDB engines.
- Persistent system-variable storage, persisted config files, or mixed
  statement side effects such as applying one normal assignment while ignoring
  a later `PERSIST` assignment.

## MySQL 8.4.9 Runtime Observations

The expectation script verifies:

- plural `ANALYZE TABLES` and `OPTIMIZE TABLES` are accepted by MySQL;
- `DESCRIBE t f1` returns only column `f1`;
- `DESCRIBE t 'f%'` applies pattern matching;
- `DESCRIBE SELECT ...` and `EXPLAIN ANALYZE` DML are valid syntax;
- `SHOW ENGINE CSV LOGS`, `SHOW ENGINE CSV MUTEX`, and
  `SHOW ENGINE MYISAM MUTEX` are accepted syntax;
- `SHOW EXTENDED INDEX FROM t`, `SHOW EXTENDED COLUMNS FROM t`, and
  `SHOW EXTENDED FULL COLUMNS FROM t` are accepted syntax;
- `SET @@time_zone := 'UTC'` uses the assignment operator and updates the
  session variable.

## MyLite Parser Patterns

The direct Lemon grammar already covers the base `ANALYZE TABLE`,
`OPTIMIZE TABLE`, `DESCRIBE table`, `EXPLAIN table`, `SHOW COLUMNS`, and
`SHOW INDEX` forms. This slice keeps the main grammar small by adding targeted
parser-fallback AST builders for these complete residual patterns after normal
Lemon parsing fails:

- `ANALYZE [NO_WRITE_TO_BINLOG|LOCAL] TABLES table_name [, ...]`;
- `OPTIMIZE [NO_WRITE_TO_BINLOG|LOCAL] TABLES table_name [, ...]`;
- `DESCRIBE|DESC table_name identifier|string_pattern`;
- `EXPLAIN table_name identifier|string_pattern`;
- `SHOW EXTENDED [FULL] COLUMNS|FIELDS FROM|IN table_name ...`;
- `SHOW EXTENDED INDEX|INDEXES|KEYS FROM|IN table_name ...`;
- `SET @@system_variable := scalar_value`.

Broader SHOW, DESCRIBE, EXPLAIN, and SET expression surfaces are currently
classified by MyLite's placeholder scanner after normal grammar parsing fails.
That scanner admits only balanced, non-obviously-incomplete statements so
malformed tails continue to report syntax errors.

## Runtime Semantics

Plural table-maintenance aliases reuse the existing `ANALYZE TABLE` and
`OPTIMIZE TABLE` runtime.

`DESCRIBE table filter` reuses the existing `SHOW COLUMNS` result columns and
metadata row builder. Identifier filters are decoded with the same identifier
normalization used for schema, table, and column names, then matched as a SHOW
LIKE pattern.

`DESCRIBE SELECT ...` and `EXPLAIN ANALYZE` DML residuals execute through the
existing deterministic EXPLAIN placeholder result path. Unsupported SHOW and
SET residuals execute through the existing unsupported-utility diagnostic path.
Admin no-op behavior is used only for server-only statements that MyLite
already classifies as embedded no-ops; mixed `SET` statements that could
otherwise have side effects are not silently partially applied.

No SQLite fork changes, file-format changes, catalog tables, or new
dependencies are introduced.

## Tests

- `parser_corpus_admin_set_residuals_test.c` covers parser admission, AST kind
  classification, executable table-maintenance and `DESCRIBE` syntax, and
  malformed legacy or incomplete surfaces.
- `runtime_parser_corpus_admin_set_residuals_test.c` verifies
  table-maintenance execution, `DESCRIBE` filtering, unsupported diagnostics,
  and supported `SET @@time_zone := ...` execution.
- `mysql_parser_corpus_admin_set_residuals_expectations.sh` records the
  MySQL 8.4.9 behavior used by the tests.

## Compatibility Status

This slice improves MySQL 8.4-compatible parser and runtime behavior for
administration and SET residuals. It intentionally leaves removed legacy
statements and true optimizer/server result synthesis outside the supported
surface.
