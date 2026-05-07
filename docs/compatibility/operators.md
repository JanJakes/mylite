# Operators

Operators, predicates, assignment forms, and SQL expression syntax that MySQL lists alongside built-ins.

| Function or operator | Status | Notes |
| --- | --- | --- |
| `&` | ❌ | Bitwise AND |
| `>` | 🟡 | Descriptor-driven filtered table `SELECT`, `DELETE`, and `UPDATE` predicates only; no expression-level operator support |
| `>>` | ❌ | Right shift |
| `>=` | 🟡 | Descriptor-driven filtered table `SELECT`, `DELETE`, and `UPDATE` predicates only; no expression-level operator support |
| `<` | 🟡 | Descriptor-driven filtered table `SELECT`, `DELETE`, and `UPDATE` predicates only; no expression-level operator support |
| `<>, !=` | 🟡 | Descriptor-driven filtered table `SELECT`, `DELETE`, and `UPDATE` predicates only; no expression-level operator support |
| `<<` | ❌ | Left shift |
| `<=` | 🟡 | Descriptor-driven filtered table `SELECT`, `DELETE`, and `UPDATE` predicates only; no expression-level operator support |
| `<=>` | 🟡 | Descriptor-driven filtered table `SELECT`, `DELETE`, and `UPDATE` predicates with non-`NULL` integer right operands only |
| `%, MOD` | ❌ | Modulo operator |
| `*` | ❌ | Multiplication operator |
| `+` | ❌ | Addition operator |
| `-` (binary) | ❌ | Minus operator |
| `-` (unary) | ❌ | Change the sign of the argument |
| `/` | ❌ | Division operator |
| `:=` | ❌ | Assign a value |
| `=` (assignment) | 🟡 | One unqualified descriptor-column single-table `UPDATE` assignment to a supported decimal integer literal or `NULL`; `SET` statements and expression assignments remain unsupported |
| `=` (comparison) | 🟡 | Descriptor-driven filtered table `SELECT`, `DELETE`, and `UPDATE` predicates with non-`NULL` integer right operands only |
| `^` | ❌ | Bitwise XOR |
| `AND, &&` | ❌ | Logical AND |
| `BETWEEN ... AND ...` | ❌ | Whether a value is within a range of values |
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
| `NOT, !` | ❌ | Negates value |
| `NOT BETWEEN ... AND ...` | ❌ | Whether a value is not within a range of values |
| `NOT EXISTS()` | ❌ | Whether the result of a query contains no rows |
| `NOT IN()` | ❌ | Whether a value is not within a set of values |
| `NOT LIKE` | ❌ | Negation of simple pattern matching |
| `NOT REGEXP` | ❌ | Negation of REGEXP |
| `OR, \|\|` | ❌ | Logical OR |
| `REGEXP` | ❌ | Whether string matches regular expression |
| `RLIKE` | ❌ | Whether string matches regular expression |
| `SOUNDS LIKE` | ❌ | Compare sounds |
| `XOR` | ❌ | Logical XOR |
| `\|` | ❌ | Bitwise OR |
| `~` | ❌ | Bitwise inversion |

[Back to compatibility overview](../../COMPATIBILITY.md)
