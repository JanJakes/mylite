# Numeric and math functions

Numeric conversion, arithmetic, trigonometric, random, and rounding functions.

In this table, "row-backed numeric contexts" means single-table row-scalar
projection, supported comparison predicates, `ORDER BY` expression keys,
arithmetic/control-flow operands, and nested covered numeric calls over numeric
descriptor columns, integer/boolean/`NULL` literals, and supported row-scalar
numeric operands. Row-backed execution still normalizes negative-zero
floating-point display through SQLite result extraction.

| Function or operator | Status | Notes |
| --- | --- | --- |
| `/` | 🟡 | Limited no-source/`DUAL` top-level scalar division and limited `DO` expression execution over signed-64 scalar arithmetic operands, returning four fractional digits for integer/integer inputs; no nested slash composition, table-backed expression support, decimal/float/string/hex/bit conversion, or expression metadata |
| `ABS()` | 🟡 | Scalar and row-backed numeric contexts; `ABS(-9223372036854775808)` reports MySQL overflow; gaps: broad coercion, row warning rows, and exact expression metadata |
| `ACOS()` | 🟡 | Scalar and row-backed numeric contexts; values outside `-1..1` return `NULL` without row warning rows; gaps: broad coercion and exact expression metadata |
| `ASIN()` | 🟡 | Scalar and row-backed numeric contexts; values outside `-1..1` return `NULL` without row warning rows; gaps: broad coercion and exact expression metadata |
| `ATAN()` | 🟡 | Scalar and row-backed numeric contexts for one-argument `ATAN(value)` and MySQL-accepted one-argument `ATAN2(value)`; gaps: broad coercion, row warning rows, and exact expression metadata |
| `ATAN2(), ATAN()` | 🟡 | Scalar and row-backed numeric contexts for two-argument `ATAN(y, x)` / `ATAN2(y, x)`; gaps: broad coercion, row warning rows, and exact expression metadata |
| `BIN()` | 🟡 | Limited no-source/`DUAL` scalar projection and limited `DO` expression execution over integer/boolean/`NULL`, direct decimal integer literals in the admitted unsigned/signed envelopes, signed-64 scalar arithmetic, and limited unsigned-64 numeric bitwise operands; returns unsigned 64-bit base-2 text with no leading zeroes; no table-backed expression support, string/decimal/float/hex/bit conversion, or expression metadata |
| `BIT_COUNT()` | 🟡 | Scalar and row-backed numeric contexts over integer-domain operands; gaps: binary-string bit counting, broad coercion, row warning rows, and exact expression metadata |
| `CEIL()` | 🟡 | Scalar and row-backed numeric contexts; gaps: full decimal/string/hex/bit coercion, row warning rows, and exact expression metadata |
| `CEILING()` | 🟡 | Synonym for the limited `CEIL()` subset |
| `CONV()` | 🟡 | Limited no-source/`DUAL` scalar projection and limited `DO` expression execution over integer/boolean/`NULL`, direct decimal integer literals in the admitted signed/unsigned value envelope, signed-64 scalar arithmetic, and limited unsigned-64 numeric bitwise value operands; `from_base` and `to_base` are signed-64 scalar arithmetic values with absolute range `2..36`; returns uppercase base-conversion text with MySQL signed/unsigned output behavior; no table-backed expression support, string/decimal/float/hex/bit conversion, or expression metadata |
| `COS()` | 🟡 | Scalar and row-backed numeric contexts; gaps: broad coercion, row warning rows, and exact expression metadata |
| `COT()` | 🟡 | Scalar and row-backed numeric contexts; zero-valued inputs report `1690 / 22003`; gaps: broad coercion, row warning rows, and exact expression metadata |
| `CRC32()` | 🟡 | Limited no-source/`DUAL` scalar projection and limited `DO` expression execution for `CRC32(value)` over string, hex, integer, boolean, and `NULL` literals; returns MySQL-compatible unsigned CRC-32 decimal text; no table-backed expression support, character-set conversion, parameters, or expression metadata |
| `DEGREES()` | 🟡 | Scalar and row-backed numeric contexts; gaps: broad coercion, row warning rows, and exact expression metadata |
| `EXP()` | 🟡 | Scalar and row-backed numeric contexts; overflow reports MySQL-compatible double out-of-range diagnostics; gaps: broad coercion, row warning rows, and exact expression metadata |
| `FLOOR()` | 🟡 | Scalar and row-backed numeric contexts; gaps: full decimal/string/hex/bit coercion, row warning rows, and exact expression metadata |
| `FORMAT()` | 🟡 | Limited no-source/`DUAL` scalar projection and limited `DO` expression execution for `FORMAT(value, decimals)` over exact integer/fixed-decimal/boolean/`NULL` value literals and integer/boolean/`NULL` decimal-place literals, using MySQL-style comma grouping and half-away-from-zero rounding with decimal places clamped to `0..30`; three-argument locale form is parsed but rejected, with no table-backed expression support, string/float/hex/bit conversion, or expression metadata |
| `LN()` | 🟡 | Scalar and row-backed numeric contexts; nonpositive input returns `NULL`, with warning rows still missing in row-backed execution; gaps: broad coercion and exact expression metadata |
| `LOG()` | 🟡 | Scalar and row-backed numeric contexts for one- and two-argument forms; invalid bases or values return `NULL`, with warning rows still missing in row-backed execution; gaps: broad coercion and exact expression metadata |
| `LOG10()` | 🟡 | Scalar and row-backed numeric contexts; nonpositive input returns `NULL`, with warning rows still missing in row-backed execution; gaps: broad coercion and exact expression metadata |
| `LOG2()` | 🟡 | Scalar and row-backed numeric contexts; nonpositive input returns `NULL`, with warning rows still missing in row-backed execution; gaps: broad coercion and exact expression metadata |
| `MOD()` | 🟡 | Limited no-source/`DUAL` signed-64 scalar modulo projection and limited `DO` expression execution with `MOD(left, right)`, `%`, and infix `MOD`; no table-backed expression support |
| `OCT()` | 🟡 | Limited no-source/`DUAL` scalar projection and limited `DO` expression execution over integer/boolean/`NULL`, direct decimal integer literals in the admitted unsigned/signed envelopes, signed-64 scalar arithmetic, and limited unsigned-64 numeric bitwise operands; returns unsigned 64-bit base-8 text with no leading zeroes; no table-backed expression support, string/decimal/float/hex/bit conversion, or expression metadata |
| `PI()` | 🟡 | Limited no-source/`DUAL` scalar projection and limited `DO` expression execution as a top-level pi constant returning MySQL's default visible text `3.141593`; no table-backed expression support, nesting, approximate arithmetic, or expression metadata |
| `POW()` | 🟡 | Scalar and row-backed numeric contexts for `POW(value, exponent)`; overflow reports MySQL-compatible double out-of-range diagnostics; gaps: broad coercion, row warning rows, and exact expression metadata |
| `POWER()` | 🟡 | Synonym for the limited `POW()` subset |
| `RADIANS()` | 🟡 | Scalar and row-backed numeric contexts; gaps: broad coercion, row warning rows, and exact expression metadata |
| `RAND()` | 🟡 | Limited no-source/`DUAL` scalar projection, limited `DO` expression execution, single-table row-scalar `SELECT` projection, limited single-table `WHERE RAND(...) <literal comparison>` predicates, WordPress-style string-target `INSERT` row values and single-row `UPDATE` values, and single-key `SELECT ... ORDER BY RAND()` / `RAND(seed)` with optional direction and supported `LIMIT`; table-backed seed literals admit integer, boolean, and `NULL`, and table-backed descriptor seeds additionally admit warning-free integer `CAST`/`CONVERT` column expressions, while no-source scalar seed coercion additionally admits fixed/approximate numeric literals rounded to integer seeds, ordinary string literals converted with MySQL-style integer-prefix warnings, and existing scalar `NULLIF()` plus integer `CAST`/`CONVERT` seed expressions; values are doubles in `[0, 1)`, seeded table-backed integer-literal expressions advance per row, and descriptor-derived seeds reinitialize per row. No joined/grouped/compound query support, multiple random order keys, random integer expressions, warning-producing table-backed seed conversion, numeric/binary/temporal DML targets, full multi-row randomized DML parity, hex/bit seed conversion, replication semantics, or broad expression metadata |
| `ROUND()` | 🟡 | Scalar and row-backed numeric contexts for one- and two-argument forms; gaps: full exact-decimal rounding/coercion, row warning rows, and exact expression metadata |
| `SIGN()` | 🟡 | Scalar and row-backed numeric contexts; gaps: broad coercion, row warning rows, and exact expression metadata |
| `SIN()` | 🟡 | Scalar and row-backed numeric contexts; gaps: broad coercion, row warning rows, and exact expression metadata |
| `SQRT()` | 🟡 | Scalar and row-backed numeric contexts; negative inputs return `NULL` without row warning rows; gaps: broad coercion and exact expression metadata |
| `TAN()` | 🟡 | Scalar and row-backed numeric contexts; gaps: broad coercion, row warning rows, and exact expression metadata |
| `TRUNCATE()` | 🟡 | Limited no-source/`DUAL` scalar projection and limited `DO` expression execution for `TRUNCATE(value, decimals)` over exact integer/fixed-decimal/boolean/`NULL` value literals and integer/boolean/`NULL` decimal-place literals, truncating toward zero with decimal places clamped to MySQL's visible supported envelope; no table-backed expression support, string/float/hex/bit conversion, or expression metadata |

[Back to compatibility overview](../../COMPATIBILITY.md)
