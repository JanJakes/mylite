# Operators

Operators, predicates, assignment forms, and SQL expression syntax that MySQL lists alongside built-ins.

| Function or operator | Status | Notes |
| --- | --- | --- |
| `&` | ❌ | Bitwise AND |
| `>` | 🟡 | Descriptor-driven filtered table `SELECT`, `DELETE`, and `UPDATE` predicates with decimal integer or `TRUE`/`FALSE` right operands only; no expression-level operator support |
| `>>` | ❌ | Right shift |
| `>=` | 🟡 | Descriptor-driven filtered table `SELECT`, `DELETE`, and `UPDATE` predicates with decimal integer or `TRUE`/`FALSE` right operands only; no expression-level operator support |
| `<` | 🟡 | Descriptor-driven filtered table `SELECT`, `DELETE`, and `UPDATE` predicates with decimal integer or `TRUE`/`FALSE` right operands only; no expression-level operator support |
| `<>, !=` | 🟡 | Descriptor-driven filtered table `SELECT`, `DELETE`, and `UPDATE` predicates with decimal integer or `TRUE`/`FALSE` right operands only; no expression-level operator support |
| `<<` | ❌ | Left shift |
| `<=` | 🟡 | Descriptor-driven filtered table `SELECT`, `DELETE`, and `UPDATE` predicates with decimal integer or `TRUE`/`FALSE` right operands only; no expression-level operator support |
| `<=>` | 🟡 | Descriptor-driven filtered table `SELECT`, `DELETE`, and `UPDATE` predicates with non-`NULL` decimal integer or `TRUE`/`FALSE` right operands only |
| `%, MOD` | 🟡 | Limited no-source/`DUAL` signed-64 scalar modulo projection with `%`, infix `MOD`, and `MOD(left, right)`; no table-backed expression support |
| `*` | 🟡 | Limited no-source/`DUAL` signed-64 scalar arithmetic projection only; no table-backed expression support |
| `+` | 🟡 | Limited no-source/`DUAL` signed-64 scalar arithmetic projection only; no table-backed expression support |
| `-` (binary) | 🟡 | Limited no-source/`DUAL` signed-64 scalar arithmetic projection only; no table-backed expression support |
| `-` (unary) | 🟡 | Limited no-source/`DUAL` signed-64 scalar unary arithmetic projection with unary `+` and unary `-`; no table-backed expression support or unsigned-width expression results |
| `/` | ❌ | Division operator |
| `:=` | ❌ | Assign a value |
| `=` (assignment) | 🟡 | One unqualified descriptor-column single-table `UPDATE` assignment to a supported decimal integer literal, `TRUE`, `FALSE`, `NULL`, or descriptor-resolved `DEFAULT`; `SET` statements and expression assignments remain unsupported |
| `=` (comparison) | 🟡 | Descriptor-driven filtered table `SELECT`, `DELETE`, and `UPDATE` predicates with non-`NULL` decimal integer or `TRUE`/`FALSE` right operands only |
| `^` | ❌ | Bitwise XOR |
| `AND, &&` | 🟡 | Limited descriptor-backed `WHERE` boolean expressions over existing `SELECT`/aggregate-source/`DELETE`/`UPDATE` predicate atoms; `AND` binds tighter than `XOR` and `OR`, `&&` is accepted with deprecation warning 1287, and no general expression-level boolean semantics are supported |
| `BETWEEN ... AND ...` | 🟡 | Limited descriptor-backed `WHERE` range predicates for existing `SELECT`/aggregate-source/`DELETE`/`UPDATE` predicate contexts with one descriptor column and two decimal integer or `TRUE`/`FALSE` bounds; no expression operands, string/temporal ranges, row ranges, or full expression-level operator support |
| `BINARY` | ❌ | Cast a string to a binary string |
| `CASE` | ❌ | Case operator |
| `DIV` | 🟡 | Specified for upcoming limited no-source/`DUAL` signed-64 scalar integer division projection; not implemented yet, and no table-backed expression support |
| `EXISTS()` | ❌ | Whether the result of a query contains any rows |
| `IN()` | 🟡 | Limited descriptor-backed `WHERE` membership predicates for existing `SELECT`/aggregate-source/`DELETE`/`UPDATE` predicate contexts with one descriptor column and a nonempty list of decimal integer, `TRUE`/`FALSE`, or `NULL` values; no expression operands, subqueries, row constructors, strings, temporals, or full expression-level operator support |
| `IS` | 🟡 | Descriptor-driven filtered table `SELECT`, `DELETE`, and `UPDATE` predicates for `IS NULL`, `IS TRUE`, `IS FALSE`, and `IS UNKNOWN` only; no literal-left or general expression truth tests |
| `IS NOT` | 🟡 | Descriptor-driven filtered table `SELECT`, `DELETE`, and `UPDATE` predicates for `IS NOT NULL`, `IS NOT TRUE`, `IS NOT FALSE`, and `IS NOT UNKNOWN` only; no literal-left or general expression truth tests |
| `IS NOT NULL` | 🟡 | Descriptor-driven filtered table `SELECT`, `DELETE`, and `UPDATE` predicates only |
| `IS NULL` | 🟡 | Descriptor-driven filtered table `SELECT`, `DELETE`, and `UPDATE` predicates only |
| `LIKE` | ❌ | Simple pattern matching |
| `NOT, !` | 🟡 | Limited keyword `NOT` over descriptor-backed `WHERE` boolean expressions for existing `SELECT`/aggregate-source/`DELETE`/`UPDATE` predicate atoms; symbolic `!` remains unsupported until MyLite owns its higher-precedence expression semantics and deprecation warning behavior |
| `NOT BETWEEN ... AND ...` | 🟡 | Limited negation of the descriptor-backed `BETWEEN` predicate subset; no expression operands, string/temporal ranges, row ranges, or full expression-level operator support |
| `NOT EXISTS()` | ❌ | Whether the result of a query contains no rows |
| `NOT IN()` | 🟡 | Limited negation of the descriptor-backed `IN` predicate subset, including MySQL three-valued `NULL` list semantics; no expression operands, subqueries, row constructors, strings, temporals, or full expression-level operator support |
| `NOT LIKE` | ❌ | Negation of simple pattern matching |
| `NOT REGEXP` | ❌ | Negation of REGEXP |
| `OR, \|\|` | 🟡 | Limited descriptor-backed `WHERE` boolean expressions over existing `SELECT`/aggregate-source/`DELETE`/`UPDATE` predicate atoms; `OR` binds looser than `XOR` and `AND`, `\|\|` is accepted as logical OR with deprecation warning 1287, and no `PIPES_AS_CONCAT` or general expression-level boolean semantics are supported |
| `REGEXP` | ❌ | Whether string matches regular expression |
| `RLIKE` | ❌ | Whether string matches regular expression |
| `SOUNDS LIKE` | ❌ | Compare sounds |
| `XOR` | 🟡 | Limited descriptor-backed `WHERE` boolean expressions over existing `SELECT`/aggregate-source/`DELETE`/`UPDATE` predicate atoms; `XOR` binds looser than `AND` and tighter than `OR`, propagates `NULL` for the admitted predicate subset, and has no scalar expression-level support |
| `\|` | ❌ | Bitwise OR |
| `~` | ❌ | Bitwise inversion |

[Back to compatibility overview](../../COMPATIBILITY.md)
