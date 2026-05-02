# MySQL Compatibility Feature Roadmap

This task list orders the first 50 MySQL compatibility features by dependency
and practical application coverage. It starts with schema/session foundations,
then table definition, writes, reads, mutation, metadata inspection, and higher
level application surfaces.

## First 50 Tasks

1. [x] Schema selection and database lifecycle: `CREATE DATABASE`, `USE`,
       `ALTER DATABASE`, `DROP DATABASE`, and `SHOW DATABASES`.
2. [x] Core metadata catalog: internal schema/table/column/index storage plus
       `INFORMATION_SCHEMA.SCHEMATA`, `TABLES`, `COLUMNS`, and `STATISTICS`.
3. [x] Character set and collation foundation: `utf8mb4`, `utf8mb3`, `latin1`,
       `binary`, default charset/collation tracking, `SET NAMES`, and
       `SET CHARACTER SET`.
4. [x] Integer and boolean column types: `TINYINT`, `SMALLINT`, `MEDIUMINT`,
       `INT`, `BIGINT`, unsigned ranges, display-width compatibility,
       `BOOL`/`BOOLEAN`, and integer aliases.
5. [x] String and binary column types: `CHAR`, `VARCHAR`, `TEXT` family,
       `BINARY`, `VARBINARY`, `BLOB` family, length limits, and metadata.
6. [x] Exact and approximate numeric column types: `DECIMAL`, `NUMERIC`,
       `FLOAT`, `DOUBLE`, aliases, precision/scale, and metadata.
7. [x] Temporal column types: `DATE`, `TIME`, `DATETIME`, `TIMESTAMP`, `YEAR`,
       fractional seconds, zero values, and metadata.
8. [x] Column attributes: `NULL`, `NOT NULL`, `DEFAULT`, expression defaults,
       `ON UPDATE`, comments, visibility, and storage/format options.
9. [x] Primary keys and `AUTO_INCREMENT`: inline and table constraints,
       allocation, explicit values, metadata, and error cases.
10. [x] Unique and secondary indexes in `CREATE TABLE`: parse-only key parts,
       prefix lengths, index types, visibility, and comments.
11. [x] `CREATE TABLE` base execution: column definitions, constraints, table
       options, schema qualification, `IF NOT EXISTS`, warnings, and atomicity.
12. [x] `DROP TABLE`: multi-table drops, `IF EXISTS`, temporary table handling,
       warning behavior, metadata cleanup, and implicit commit behavior.
13. [x] `INSERT ... VALUES`: column lists, multi-row values, defaults,
       generated/default columns, affected rows, warnings, and insert ids.
14. [x] `INSERT ... SET`: assignment-form insert semantics, defaults, duplicate
       column diagnostics, affected rows, and insert ids.
15. [x] Table-backed `SELECT` core: projection aliases, qualified wildcards,
       base table references, schema qualification, and result metadata.
16. [x] Expression operator foundation: comparison, logical, bitwise, `IS NULL`,
       `BETWEEN`, `LIKE`, `IN`, arithmetic, precedence, and type conversion.
17. [x] `WHERE`: predicate evaluation, three-valued logic, conversion warnings,
       and name resolution. Spec: [WHERE clause](../where-clause/specs.md).
18. [x] `ORDER BY`, `LIMIT`, and `OFFSET`: aliases, ordinals, collations,
       integer conversion, prepared markers, and error cases. Spec:
       [ORDER BY, LIMIT, and OFFSET](../order-limit-offset/specs.md).
19. [x] Single-table `UPDATE`: assignment order, expressions, `WHERE`,
       `ORDER BY`, `LIMIT`, generated/default columns, affected rows, and
       warnings. Spec: [single-table UPDATE](../update-single-table/specs.md).
20. [x] Single-table `DELETE`: aliases, `WHERE`, `ORDER BY`, `LIMIT`,
       affected rows, and warnings. Spec:
       [single-table DELETE](../delete-single-table/specs.md).
21. [x] Transaction statements: `START TRANSACTION`, `BEGIN`, `COMMIT`,
       `ROLLBACK`, access modes, chaining, release options, and diagnostics.
       Spec: [transaction statements](../transaction-statements/specs.md).
22. [x] Savepoints: `SAVEPOINT`, `ROLLBACK TO SAVEPOINT`,
       `RELEASE SAVEPOINT`, replacement, nesting, and error behavior.
       Spec: [savepoints](../savepoints/specs.md).
23. [x] Result metadata and expression labels: type, length, flags, origin
       metadata, alias labels, duplicate labels, and nullability. Spec:
       [result metadata and expression labels](../result-metadata-expression-labels/specs.md).
24. [ ] Scalar built-in functions used by common applications: string, numeric,
       temporal, conditional, comparison, information, and compatibility
       functions with MySQL conversion behavior. Started; spec:
       [scalar built-in functions](../scalar-built-in-functions/specs.md).
25. [x] Aggregate functions and grouping: `COUNT`, `SUM`, `AVG`, `MIN`, `MAX`,
       `GROUP BY`, `HAVING`, aliases, ordinals, and `ONLY_FULL_GROUP_BY`.
       Spec: [aggregate functions and grouping](../aggregate-grouping/specs.md).
26. [x] Inner joins and comma joins: join precedence, aliases, `ON`, `USING`,
       name resolution, and result metadata. Spec:
       [inner joins](../inner-joins/specs.md).
27. [x] Outer joins: `LEFT`/`RIGHT` joins, null extension, predicate placement,
       and metadata. Spec: [outer joins](../outer-joins/specs.md).
