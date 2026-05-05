# Joined `GROUP BY`

## Scope

MyLite supports grouped aggregate queries over joined table sources, including
outer joins. The immediate compatibility target is WordPress-style queries such
as `LEFT JOIN ... GROUP BY wp_posts.ID ... ORDER BY ...`.

## Behavior

- `GROUP BY` and `HAVING` are accepted for inner, cross, comma, left, and right
  joined table sources when the involved expressions are otherwise supported.
- Aggregate evaluation happens after join row materialization, including null
  extension for outer joins.
- Group keys use the same expression and column-reference resolution rules as
  single-table grouping.
- `ONLY_FULL_GROUP_BY` validation applies to joined queries. Columns from a
  table are group-invariant when that table is functionally determined by a
  grouped non-null unique key.
- Functional-dependence checks are plan-wide, not limited to the legacy
  single-table storage slot.
- `ORDER BY` after grouping can reference output aliases, aggregate outputs,
  group keys, and group-invariant expressions using existing order binding
  rules.

## Verified Expectations

Verified against MySQL 8.4.9:

```sql
SELECT wp_posts.ID, wp_posts.post_title, COUNT(wp_postmeta.meta_id) AS meta_count
FROM wp_posts
LEFT JOIN wp_postmeta ON wp_posts.ID = wp_postmeta.post_id
GROUP BY wp_posts.ID
ORDER BY wp_posts.post_date DESC;
```

With posts `1..3` and two matching metadata rows for post `1`, one for post `2`,
and none for post `3`, MySQL returns:

| ID | post_title | meta_count |
| --- | --- | --- |
| 3 | c | 0 |
| 2 | b | 1 |
| 1 | a | 2 |

## Compatibility Decisions

MyLite should not introduce a separate joined-grouping executor. The existing
joined row materializers already feed grouped aggregate execution; this feature
removes the historical prepare-time restriction and fixes validation paths that
still assumed a single table.
