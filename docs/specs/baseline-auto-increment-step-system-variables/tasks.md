# Baseline AUTO_INCREMENT Step System Variables Tasks

- [x] Read project architecture, compatibility, system-variable, and
  auto-increment lifecycle context.
- [x] Verify MySQL 8.4.9 behavior for variable readback, assignment,
  warnings, generated allocation, table lower bounds, explicit inserts, and
  updates.
- [x] Specify the supported session-variable and allocation subset.
- [x] Add MySQL 8.4.9 expectation script for the supported and deferred
  user-visible behavior.
- [x] Add runtime/session state support for the two variables.
- [x] Add scalar `@@`, `SHOW VARIABLES`, and `SET` runtime behavior.
- [x] Extend auto-increment allocation and counter advancement to honor the
  supported increment/offset series.
- [x] Add focused C runtime tests and CMake registration.
- [x] Update `COMPATIBILITY.md` and detailed compatibility docs.
- [x] Run focused tests, MySQL expectation script, and the full check workflow.
- [x] Review, commit, push, and run the feature review gate.
