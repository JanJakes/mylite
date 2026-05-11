# Numeric and math functions

Numeric conversion, arithmetic, trigonometric, random, and rounding functions.

| Function or operator | Status | Notes |
| --- | --- | --- |
| `ABS()` | 🟡 | Limited no-source/`DUAL` scalar projection and limited `DO` expression execution over integer/boolean/`NULL`, signed-64 scalar arithmetic, and limited unsigned-64 numeric bitwise operands; direct decimal integer magnitudes are admitted within the unsigned-64 envelope except `ABS(-9223372036854775808)` reports MySQL overflow; no table-backed expression support, string/decimal/float/hex/bit conversion, or expression metadata |
| `ACOS()` | ❌ | Return arc cosine |
| `ASIN()` | ❌ | Return arc sine |
| `ATAN()` | ❌ | Return arc tangent |
| `ATAN2(), ATAN()` | ❌ | Return arc tangent of the two arguments |
| `BIN()` | ❌ | Return a string containing binary representation of a number |
| `BIT_COUNT()` | 🟡 | Limited no-source/`DUAL` scalar projection and limited `DO` expression execution over integer/boolean/`NULL`, signed-64 scalar arithmetic, and limited unsigned-64 numeric bitwise operands; no table-backed expression support or binary-string bit counting |
| `CEIL()` | 🟡 | Limited no-source/`DUAL` scalar projection and limited `DO` expression execution over integer/boolean/`NULL`, direct decimal integer literals up to 81 significant digits, signed-64 scalar arithmetic, and limited unsigned-64 numeric bitwise operands; no decimal/float rounding, table-backed expression support, string/hex/bit conversion, or expression metadata |
| `CEILING()` | 🟡 | Synonym for the limited `CEIL()` subset |
| `CONV()` | ❌ | Convert numbers between different number bases |
| `COS()` | ❌ | Return cosine |
| `COT()` | ❌ | Return cotangent |
| `CRC32()` | ❌ | Compute a cyclic redundancy check value |
| `DEGREES()` | ❌ | Convert radians to degrees |
| `EXP()` | ❌ | Raise to the power of |
| `FLOOR()` | 🟡 | Limited no-source/`DUAL` scalar projection and limited `DO` expression execution over integer/boolean/`NULL`, direct decimal integer literals up to 81 significant digits, signed-64 scalar arithmetic, and limited unsigned-64 numeric bitwise operands; no decimal/float rounding, table-backed expression support, string/hex/bit conversion, or expression metadata |
| `FORMAT()` | ❌ | Localized decimal formatting |
| `LN()` | ❌ | Return natural logarithm of the argument |
| `LOG()` | ❌ | Return natural logarithm of the first argument |
| `LOG10()` | ❌ | Return base-10 logarithm of the argument |
| `LOG2()` | ❌ | Return base-2 logarithm of the argument |
| `MOD()` | 🟡 | Limited no-source/`DUAL` signed-64 scalar modulo projection and limited `DO` expression execution with `MOD(left, right)`, `%`, and infix `MOD`; no table-backed expression support |
| `OCT()` | ❌ | Return a string containing octal representation of a number |
| `PI()` | ❌ | Return value of pi |
| `POW()` | ❌ | Return argument raised to the specified power |
| `POWER()` | ❌ | Return argument raised to the specified power |
| `RADIANS()` | ❌ | Return argument converted to radians |
| `RAND()` | ❌ | Return a random floating-point value |
| `ROUND()` | ❌ | Round the argument |
| `SIGN()` | 🟡 | Limited no-source/`DUAL` scalar projection and limited `DO` expression execution over integer/boolean/`NULL`, direct exact-decimal integer literals, signed-64 scalar arithmetic, and limited unsigned-64 numeric bitwise operands; no table-backed expression support, string/decimal/float/hex/bit conversion, or expression metadata |
| `SIN()` | ❌ | Return sine of the argument |
| `SQRT()` | ❌ | Return square root of the argument |
| `TAN()` | ❌ | Return tangent of the argument |
| `TRUNCATE()` | ❌ | Truncate to specified number of decimal places |

[Back to compatibility overview](../../COMPATIBILITY.md)
