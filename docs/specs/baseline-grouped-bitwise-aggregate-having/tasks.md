# Baseline Grouped Bitwise Aggregate HAVING Tasks

- [x] Record MySQL 8.4.9 expectations for selected bitwise aggregate aliases
  and repeated selected bitwise aggregate expressions in grouped `HAVING`.
- [x] Compare bitwise aggregate `HAVING` operands with unsigned decimal
  order-key semantics, including nonnegative literals above signed 64-bit range.
- [x] Add runtime tests for `BIT_AND()`, `BIT_OR()`, `BIT_XOR()`, row-scalar
  bitwise arguments, and unsupported negative comparison literals.
- [x] Update compatibility documentation and related grouped aggregate specs.
- [x] Run focused MySQL expectations, focused CTest, format/check gates, and
  release-gate review before commit.
