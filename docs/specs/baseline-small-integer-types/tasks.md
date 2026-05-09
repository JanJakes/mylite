# Baseline Small Integer Types Tasks

## Design And Evidence

- [x] Verify `packages/libmylite/tests/mysql_baseline_small_integer_types_expectations.sh`
      against MySQL 8.4.9.
- [x] Keep the feature scope limited to `TINYINT`, `SMALLINT`, and
      `MEDIUMINT`, each optionally `UNSIGNED`.
- [x] Confirm no SQLite fork patch or file-format change is required.

## Parser And AST

- [x] Add AST integer-family enum values for `TINYINT`, `SMALLINT`, and
      `MEDIUMINT`.
- [x] Map lexer keywords to parser tokens for those type names.
- [x] Extend `integer_type` grammar for signed and unsigned variants.
- [x] Add parser tests for `CREATE TABLE`, `ALTER TABLE ADD`, `MODIFY`, and
      `CHANGE` forms using the new families.
- [x] Preserve deterministic rejection for display width, `SIGNED`, `ZEROFILL`,
      `BOOL` / `BOOLEAN`, and alias forms.

## Runtime And Catalog

- [x] Map new AST integer families to logical descriptor text and physical
      `INTEGER` storage.
- [x] Extend descriptor range conversion for all new signed and unsigned
      ranges.
- [x] Ensure `INSERT ... VALUES`, `INSERT ... SET`, and `UPDATE` assignment
      conversion uses the new ranges.
- [x] Ensure supported `SELECT`, `DELETE`, and `UPDATE` predicate conversion
      uses the new ranges.
- [x] Ensure ordered reads and ordered limited deletes/updates work over the
      new families without custom sorting.
- [x] Ensure `ALTER TABLE ... ADD`, `MODIFY`, and `CHANGE` use the new logical
      descriptors and validate existing rows before narrowing commits.
- [x] Preserve catalog generation, descriptor version, SQLite schema generation,
      and physical-name behavior already specified by each lifecycle slice.

## Introspection

- [x] Render `SHOW COLUMNS` / `DESCRIBE` / `EXPLAIN table` lower-case type
      text for each new logical descriptor.
- [x] Render `SHOW CREATE TABLE` lower-case type text for each new logical
      descriptor.
- [x] Preserve unsupported diagnostics for unknown future descriptor types.

## Tests

- [x] Extend runtime create/table lifecycle coverage for descriptors and
      physical storage.
- [x] Extend row-values and insert-set coverage for boundaries, `NULL`,
      not-null diagnostics, out-of-range diagnostics, atomicity, persistence,
      and independent handles.
- [x] Extend select-where, select-order-limit, delete, and update coverage for
      predicates, ordering, limits, assignments, and out-of-range literals.
- [x] Extend alter add/drop/rename/modify/change coverage for descriptors,
      existing-row validation, rebuild safety, row preservation, rollback, and
      introspection.
- [x] Extend show columns, show create table, and explain table coverage for
      type text.
- [x] Register any new test binaries in `packages/libmylite/CMakeLists.txt`.

## Compatibility Docs

- [x] Update `COMPATIBILITY.md`.
- [x] Update `docs/compatibility/type-system-literals-conversion.md`.
- [x] Update table DDL/DML/query-expression docs only where integer-family
      wording changes.
- [x] Avoid claiming display width, `SIGNED`, `ZEROFILL`, `BOOL` / `BOOLEAN`,
      vendor aliases, compact storage, protocol metadata, or general
      expression semantics.

## Verification

- [x] `cmake --build --preset dev`
- [x] Focused parser/runtime CTest entries touched by this feature.
- [x] `./packages/libmylite/tests/mysql_baseline_small_integer_types_expectations.sh`
- [x] `cmake --workflow --preset check`
- [x] Final architecture/self-review for descriptor authority, range
      correctness, physical SQLite handling, docs accuracy, and test relevance.
