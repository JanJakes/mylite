# JSON functions and operators

JSON construction, extraction, mutation, aggregation, validation, and storage-observable functions.

| Function or operator | Status | Notes |
| --- | --- | --- |
| `->` | 🟡 | Limited single-table row-scalar projection shorthand for `JSON_EXTRACT(column, path)` over unqualified or qualified descriptor columns and simple path string literals; no literal/function left operands, wildcards/ranges, predicates, expression ordering, DML assignment expressions, or full expression support |
| `->>` | 🟡 | Limited single-table row-scalar projection shorthand for `JSON_UNQUOTE(JSON_EXTRACT(column, path))` over unqualified or qualified descriptor columns and simple path string literals; no literal/function left operands, wildcards/ranges, predicates, expression ordering, DML assignment expressions, or full expression support |
| `JSON_ARRAY()` | 🟡 | Limited no-source/`DUAL`/`DO` and single-table row-scalar projection over admitted SQL string, signed-64 integer, boolean, `NULL`, JSON descriptor, integer descriptor, boolean-like integer descriptor, and nonbinary string descriptor values; JSON columns construct as JSON values while string columns construct as JSON strings; no nested construction, arbitrary expression operands, predicates, ordering expressions, DML assignment expressions, aggregation, mutation, or protocol metadata beyond limited scalar/row-scalar result-column metadata |
| `JSON_ARRAY_APPEND()` | ❌ | Append data to JSON document |
| `JSON_ARRAY_INSERT()` | ❌ | Insert into JSON array |
| `JSON_ARRAYAGG()` | ❌ | Return result set as a single JSON array |
| `JSON_CONTAINS()` | 🟡 | Limited no-source/`DUAL`/`DO` and single-table row-scalar projection/predicate containment over SQL string/`NULL`, JSON descriptor, and nonbinary string descriptor document/candidate values, with optional simple path string/`NULL` literal and current signed-integer JSON number subset; no wildcard/range paths, arbitrary expression arguments, binary strings, decimals/exponents, ordering/grouping expressions, DML assignment expressions, mutation, or protocol metadata beyond limited scalar/row-scalar result-column metadata |
| `JSON_CONTAINS_PATH()` | 🟡 | Limited no-source/`DUAL`/`DO` and single-table row-scalar projection/predicate path existence over SQL string/`NULL`, JSON descriptor, and nonbinary string descriptor document values with case-insensitive literal `one`/`all` and simple path string/`NULL` literals; no wildcard/range/multi-match paths, arbitrary expression arguments, path columns, binary strings, ordering/grouping expressions, DML assignment expressions, or protocol metadata beyond limited scalar/row-scalar result-column metadata |
| `JSON_DEPTH()` | ❌ | Maximum depth of JSON document |
| `JSON_EXTRACT()` | 🟡 | Limited no-source/`DUAL`/`DO` and single-table row-scalar projection with one JSON document operand and one simple path string literal; supports SQL string literals, `NULL`, JSON columns, and nonbinary string columns, returning JSON text or SQL `NULL`; no multiple paths, wildcards/ranges, predicates, expression ordering, DML assignment expressions, arbitrary expressions, or protocol metadata beyond limited scalar/row-scalar result-column metadata |
| `JSON_INSERT()` | ❌ | Insert data into JSON document |
| `JSON_KEYS()` | ❌ | Array of keys from JSON document |
| `JSON_LENGTH()` | 🟡 | Limited no-source/`DUAL`/`DO` and single-table row-scalar shallow length over SQL string literals, `NULL`, JSON columns, nonbinary string columns, and supported `JSON_EXTRACT()` results, with optional simple path string literal; no wildcards, multi-match paths, arbitrary expressions, predicates, expression ordering, DML assignment expressions, full JSON number grammar, or protocol metadata beyond limited scalar/row-scalar result-column metadata |
| `JSON_MERGE()` | ❌ | Deprecated merge synonym |
| `JSON_MERGE_PATCH()` | ❌ | Merge JSON documents, replacing values of duplicate keys |
| `JSON_MERGE_PRESERVE()` | ❌ | Merge JSON documents, preserving duplicate keys |
| `JSON_OBJECT()` | 🟡 | Limited no-source/`DUAL`/`DO` and single-table row-scalar projection with zero or even key/value arguments; keys may be admitted string, signed-64 integer, boolean, or non-`NULL` descriptor integer/nonbinary string values, values follow `JSON_ARRAY()` limits including JSON descriptor values, duplicate keys keep the last value, and `NULL` keys report MySQL-compatible `3158`; no nested construction, arbitrary expression operands, JSON-column keys, predicates, ordering expressions, DML assignment expressions, aggregation, mutation, or protocol metadata beyond limited scalar/row-scalar result-column metadata |
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
| `JSON_TYPE()` | 🟡 | Limited no-source/`DUAL`/`DO` and single-table row-scalar value type labels over SQL string literals, `NULL`, JSON columns, nonbinary string columns, and supported `JSON_EXTRACT()` results; currently reports `OBJECT`, `ARRAY`, `BOOLEAN`, `NULL`, `STRING`, and signed-integer `INTEGER`; no decimal/exponent JSON numbers, arbitrary expressions, predicates, expression ordering, DML assignment expressions, or protocol metadata beyond limited scalar/row-scalar result-column metadata |
| `JSON_UNQUOTE()` | 🟡 | Limited no-source/`DUAL`/`DO` and single-table row-scalar projection over SQL string literals, `NULL`, JSON/string columns, and supported `JSON_EXTRACT()` results; unquotes JSON string text and preserves non-string JSON text; no binary/numeric/boolean scalar inputs, predicates, expression ordering, DML assignment expressions, or arbitrary expressions |
| `JSON_VALID()` | 🟡 | Limited no-source/`DUAL`/`DO` and single-table row-scalar validity check over admitted string, JSON, integer, binary string, `BIT`, boolean, and `NULL` values, including supported row predicates; no arbitrary expression, path, mutation semantics, or protocol metadata beyond limited scalar/row-scalar result-column metadata |
| `JSON_VALUE()` | ❌ | JSON path value extraction |
| `MEMBER OF()` | ❌ | JSON array membership |

[Back to compatibility overview](../../COMPATIBILITY.md)
