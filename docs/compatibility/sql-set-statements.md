# SQL SET statements

General SET statement, character set, and connection collation shorthand compatibility.

The parser admits selected expression-shaped `SET` values seen in MySQL
compatibility corpora, including generic function calls, `LOG10()`,
`UNIX_TIMESTAMP()`, `CONVERT(... USING ...)`, variable-rooted arithmetic, and
scalar subqueries. Execution remains limited to the documented variable
assignment evaluator; unsupported values fail with MyLite diagnostics rather
than being silently evaluated.

| Feature | Status | Notes |
| --- | --- | --- |
| `SET` | 🟡 | Limited handle-local user-variable assignment lists with `SET @name = value`, `SET @name := value`, comma-separated assignments, case-insensitive 1..64-character user-variable names, supported integer/fixed-decimal/floating/string/hex/bit/charset-introduced/boolean/`NULL`/system-variable/user-variable values, limited source-free variable-rooted arithmetic, zero-argument `UNIX_TIMESTAMP()`, atomic rollback when any assignment in the list fails, and SQL-level prepared statement source/`USING` values; supported system-variable targets may appear in the same assignment list and may restore admitted values from user variables, including common `sql_mode`, `time_zone`, and fixed boolean save/restore forms. System-variable assignment otherwise keeps the existing mutable session-local `sql_mode`, `timestamp` including `@@timestamp` plus integer arithmetic only inside `SET timestamp`, `time_zone`, `auto_increment_increment`, `auto_increment_offset`, and fixed no-op boolean baselines. No mutable global/persist assignment, user-variable assignment outside `SET` except top-level supported `SELECT ... INTO @vars` and limited deprecated scalar `@name := expression`, arbitrary `SET` expressions outside the documented scalar subset, general system-variable scalar arithmetic, direct parameters, subqueries, `DEFAULT` user-variable values, charset conversion for introduced literals, nonzero fractional timestamp assignment values, or SQLite pass-through |
| `SET CHARACTER SET` | 🟡 | Limited admitted catalog charsets, including `utf8mb4`, focused legacy charset readback values, `binary`, `DEFAULT`, and `SET CHARSET` synonyms, update connection charset/collation readback; optional comma-separated tail assignments are applied atomically through the supported `SET` assignment executor. No charset conversion, protocol negotiation, warnings beyond underlying tail assignments, or full mutable charset semantics |
| `SET NAMES` | 🟡 | Limited admitted catalog charsets, quoted-name variants, `DEFAULT`, `binary`, and `COLLATE admitted_collation` update connection charset/collation readback; optional comma-separated tail assignments are applied atomically through the supported `SET` assignment executor. No charset conversion, protocol negotiation, warnings beyond underlying tail assignments, or full mutable charset semantics |

[Back to compatibility overview](../../COMPATIBILITY.md)
