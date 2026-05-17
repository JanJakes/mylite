# Baseline Insert Ignore Select Tasks

## Design

- [x] Review the existing insert-select, insert-ignore, and insert-modifier
  specs.
- [x] Confirm official MySQL 8.4 syntax admits `IGNORE` on `INSERT ... SELECT`
  with `LOW_PRIORITY` / `HIGH_PRIORITY`.
- [x] Reuse the existing MySQL 8.4.9 expectation artifact for the clean
  `LOW_PRIORITY IGNORE ... SELECT` case.
- [x] Document the limited clean-row scope and explicitly defer full warning
  demotion.

## Implementation

- [ ] Extend the MyLite parser/AST builder to carry an optional `IGNORE` node
  on `INSERT ... SELECT`.
- [ ] Accept no-priority, `LOW_PRIORITY`, and `HIGH_PRIORITY` prefixes before
  `IGNORE` for `INSERT ... SELECT`.
- [ ] Keep `DELAYED IGNORE ... SELECT`, invalid modifier order, and wider
  grammar forms unsupported.
- [ ] Reject row-scalar no-source and `FROM DUAL` `INSERT IGNORE ... SELECT`
  sources before mutation.
- [ ] Preserve existing table-backed validation and insertion behavior for
  clean rows.

## Tests And Docs

- [ ] Add parser tests for accepted and rejected modifier shapes.
- [ ] Add runtime tests for successful clean table-backed inserts, warning
  count, affected rows, data visibility, and persistence.
- [ ] Add runtime tests proving strict validation errors remain errors in this
  limited slice.
- [ ] Update `COMPATIBILITY.md` and `docs/compatibility/sql-table-dml.md`
  without overclaiming full `INSERT IGNORE ... SELECT`.
- [ ] Run focused parser/runtime CTest entries.
- [ ] Run `cmake --workflow --preset check`.
- [ ] Rerun MySQL 8.4.9 expectation generation/comparison when local Docker is
  responsive again.
