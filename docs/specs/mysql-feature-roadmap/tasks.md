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
6. [ ] Exact and approximate numeric column types: `DECIMAL`, `NUMERIC`,
       `FLOAT`, `DOUBLE`, aliases, precision/scale, and metadata.
7. [ ] Temporal column types: `DATE`, `TIME`, `DATETIME`, `TIMESTAMP`, `YEAR`,
       fractional seconds, zero values, and metadata.
8. [ ] Column attributes: `NULL`, `NOT NULL`, `DEFAULT`, expression defaults,
       `ON UPDATE`, comments, visibility, and storage/format options.
9. [ ] Primary keys and `AUTO_INCREMENT`: inline and table constraints,
       allocation, explicit values, metadata, and error cases.
10. [ ] Unique and secondary indexes in `CREATE TABLE`: key parts, prefix
       lengths, index types, visibility, comments, and metadata.
11. [ ] `CREATE TABLE` base execution: column definitions, constraints, table
       options, schema qualification, `IF NOT EXISTS`, warnings, and atomicity.
12. [ ] `DROP TABLE`: multi-table drops, `IF EXISTS`, temporary table handling,
       warning behavior, metadata cleanup, and implicit commit behavior.
13. [ ] `INSERT ... VALUES`: column lists, multi-row values, defaults,
       generated/default columns, affected rows, warnings, and insert ids.
14. [ ] `INSERT ... SET`: assignment-form insert semantics, defaults, duplicate
       column diagnostics, affected rows, and insert ids.
15. [ ] Table-backed `SELECT` core: projection aliases, qualified wildcards,
       base table references, schema qualification, and result metadata.
16. [ ] Expression operator foundation: comparison, logical, bitwise, `IS NULL`,
       `BETWEEN`, `LIKE`, `IN`, arithmetic, precedence, and type conversion.
17. [ ] `WHERE`: predicate evaluation, three-valued logic, conversion warnings,
       and name resolution.
18. [ ] `ORDER BY`, `LIMIT`, and `OFFSET`: aliases, ordinals, collations,
       integer conversion, prepared markers, and error cases.
19. [ ] Single-table `UPDATE`: assignment order, expressions, `WHERE`,
       `ORDER BY`, `LIMIT`, generated/default columns, affected rows, and
       warnings.
20. [ ] Single-table `DELETE`: aliases, `WHERE`, `ORDER BY`, `LIMIT`,
       affected rows, and warnings.
21. [ ] Transaction statements: `START TRANSACTION`, `BEGIN`, `COMMIT`,
       `ROLLBACK`, access modes, chaining, release options, and diagnostics.
22. [ ] Savepoints: `SAVEPOINT`, `ROLLBACK TO SAVEPOINT`,
       `RELEASE SAVEPOINT`, replacement, nesting, and error behavior.
23. [ ] Result metadata and expression labels: type, length, flags, origin
       metadata, alias labels, duplicate labels, and nullability.
24. [ ] Scalar built-in functions used by common applications: string, numeric,
       temporal, conditional, comparison, information, and compatibility
       functions with MySQL conversion behavior.
25. [ ] Aggregate functions and grouping: `COUNT`, `SUM`, `AVG`, `MIN`, `MAX`,
       `GROUP BY`, `HAVING`, aliases, ordinals, and `ONLY_FULL_GROUP_BY`.
26. [ ] Inner joins and comma joins: join precedence, aliases, `ON`, `USING`,
       name resolution, and result metadata.
27. [ ] Outer joins: `LEFT`/`RIGHT` joins, null extension, predicate placement,
       and metadata.
28. [ ] `DISTINCT` and `DISTINCTROW`: duplicate elimination, collation,
       metadata, and interaction with ordering/limits.
29. [ ] Subqueries: scalar, row, `EXISTS`, `IN`, quantified comparisons,
       correlation, cardinality errors, and metadata.
30. [ ] `UNION`: `ALL`/`DISTINCT`, column names/types, ordering, limits,
       parentheses, and metadata.
31. [ ] `INSERT IGNORE`: duplicate, conversion, and constraint warning-demotion
       rules.
32. [ ] `INSERT ... ON DUPLICATE KEY UPDATE`: conflict selection, assignment
       evaluation, affected rows, insert ids, aliases, and warnings.
33. [ ] `REPLACE`: values, set, and select forms; delete-then-insert semantics;
       cascades; triggers; affected rows; and auto-increment behavior.
34. [ ] `CREATE INDEX` and `DROP INDEX`: standalone index creation/removal,
       options, metadata, warnings, and implicit commit behavior.
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
