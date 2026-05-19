# SQL SET statements

General SET statement, character set, and connection collation shorthand compatibility.

| Feature | Status | Notes |
| --- | --- | --- |
| `SET` | 🟡 | Limited handle-local user-variable assignment lists with `SET @name = value`, `SET @name := value`, comma-separated assignments, case-insensitive 1..64-character user-variable names, supported integer/string/boolean/`NULL`/system-variable/user-variable values, atomic rollback when any assignment in the list fails, and SQL-level prepared statement source/`USING` values; supported system-variable targets may appear in the same assignment list and may restore admitted values from user variables, including common `sql_mode`, `time_zone`, and fixed boolean save/restore forms. System-variable assignment otherwise keeps the existing mutable session-local `sql_mode`, `timestamp`, `time_zone`, `auto_increment_increment`, `auto_increment_offset`, and fixed no-op boolean baselines. No mutable global/persist assignment, assignment outside `SET`, arbitrary expressions, direct parameters, subqueries, `DEFAULT` user-variable values, fractional timestamp values, or SQLite pass-through |
| `SET CHARACTER SET` | 🟡 | Limited `SET CHARACTER SET utf8mb4`, `SET CHARACTER SET DEFAULT`, and `SET CHARSET` synonyms update connection charset readback to `utf8mb4` and reset connection collation readback to `utf8mb4_0900_ai_ci`; no other character sets, conversion, warnings, or general variable assignment |
| `SET NAMES` | 🟡 | Limited `SET NAMES utf8mb4`, quoted-name variants, `SET NAMES DEFAULT`, and `SET NAMES utf8mb4 COLLATE admitted_utf8mb4_collation` update connection charset/collation readback for admitted collations; no other character sets, other collations, conversion, warnings, or general variable assignment |

[Back to compatibility overview](../../COMPATIBILITY.md)
