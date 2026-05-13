# Baseline Integer Expression Defaults Tasks

## Design and Evidence

- [x] Verify MySQL 8.4.9 behavior for parenthesized integer expression
      defaults, metadata, omitted/default DML, `ALTER ... SET DEFAULT`, syntax
      errors, `DEFAULT (NULL)`, and out-of-range materialization behavior.
- [x] Write the independently authored feature specification.
- [x] Add MySQL-runtime expectation script for the supported and intentionally
      deferred behavior.
- [x] Update compatibility docs for the limited expression-default surface.

## Implementation

- [ ] Extend parser support so `DEFAULT (expr)` reaches the AST as a
      parenthesized default value while unparenthesized expressions remain
      syntax errors.
- [ ] Add catalog default kinds for integer expression and `NULL` expression
      defaults, including schema migration from the previous catalog version.
- [ ] Add planner validation/evaluation for the admitted constant integer/`NULL`
      expression subset.
- [ ] Preserve expression-default metadata in descriptor cloning, `SHOW
      COLUMNS`, `SHOW CREATE TABLE`, and `INFORMATION_SCHEMA.COLUMNS`.
- [ ] Materialize expression defaults in omitted-column inserts and supported
      DML `DEFAULT` keyword paths.
- [ ] Add focused parser and runtime C coverage.

## Verification

- [ ] Run `packages/libmylite/tests/mysql_baseline_integer_expression_defaults_expectations.sh`.
- [ ] Run focused parser/default CTest entries.
- [ ] Run existing default, DDL, DML, catalog, file-backed, and parser lifecycle
      tests touched by descriptor defaults.
- [ ] Run `cmake --workflow --preset check`.
- [ ] Review the final diff for MySQL evidence, descriptor authority,
      catalog-schema safety, metadata rendering, DML materialization,
      performance, scope control, cleanup on failure, and compatibility docs.
