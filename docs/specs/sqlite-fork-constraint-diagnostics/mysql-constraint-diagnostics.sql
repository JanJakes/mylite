DROP DATABASE IF EXISTS mylite_constraint_diagnostics;
CREATE DATABASE mylite_constraint_diagnostics;
USE mylite_constraint_diagnostics;

CREATE TABLE wp_constraint_like (
  id INT NOT NULL,
  slug VARCHAR(20) NOT NULL,
  title VARCHAR(20) NOT NULL,
  PRIMARY KEY(id),
  UNIQUE KEY slug_key(slug)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT INTO wp_constraint_like VALUES (1, 'home', 'Home');
INSERT INTO wp_constraint_like VALUES (2, NULL, 'Missing slug');
INSERT INTO wp_constraint_like VALUES (1, 'about', 'Duplicate primary');
INSERT INTO wp_constraint_like VALUES (3, 'home', 'Duplicate slug');
SELECT COUNT(*), GROUP_CONCAT(slug ORDER BY id SEPARATOR ',') FROM wp_constraint_like;

DROP TABLE wp_constraint_like;
CREATE TABLE constraint_update_like (
  id INT NOT NULL,
  slug VARCHAR(20) NOT NULL UNIQUE,
  title VARCHAR(20) NOT NULL,
  PRIMARY KEY(id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT INTO constraint_update_like VALUES (1, 'home', 'Home'), (2, 'about', 'About');
UPDATE constraint_update_like SET slug = NULL WHERE id = 2;
UPDATE constraint_update_like SET slug = 'home' WHERE id = 2;
UPDATE constraint_update_like SET id = 1 WHERE slug = 'about';
SELECT id, slug FROM constraint_update_like ORDER BY id;

DROP TABLE constraint_update_like;
CREATE TABLE check_like (
  id INT PRIMARY KEY,
  qty INT,
  CONSTRAINT qty_positive CHECK (qty > 0)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT INTO check_like VALUES (1, 1);
INSERT INTO check_like VALUES (2, 0);
UPDATE check_like SET qty = -1 WHERE id = 1;
SELECT id, qty FROM check_like ORDER BY id;

DROP TABLE check_like;
CREATE TABLE fk_parent_like (
  id INT PRIMARY KEY
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE fk_child_like (
  id INT PRIMARY KEY,
  parent_id INT,
  CONSTRAINT fk_parent FOREIGN KEY(parent_id) REFERENCES fk_parent_like(id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT INTO fk_child_like VALUES (1, 9);
INSERT INTO fk_parent_like VALUES (1), (2);
INSERT INTO fk_child_like VALUES (2, 1);
UPDATE fk_child_like SET parent_id = 8 WHERE id = 2;
DELETE FROM fk_parent_like WHERE id = 1;
UPDATE fk_parent_like SET id = 3 WHERE id = 1;
SELECT COUNT(*), GROUP_CONCAT(id ORDER BY id SEPARATOR ',') FROM fk_child_like;
SELECT COUNT(*), GROUP_CONCAT(id ORDER BY id SEPARATOR ',') FROM fk_parent_like;

DROP DATABASE mylite_constraint_diagnostics;
