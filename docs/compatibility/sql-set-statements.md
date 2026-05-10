# SQL SET statements

General SET statement, character set, and connection collation shorthand compatibility.

| Feature | Status | Notes |
| --- | --- | --- |
| `SET` | ❌ | Variable assignment forms |
| `SET CHARACTER SET` | 🟡 | Limited fixed no-op `SET CHARACTER SET utf8mb4`, `SET CHARACTER SET DEFAULT`, and `SET CHARSET` synonyms preserve MyLite's fixed `utf8mb4` / `utf8mb4_0900_ai_ci` connection baseline; no mutable charset state, other character sets, conversion, warnings, or general variable assignment |
| `SET NAMES` | 🟡 | Limited fixed no-op `SET NAMES utf8mb4`, quoted-name variants, `SET NAMES DEFAULT`, and `SET NAMES utf8mb4 COLLATE utf8mb4_0900_ai_ci`; no mutable charset/collation state, other character sets, other collations, conversion, warnings, or general variable assignment |

[Back to compatibility overview](../../COMPATIBILITY.md)
