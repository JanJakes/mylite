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

- [x] Extend the MyLite parser/AST builder to carry an optional `IGNORE` node
  on `INSERT ... SELECT`.
- [x] Accept no-priority, `LOW_PRIORITY`, and `HIGH_PRIORITY` prefixes before
  `IGNORE` for `INSERT ... SELECT`.
- [x] Keep `DELAYED IGNORE ... SELECT`, invalid modifier order, and wider
  execution forms unsupported.
- [x] Reject row-scalar no-source and `FROM DUAL` `INSERT IGNORE ... SELECT`
  sources before mutation.
- [x] Preserve existing table-backed validation and insertion behavior for
  clean rows.

## Tests And Docs

- [x] Add parser tests for accepted and rejected modifier shapes.
- [x] Add runtime tests for successful clean table-backed inserts, warning
  count, affected rows, data visibility, and persistence.
- [x] Add runtime tests proving strict validation errors remain errors in this
  limited slice.
- [x] Update `COMPATIBILITY.md` and `docs/compatibility/sql-table-dml.md`
  without overclaiming full `INSERT IGNORE ... SELECT`.
- [x] Run focused parser/runtime CTest entries.
- [x] Run `cmake --workflow --preset check`.
- [ ] Rerun MySQL 8.4.9 expectation generation/comparison when local Docker is
  responsive again.
