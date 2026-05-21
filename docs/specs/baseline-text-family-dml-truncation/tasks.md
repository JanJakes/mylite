# Baseline Text-Family DML Truncation Tasks

- [x] Verify MySQL 8.4.9 behavior for strict, non-strict, `INSERT IGNORE`,
      trailing-space, `UPDATE`, `REPLACE`, UTF-8 boundary, and `INSERT ... SELECT`
      text-family truncation cases.
- [x] Specify scope, grammar impact, conversion semantics, diagnostics,
      architecture ownership, and compatibility documentation requirements.
- [x] Extend MySQL expectation coverage in
      `packages/libmylite/tests/mysql_baseline_text_type_expectations.sh`.
- [ ] Extend C runtime coverage in
      `packages/libmylite/tests/runtime_text_type_test.c`.
- [ ] Implement descriptor-owned text-family truncation conversion for admitted
      `INSERT`, `REPLACE`, `UPDATE`, and `INSERT ... SELECT` paths.
- [ ] Update compatibility docs for the exact supported subset.
- [ ] Run focused build/tests and MySQL expectation script.
- [ ] Run `cmake --workflow --preset check`.
- [ ] Review the diff for descriptor authority, diagnostics, warning counts,
      row-count behavior, UTF-8 prefix safety, file-format safety, and scope
      control.