28. [x] `DISTINCT` and `DISTINCTROW`: duplicate elimination, collation,
       metadata, and interaction with ordering/limits. Spec:
       [SELECT DISTINCT](../select-distinct/specs.md).
29. [ ] Subqueries: scalar, row, `EXISTS`, `IN`, quantified comparisons,
       correlation, cardinality errors, and metadata. Started; spec:
       [subqueries](../subqueries/specs.md).
30. [x] `UNION`: `ALL`/`DISTINCT`, column names/types, ordering, limits,
       parentheses, and metadata.
31. [ ] `INSERT IGNORE`: duplicate, conversion, and constraint warning-demotion
       rules. Partially implemented for duplicate-key and required-column
       demotion on supported `INSERT ... VALUES`/`INSERT ... SET` forms; full
       conversion/range/truncation/temporal demotion remains deferred. Spec:
       [INSERT IGNORE](../insert-ignore/specs.md).
32. [ ] `INSERT ... ON DUPLICATE KEY UPDATE`: conflict selection, assignment
       evaluation, affected rows, insert ids, aliases, and warnings. Partially
       implemented for supported `VALUES`/`SET` forms and scoped `IGNORE`;
       insert-select, partitions, generated columns, triggers, foreign keys,
       full conversion demotion, and broad expressions remain deferred. Spec:
       [INSERT ... ON DUPLICATE KEY UPDATE](../insert-on-duplicate-key-update/specs.md).
33. [ ] `REPLACE`: values, set, and select forms; delete-then-insert semantics;
       cascades; triggers; affected rows; and auto-increment behavior.
       Partially implemented for supported `VALUES`/`VALUE`/`VALUES ROW(...)`
       and `SET` forms, including optional `INTO`, column lists, explicit
       conflict delete loop, source-order multi-row semantics, affected rows,
       `LOW_PRIORITY`, `DELAYED` warning 3005, rollback, and auto-increment/
       last-insert-id behavior. Query-source replace, partitions, cascades,
       triggers, generated columns, and broad conversion fidelity remain
       deferred. Spec: [REPLACE](../replace/specs.md).
34. [ ] `CREATE INDEX` and `DROP INDEX`: standalone index creation/removal,
       options, metadata, warnings, and implicit commit behavior.
       Started/speced only; no parser or runtime support yet. First slice
       targets metadata-backed ordinary and unique standalone indexes plus
       `DROP INDEX name ON table`, with full-text, spatial, functional, and
       optimizer surfaces deferred. Spec:
       [standalone CREATE INDEX and DROP INDEX](../create-drop-index/specs.md).
35. [ ] `ALTER TABLE` column operations: `ADD`, `DROP`, `RENAME`, `CHANGE`,
       `MODIFY`, defaults, positioning, validation, and metadata rewrite.
36. [ ] `ALTER TABLE` key and constraint operations: primary, unique, index,
       check, and foreign-key add/drop/rename/toggle behavior.
37. [ ] `RENAME TABLE` and `ALTER TABLE ... RENAME TO`: atomic multi-table
       rename, metadata rewrite, temporary table handling, and errors.
38. [ ] `TRUNCATE TABLE`: DDL-like delete, auto-increment reset, foreign-key
       restrictions, implicit commits, and affected-row behavior.
39. [ ] `SHOW TABLES`, `SHOW COLUMNS`, `SHOW INDEX`, `DESCRIBE`, and
       `SHOW CREATE TABLE`: result-set shape, filtering, metadata formatting,
       and warnings.
40. [ ] `SHOW VARIABLES`, `SHOW STATUS`, `SHOW WARNINGS`, `SHOW ERRORS`, and
       count variants: session state, diagnostics, filtering, and metadata.
41. [ ] User and system variables: `SET`, user-variable storage, system-variable
       validation, scope, charset/collation metadata, and expression use.
42. [ ] Prepared statements: `PREPARE`, `EXECUTE`, `DEALLOCATE PREPARE`,
       parameter markers, metadata, and diagnostics.
43. [ ] `CREATE TABLE ... LIKE`: metadata cloning, indexes, defaults, generated
       columns, temporary tables, and atomicity.
44. [ ] `CREATE TABLE ... SELECT` and `INSERT ... SELECT`: type inference,
       metadata, defaults, locking, warnings, and atomicity.
45. [ ] `CREATE TEMPORARY TABLE`: session lifecycle, name shadowing, metadata,
       and cleanup.
46. [ ] Views: `CREATE VIEW`, `ALTER VIEW`, `DROP VIEW`, `SHOW CREATE VIEW`,
       algorithms, definers, security, column names, and check options.
47. [ ] Triggers: `CREATE TRIGGER`, `DROP TRIGGER`, `SHOW TRIGGERS`,
       `SHOW CREATE TRIGGER`, timing, ordering, body execution, and metadata.
48. [ ] Foreign keys: definition, validation, cascades, `RESTRICT`/`NO ACTION`,
       `SET NULL`, metadata, and enforcement.
49. [ ] JSON type and functions: validation, extraction, paths, comparison,
       mutation, generated-column interactions, and metadata.
50. [ ] Administrative compatibility placeholders: `LOCK TABLES`,
       `UNLOCK TABLES`, maintenance statements, account/privilege statements,
       replication-only statements, and embedded-compatible diagnostics.

## Execution Rule

Each task is executed as an end-to-end feature: independently authored spec,
MySQL 8.4.9 runtime verification, grammar support, AST/analyzer/runtime work,
tests, compatibility-matrix updates, review, and a focused commit.
