# Baseline GROUP_CONCAT Max Length System Variable Tasks

- [x] Research MySQL 8.4 documentation and MySQL 8.4.9 runtime behavior for
      `group_concat_max_len` and capped `GROUP_CONCAT()` output.
- [x] Write the independently authored feature specification.
- [ ] Add MySQL 8.4.9 expectation probes for reads, assignment conversion,
      truncation, warnings, and unsupported forms.
- [ ] Add handle-local session storage and initialization for
      `group_concat_max_len`.
- [ ] Register `group_concat_max_len` in scalar system-variable and
      `SHOW VARIABLES` paths.
- [ ] Implement session-local `SET group_concat_max_len` parsing, clamping,
      diagnostics, and multi-assignment rollback.
- [ ] Add a MyLite-owned SQLite aggregate for capped `GROUP_CONCAT()`.
- [ ] Switch generated `GROUP_CONCAT()` SQL to the MyLite aggregate while
      preserving descriptor-derived identifiers and bound parameters.
- [ ] Add focused C runtime coverage for variable behavior, aggregate caps,
      warnings, file safety, persistence, and independent handles.
- [ ] Update compatibility documentation without overclaiming full
      `GROUP_CONCAT()` or mutable global system variables.
- [ ] Run focused expectation/C tests and the full check workflow.
- [ ] Review the final diff for architecture boundaries, MySQL evidence,
      warning semantics, memory behavior, UTF-8 truncation, and scope control.
