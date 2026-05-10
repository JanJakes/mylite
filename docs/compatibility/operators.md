# Operators

Operators, predicates, assignment forms, and SQL expression syntax that MySQL lists alongside built-ins.

| Function or operator | Status | Notes |
| --- | --- | --- |
| `&` | ❌ | Bitwise AND |
| `>` | 🟡 | Descriptor-driven filtered table `SELECT`, `DELETE`, and `UPDATE` predicates with decimal integer or `TRUE`/`FALSE` right operands only; limited no-source/`DUAL` signed-64 scalar comparison projection; no table-backed expression support |
| `>>` | ❌ | Right shift |
| `>=` | 🟡 | Descriptor-driven filtered table `SELECT`, `DELETE`, and `UPDATE` predicates with decimal integer or `TRUE`/`FALSE` right operands only; limited no-source/`DUAL` signed-64 scalar comparison projection; no table-backed expression support |
| `<` | 🟡 | Descriptor-driven filtered table `SELECT`, `DELETE`, and `UPDATE` predicates with decimal integer or `TRUE`/`FALSE` right operands only; limited no-source/`DUAL` signed-64 scalar comparison projection; no table-backed expression support |
| `<>, !=` | 🟡 | Descriptor-driven filtered table `SELECT`, `DELETE`, and `UPDATE` predicates with decimal integer or `TRUE`/`FALSE` right operands only; limited no-source/`DUAL` signed-64 scalar comparison projection; no table-backed expression support |
| `<<` | ❌ | Left shift |
| `<=` | 🟡 | Descriptor-driven filtered table `SELECT`, `DELETE`, and `UPDATE` predicates with decimal integer or `TRUE`/`FALSE` right operands only; limited no-source/`DUAL` signed-64 scalar comparison projection; no table-backed expression support |
| `<=>` | 🟡 | Descriptor-driven filtered table `SELECT`, `DELETE`, and `UPDATE` predicates with non-`NULL` decimal integer or `TRUE`/`FALSE` right operands only; limited no-source/`DUAL` signed-64 scalar NULL-safe equality projection |
| `%, MOD` | 🟡 | Limited no-source/`DUAL` signed-64 scalar modulo projection with `%`, infix `MOD`, and `MOD(left, right)`; no table-backed expression support |
| `*` | 🟡 | Limited no-source/`DUAL` signed-64 scalar arithmetic projection only; no table-backed expression support |
| `+` | 🟡 | Limited no-source/`DUAL` signed-64 scalar arithmetic projection only; no table-backed expression support |
| `-` (binary) | 🟡 | Limited no-source/`DUAL` signed-64 scalar arithmetic projection only; no table-backed expression support |
| `-` (unary) | 🟡 | Limited no-source/`DUAL` signed-64 scalar unary arithmetic projection with unary `+` and unary `-`; no table-backed expression support or unsigned-width expression results |
| `/` | ❌ | Division operator |
| `:=` | ❌ | Assign a value |
| `=` (assignment) | 🟡 | One unqualified descriptor-column single-table `UPDATE` assignment to a supported decimal integer literal, `TRUE`, `FALSE`, `NULL`, or descriptor-resolved `DEFAULT`; `SET` statements and expression assignments remain unsupported |
| `=` (comparison) | 🟡 | Descriptor-driven filtered table `SELECT`, `DELETE`, and `UPDATE` predicates with non-`NULL` decimal integer or `TRUE`/`FALSE` right operands only; limited no-source/`DUAL` signed-64 scalar comparison projection; no table-backed expression support |
| `^` | ❌ | Bitwise XOR |
| `AND, &&` | 🟡 | Limited descriptor-backed `WHERE` boolean expressions over existing `SELECT`/aggregate-source/`DELETE`/`UPDATE` predicate atoms; `AND` binds tighter than `XOR` and `OR`, `&&` is accepted with deprecation warning 1287, and limited no-source/`DUAL` scalar logical projection admits keyword `AND` only |
| `BETWEEN ... AND ...` | 🟡 | Limited descriptor-backed `WHERE` range predicates for existing `SELECT`/aggregate-source/`DELETE`/`UPDATE` predicate contexts with one descriptor column and two decimal integer or `TRUE`/`FALSE` bounds; no expression operands, string/temporal ranges, row ranges, or full expression-level operator support |
| `BINARY` | ❌ | Cast a string to a binary string |
| `CASE` | ❌ | Case operator |
| `DIV` | 🟡 | Limited no-source/`DUAL` signed-64 scalar integer division projection; no table-backed expression support |
| `EXISTS()` | ❌ | Whether the result of a query contains any rows |
| `IN()` | 🟡 | Limited descriptor-backed `WHERE` membership predicates for existing `SELECT`/aggregate-source/`DELETE`/`UPDATE` predicate contexts with one descriptor column and a nonempty list of decimal integer, `TRUE`/`FALSE`, or `NULL` values; no expression operands, subqueries, row constructors, strings, temporals, or full expression-level operator support |
| `IS` | 🟡 | Descriptor-driven filtered table `SELECT`, `DELETE`, and `UPDATE` predicates for `IS NULL`, `IS TRUE`, `IS FALSE`, and `IS UNKNOWN`; limited no-source/`DUAL` signed-64 scalar projection for `IS NULL`, `IS TRUE`, `IS FALSE`, and `IS UNKNOWN`; no table-backed expression projection, string truth conversion, or unparenthesized scalar `IS` chaining |
| `IS NOT` | 🟡 | Descriptor-driven filtered table `SELECT`, `DELETE`, and `UPDATE` predicates for `IS NOT NULL`, `IS NOT TRUE`, `IS NOT FALSE`, and `IS NOT UNKNOWN`; limited no-source/`DUAL` signed-64 scalar projection for `IS NOT NULL`, `IS NOT TRUE`, `IS NOT FALSE`, and `IS NOT UNKNOWN`; no table-backed expression projection, string truth conversion, or unparenthesized scalar `IS` chaining |
| `IS NOT NULL` | 🟡 | Descriptor-driven filtered table `SELECT`, `DELETE`, and `UPDATE` predicates; limited no-source/`DUAL` scalar projection over admitted signed-64 scalar operands |
| `IS NULL` | 🟡 | Descriptor-driven filtered table `SELECT`, `DELETE`, and `UPDATE` predicates; limited no-source/`DUAL` scalar projection over admitted signed-64 scalar operands |
| `LIKE` | ❌ | Simple pattern matching |
| `NOT, !` | 🟡 | Limited keyword `NOT` over descriptor-backed `WHERE` boolean expressions for existing `SELECT`/aggregate-source/`DELETE`/`UPDATE` predicate atoms; limited no-source/`DUAL` scalar logical projection admits keyword `NOT`; symbolic `!` remains unsupported until MyLite owns its higher-precedence expression semantics and deprecation warning behavior |
| `NOT BETWEEN ... AND ...` | 🟡 | Limited negation of the descriptor-backed `BETWEEN` predicate subset; no expression operands, string/temporal ranges, row ranges, or full expression-level operator support |
| `NOT EXISTS()` | ❌ | Whether the result of a query contains no rows |
| `NOT IN()` | 🟡 | Limited negation of the descriptor-backed `IN` predicate subset, including MySQL three-valued `NULL` list semantics; no expression operands, subqueries, row constructors, strings, temporals, or full expression-level operator support |
| `NOT LIKE` | ❌ | Negation of simple pattern matching |
| `NOT REGEXP` | ❌ | Negation of REGEXP |
| `OR, \|\|` | 🟡 | Limited descriptor-backed `WHERE` boolean expressions over existing `SELECT`/aggregate-source/`DELETE`/`UPDATE` predicate atoms; `OR` binds looser than `XOR` and `AND`, `\|\|` is accepted as logical OR with deprecation warning 1287, and limited no-source/`DUAL` scalar logical projection admits keyword `OR` only, with no scalar `\|\|`, `PIPES_AS_CONCAT`, or general expression-level boolean semantics |
| `REGEXP` | ❌ | Whether string matches regular expression |
| `RLIKE` | ❌ | Whether string matches regular expression |
| `SOUNDS LIKE` | ❌ | Compare sounds |
| `XOR` | 🟡 | Limited descriptor-backed `WHERE` boolean expressions over existing `SELECT`/aggregate-source/`DELETE`/`UPDATE` predicate atoms; `XOR` binds looser than `AND` and tighter than `OR`, propagates `NULL` for the admitted predicate subset, and has limited no-source/`DUAL` scalar logical projection support |
| `\|` | ❌ | Bitwise OR |
| `~` | ❌ | Bitwise inversion |

[Back to compatibility overview](../../COMPATIBILITY.md)
