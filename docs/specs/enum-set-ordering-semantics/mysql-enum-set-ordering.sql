DROP DATABASE IF EXISTS mylite_enum_set_ordering;
CREATE DATABASE mylite_enum_set_ordering CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE mylite_enum_set_ordering;
SET NAMES utf8mb4;

CREATE TABLE value_lists (
  id INT PRIMARY KEY AUTO_INCREMENT,
  e ENUM('zeta','alpha','beta') NOT NULL,
  s SET('zeta','alpha','beta') DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT INTO value_lists (e,s) VALUES
  ('alpha','alpha'),
  ('zeta','zeta'),
  ('beta','beta'),
  ('alpha','zeta,beta'),
  ('zeta','alpha,beta'),
  ('beta','zeta,alpha');

SELECT 'order-enum', id, e, e + 0, s, s + 0 FROM value_lists ORDER BY e, id;
SELECT 'order-set', id, e, e + 0, s, s + 0 FROM value_lists ORDER BY s, id;
SELECT 'distinct-enum', e, e + 0 FROM value_lists GROUP BY e ORDER BY e;
SELECT 'distinct-set', s, s + 0 FROM value_lists GROUP BY s ORDER BY s;
SELECT 'group-enum', e, e + 0, COUNT(*) FROM value_lists GROUP BY e ORDER BY e;
SELECT 'group-set', s, s + 0, COUNT(*) FROM value_lists GROUP BY s ORDER BY s;
SELECT 'where-enum-string', id, e, e + 0 FROM value_lists WHERE e > 'alpha' ORDER BY id;
SELECT 'where-enum-numeric', id, e, e + 0 FROM value_lists WHERE e > 1 ORDER BY id;
SELECT 'where-set-string', id, s, s + 0 FROM value_lists WHERE s > 'alpha' ORDER BY id;
SELECT 'where-set-numeric', id, s, s + 0 FROM value_lists WHERE s > 2 ORDER BY id;
SELECT 'comparisons', id,
  e = 'zeta', e = 1, e > 'alpha', e > 1,
  s = 'zeta', s = 1, s > 'alpha', s > 1
FROM value_lists ORDER BY id;
SELECT 'extrema',
  MIN(e), MIN(e + 0), MAX(e), MAX(e + 0),
  MIN(s), MIN(s + 0), MAX(s), MAX(s + 0)
FROM value_lists;
SELECT 'group-concat',
  GROUP_CONCAT(e ORDER BY e SEPARATOR '|'),
  GROUP_CONCAT(s ORDER BY s SEPARATOR '|')
FROM value_lists;
