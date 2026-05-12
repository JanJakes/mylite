# Baseline SQL Mode Session State Tasks

- [x] Verify MySQL 8.4.9 behavior for the listed `SET sql_mode` forms,
  canonical output, warnings, invalid names, duplicate modes, combination
  modes, and scope reads.
- [x] Write the independently authored feature spec with MyLite grammar
  snippets, ownership boundaries, supported effects, non-goals, diagnostics,
  and test plan.
- [ ] Add session SQL-mode storage helpers, canonicalization, and default
  initialization.
- [ ] Feed session lexer mode flags into `mylite_execute()`.
- [ ] Implement `SET sql_mode` session assignment, validation, warnings, and
  scalar/SHOW readback with fixed global value.
- [ ] Apply supported effects for `NO_BACKSLASH_ESCAPES`,
  `NO_AUTO_VALUE_ON_ZERO`, and `REAL_AS_FLOAT`.
- [ ] Add/update C runtime tests and MySQL expectation scripts.
- [ ] Update compatibility docs for the exact supported subset and deferred
  mode effects.
- [ ] Run focused build/tests plus full `cmake --workflow --preset check`.
- [ ] Review the diff for architecture, MySQL evidence, session/file
  isolation, diagnostics, mode-effect scope, and test relevance.
