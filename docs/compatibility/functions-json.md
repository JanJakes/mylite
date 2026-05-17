# JSON functions and operators

JSON construction, extraction, mutation, aggregation, validation, and storage-observable functions.

| Function or operator | Status | Notes |
| --- | --- | --- |
| `->` | 🟡 | Limited single-table row-scalar projection shorthand for `JSON_EXTRACT(column, path)` over unqualified or qualified descriptor columns and simple path string literals; no literal/function left operands, wildcards/ranges, predicates, expression ordering, DML assignment expressions, or full expression support |
| `->>` | 🟡 | Limited single-table row-scalar projection shorthand for `JSON_UNQUOTE(JSON_EXTRACT(column, path))` over unqualified or qualified descriptor columns and simple path string literals; no literal/function left operands, wildcards/ranges, predicates, expression ordering, DML assignment expressions, or full expression support |
| `JSON_ARRAY()` | ❌ | Create JSON array |
| `JSON_ARRAY_APPEND()` | ❌ | Append data to JSON document |
| `JSON_ARRAY_INSERT()` | ❌ | Insert into JSON array |
| `JSON_ARRAYAGG()` | ❌ | Return result set as a single JSON array |
| `JSON_CONTAINS()` | ❌ | Whether JSON document contains specific object at path |
| `JSON_CONTAINS_PATH()` | ❌ | Whether JSON document contains any data at path |
| `JSON_DEPTH()` | ❌ | Maximum depth of JSON document |
| `JSON_EXTRACT()` | 🟡 | Limited no-source/`DUAL`/`DO` and single-table row-scalar projection with one JSON document operand and one simple path string literal; supports SQL string literals, `NULL`, JSON columns, and nonbinary string columns, returning JSON text or SQL `NULL`; no multiple paths, wildcards/ranges, predicates, expression ordering, DML assignment expressions, or arbitrary expressions |
| `JSON_INSERT()` | ❌ | Insert data into JSON document |
| `JSON_KEYS()` | ❌ | Array of keys from JSON document |
| `JSON_LENGTH()` | ❌ | Number of elements in JSON document |
| `JSON_MERGE()` | ❌ | Deprecated merge synonym |
| `JSON_MERGE_PATCH()` | ❌ | Merge JSON documents, replacing values of duplicate keys |
| `JSON_MERGE_PRESERVE()` | ❌ | Merge JSON documents, preserving duplicate keys |
| `JSON_OBJECT()` | ❌ | Create JSON object |
| `JSON_OBJECTAGG()` | ❌ | Return result set as a single JSON object |
| `JSON_OVERLAPS()` | ❌ | Shared JSON keys/elements |
| `JSON_PRETTY()` | ❌ | Print a JSON document in human-readable format |
| `JSON_QUOTE()` | ❌ | Quote JSON document |
| `JSON_REMOVE()` | ❌ | Remove data from JSON document |
| `JSON_REPLACE()` | ❌ | Replace values in JSON document |
| `JSON_SCHEMA_VALID()` | ❌ | JSON Schema validation boolean |
| `JSON_SCHEMA_VALIDATION_REPORT()` | ❌ | JSON Schema validation report |
| `JSON_SEARCH()` | ❌ | Path to value within JSON document |
| `JSON_SET()` | ❌ | Insert data into JSON document |
| `JSON_STORAGE_FREE()` | ❌ | Partial-update free space |
| `JSON_STORAGE_SIZE()` | ❌ | Binary JSON storage size |
| `JSON_TABLE()` | ❌ | Return data from a JSON expression as a relational table |
| `JSON_TYPE()` | ❌ | Type of JSON value |
| `JSON_UNQUOTE()` | 🟡 | Limited no-source/`DUAL`/`DO` and single-table row-scalar projection over SQL string literals, `NULL`, JSON/string columns, and supported `JSON_EXTRACT()` results; unquotes JSON string text and preserves non-string JSON text; no binary/numeric/boolean scalar inputs, predicates, expression ordering, DML assignment expressions, or arbitrary expressions |
| `JSON_VALID()` | 🟡 | Limited no-source/`DUAL`/`DO` and single-table row-scalar validity check over admitted string, JSON, integer, binary string, `BIT`, boolean, and `NULL` values, including supported row predicates; no arbitrary expression, path, construction, or mutation semantics |
| `JSON_VALUE()` | ❌ | JSON path value extraction |
| `MEMBER OF()` | ❌ | JSON array membership |

[Back to compatibility overview](../../COMPATIBILITY.md)
