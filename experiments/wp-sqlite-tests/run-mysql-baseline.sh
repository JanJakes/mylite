#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
upstream_dir="$repo_root/build/wp-sqlite-integration-src"
results_path="$repo_root/build/wp-sqlite-mysql-849-results.json"
assertions_path="$repo_root/build/wp-sqlite-mysql-849-assertions.json"

if [[ ! -d "$upstream_dir/.git" ]]; then
	rm -rf "$upstream_dir"
	git clone --depth=1 --branch=trunk https://github.com/WordPress/sqlite-database-integration.git "$upstream_dir"
else
	git -C "$upstream_dir" fetch --depth=1 origin trunk
	git -C "$upstream_dir" checkout --detach FETCH_HEAD >/dev/null
fi

network="mylite-wp-sqlite-$$"
mysql_name="mylite-mysql-849-$$"

cleanup() {
	docker rm -f "$mysql_name" >/dev/null 2>&1 || true
	docker network rm "$network" >/dev/null 2>&1 || true
}
trap cleanup EXIT

docker network create "$network" >/dev/null
docker run -d --name "$mysql_name" --network "$network" -e MYSQL_ROOT_PASSWORD=mylite mysql:8.4.9 >/dev/null

for attempt in $(seq 1 90); do
	if docker exec "$mysql_name" mysqladmin ping -uroot -pmylite --silent >/dev/null 2>&1; then
		break
	fi
	sleep 1
	if [[ "$attempt" -eq 90 ]]; then
		docker logs "$mysql_name" >&2
		exit 1
	fi
done

docker run --rm --network "$network" -v "$repo_root":/work -w /work php:8.4-cli-bookworm sh -lc "
	set -eu
	docker-php-ext-install mysqli >/dev/null
	WP_SQLITE_TEST_BACKEND=mysql \\
	WP_SQLITE_TEST_MYSQL_HOST=$mysql_name \\
	WP_SQLITE_TEST_MYSQL_USER=root \\
	WP_SQLITE_TEST_MYSQL_PASSWORD=mylite \\
	WP_SQLITE_TEST_MYSQL_RESET_EACH_CONNECTION=1 \\
	php experiments/wp-sqlite-tests/runner.php \\
		--tests-dir=/work/build/wp-sqlite-integration-src/packages/mysql-on-sqlite/tests \\
		--results=/work/build/wp-sqlite-mysql-849-results.json \\
		--assertion-mode=record \\
		--assertions=/work/build/wp-sqlite-mysql-849-assertions.json
"

echo "MySQL 8.4.9 results written to $results_path"
echo "MySQL 8.4.9 assertion overrides written to $assertions_path"
