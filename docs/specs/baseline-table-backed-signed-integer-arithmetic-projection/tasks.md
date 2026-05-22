# Baseline Table-Backed Signed Integer Arithmetic Projection Tasks

- [x] Read MyLite architecture, compatibility, row-scalar expression, scalar
  arithmetic, SQLite integration, and engineering-standard context.
- [x] Verify MySQL 8.4.9 behavior for table-backed signed integer arithmetic
  projection, labels, row envelope interaction, warnings, and overflow.
- [x] Write an independently authored feature spec with grammar subset,
  ownership boundaries, diagnostics, generated SQL shape, and unsupported
  behavior.
- [x] Update compatibility documentation for the exact limited supported
  subset.
- [x] Add MySQL-runtime expectation script for supported behavior.
- [x] Add MyLite checked SQLite scalar functions for signed 64-bit arithmetic.
- [x] Extend row-scalar SELECT planning for table-backed signed integer
  arithmetic projection.
- [x] Add fast C runtime coverage and parser coverage if needed.
- [x] Run focused MySQL expectation, focused CTest entries, and
  `cmake --workflow --preset check`.
- [x] Review architecture, performance, memory cleanup, diagnostics, docs, and
  tests before committing.
