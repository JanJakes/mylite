# SQL SET statements

General SET statement, character set, and connection collation shorthand compatibility.

| Feature | Status | Notes |
| --- | --- | --- |
| `SET` | 🟡 | Limited fixed no-op session/local system-variable assignments preserving MyLite's current boolean baselines plus mutable session-local `SET sql_mode` / `SET SESSION sql_mode` / `SET @@[SESSION\|LOCAL.]sql_mode` assignment from `DEFAULT` or string literals and limited session `timestamp` integer/default assignment for statement-time testing and automatic temporal values; no mutable global/persist assignment, user variables, assignment lists, `:=`, arbitrary expressions, parameters, subqueries, fractional timestamp values, mutable time-zone state, or SQLite pass-through |
| `SET CHARACTER SET` | 🟡 | Limited fixed no-op `SET CHARACTER SET utf8mb4`, `SET CHARACTER SET DEFAULT`, and `SET CHARSET` synonyms preserve MyLite's fixed `utf8mb4` / `utf8mb4_0900_ai_ci` connection baseline; no mutable charset state, other character sets, conversion, warnings, or general variable assignment |
| `SET NAMES` | 🟡 | Limited fixed no-op `SET NAMES utf8mb4`, quoted-name variants, `SET NAMES DEFAULT`, and `SET NAMES utf8mb4 COLLATE utf8mb4_0900_ai_ci`; no mutable charset/collation state, other character sets, other collations, conversion, warnings, or general variable assignment |

[Back to compatibility overview](../../COMPATIBILITY.md)
