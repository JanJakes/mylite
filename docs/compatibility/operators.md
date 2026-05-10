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
| `%, MOD` | ❌ | Modulo operator |
| `*` | ❌ | Multiplication operator |
| `+` | ❌ | Addition operator |
| `-` (binary) | ❌ | Minus operator |
| `-` (unary) | ❌ | Change the sign of the argument |
| `/` | ❌ | Division operator |
| `:=` | ❌ | Assign a value |
| `=` (assignment) | 🟡 | One unqualified descriptor-column single-table `UPDATE` assignment to a supported decimal integer literal, `TRUE`, `FALSE`, or `NULL`; `SET` statements and expression assignments remain unsupported |
| `=` (comparison) | 🟡 | Descriptor-driven filtered table `SELECT`, `DELETE`, and `UPDATE` predicates with non-`NULL` decimal integer or `TRUE`/`FALSE` right operands only |
| `^` | ❌ | Bitwise XOR |
| `AND, &&` | 🟡 | Limited descriptor-backed `WHERE` boolean expressions over existing `SELECT`/aggregate-source/`DELETE`/`UPDATE` predicate atoms; `AND` binds tighter than `OR`, `&&` is accepted with deprecation warning 1287, and no general expression-level boolean semantics are supported |
| `BETWEEN ... AND ...` | 🟡 | Limited descriptor-backed `WHERE` range predicates for existing `SELECT`/aggregate-source/`DELETE`/`UPDATE` predicate contexts with one descriptor column and two decimal integer or `TRUE`/`FALSE` bounds; no expression operands, string/temporal ranges, row ranges, or full expression-level operator support |
| `BINARY` | ❌ | Cast a string to a binary string |
| `CASE` | ❌ | Case operator |
| `DIV` | ❌ | Integer division |
| `EXISTS()` | ❌ | Whether the result of a query contains any rows |
| `IN()` | ❌ | Whether a value is within a set of values |
| `IS` | ❌ | Test a value against a boolean |
| `IS NOT` | ❌ | Test a value against a boolean |
| `IS NOT NULL` | 🟡 | Descriptor-driven filtered table `SELECT`, `DELETE`, and `UPDATE` predicates only |
| `IS NULL` | 🟡 | Descriptor-driven filtered table `SELECT`, `DELETE`, and `UPDATE` predicates only |
| `LIKE` | ❌ | Simple pattern matching |
| `NOT, !` | 🟡 | Limited keyword `NOT` over descriptor-backed `WHERE` boolean expressions for existing `SELECT`/aggregate-source/`DELETE`/`UPDATE` predicate atoms; symbolic `!` remains unsupported until MyLite owns its higher-precedence expression semantics and deprecation warning behavior |
| `NOT BETWEEN ... AND ...` | 🟡 | Limited negation of the descriptor-backed `BETWEEN` predicate subset; no expression operands, string/temporal ranges, row ranges, or full expression-level operator support |
| `NOT EXISTS()` | ❌ | Whether the result of a query contains no rows |
| `NOT IN()` | ❌ | Whether a value is not within a set of values |
| `NOT LIKE` | ❌ | Negation of simple pattern matching |
| `NOT REGEXP` | ❌ | Negation of REGEXP |
| `OR, \|\|` | 🟡 | Limited descriptor-backed `WHERE` boolean expressions over existing `SELECT`/aggregate-source/`DELETE`/`UPDATE` predicate atoms; `OR` binds looser than `AND`, `\|\|` is accepted as logical OR with deprecation warning 1287, and no `PIPES_AS_CONCAT` or general expression-level boolean semantics are supported |
| `REGEXP` | ❌ | Whether string matches regular expression |
| `RLIKE` | ❌ | Whether string matches regular expression |
| `SOUNDS LIKE` | ❌ | Compare sounds |
| `XOR` | ❌ | Logical XOR |
| `\|` | ❌ | Bitwise OR |
| `~` | ❌ | Bitwise inversion |

[Back to compatibility overview](../../COMPATIBILITY.md)
