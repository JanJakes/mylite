#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
assertions_path="$repo_root/build/wp-sqlite-mysql-849-assertions.json"
results_path="$repo_root/build/wp-sqlite-mylite-verified-results.json"
upstream_dir="$repo_root/build/wp-sqlite-integration-src"

if [[ "${WP_SQLITE_REFRESH_MYSQL_ASSERTIONS:-0}" == "1" ||
	! -f "$assertions_path" ||
	! -d "$upstream_dir/.git" ]]; then
	"$repo_root/experiments/wp-sqlite-tests/run-mysql-baseline.sh"
else
	git -C "$upstream_dir" fetch --depth=1 origin trunk
	git -C "$upstream_dir" checkout --detach FETCH_HEAD >/dev/null
	echo "Reusing MySQL 8.4.9 assertion overrides from $assertions_path"
fi

docker run --rm -v "$repo_root":/work -w /work php:8.4-cli-bookworm sh -lc '
	set -eu
	apt-get update >/dev/null
	apt-get install -y --no-install-recommends cmake ninja-build build-essential pkg-config >/dev/null
	cmake -S . -B build/docker-php84-latest -G Ninja -DCMAKE_BUILD_TYPE=Release -DMYLITE_WARNINGS_AS_ERRORS=ON >/dev/null
	cmake --build build/docker-php84-latest >/dev/null
'

docker run --rm -v "$repo_root":/work -w /work php:8.4-cli-bookworm sh -lc '
	set -eu
	php \
		-d extension=/work/build/docker-php84-latest/packages/php-ext-mylite/mysqli.so \
		experiments/wp-sqlite-tests/runner.php \
		--tests-dir=/work/build/wp-sqlite-integration-src/packages/mysql-on-sqlite/tests \
		--results=/work/build/wp-sqlite-mylite-verified-results.json \
		--assertion-mode=replay \
		--assertions=/work/build/wp-sqlite-mysql-849-assertions.json \
		--baseline-results=/work/build/wp-sqlite-mysql-849-results.json
'

echo "MyLite verified results written to $results_path"
echo "Used MySQL 8.4.9 assertion overrides from $assertions_path"
