<?php

namespace PHPUnit\Framework {
	class AssertionFailedError extends \Exception {}
	class SkippedTestError extends \Exception {}

	class TestCase {
		private ?string $expected_exception = null;
		private ?string $expected_exception_message = null;
		private ?int $expected_exception_code = null;

		public static function setUpBeforeClass(): void {}
		public static function tearDownAfterClass(): void {}
		public function setUp(): void {}
		public function tearDown(): void {}

		public function expectException(string $exception): void {
			$this->expected_exception = $exception;
		}

		public function expectExceptionMessage(string $message): void {
			$this->expected_exception_message = $message;
		}

		public function expectExceptionCode(int $code): void {
			$this->expected_exception_code = $code;
		}

		public function getExpectedException(): ?string {
			return $this->expected_exception;
		}

		public function checkExpectedException(\Throwable $throwable): void {
			if ($this->expected_exception === null) {
				throw $throwable;
			}
			if (!is_a($throwable, $this->expected_exception)) {
				throw new AssertionFailedError(
					'Expected exception ' . $this->expected_exception . ', got ' . get_class($throwable)
				);
			}
			if ($this->expected_exception_message !== null &&
				strpos($throwable->getMessage(), $this->expected_exception_message) === false) {
				throw new AssertionFailedError(
					'Expected exception message containing ' . self::export($this->expected_exception_message) .
					', got ' . self::export($throwable->getMessage())
				);
			}
			if ($this->expected_exception_code !== null &&
				(int) $throwable->getCode() !== $this->expected_exception_code) {
				throw new AssertionFailedError(
					'Expected exception code ' . $this->expected_exception_code . ', got ' . $throwable->getCode()
				);
			}
		}

		public function assertExpectedExceptionWasRaised(): void {
			if ($this->expected_exception !== null) {
				throw new AssertionFailedError('Expected exception ' . $this->expected_exception . ' was not thrown');
			}
		}

		public function markTestSkipped(string $message = ''): void {
			throw new SkippedTestError($message === '' ? 'Skipped' : $message);
		}

		public function fail(string $message = ''): void {
			throw new AssertionFailedError($message === '' ? 'Failed' : $message);
		}

		public function assertTrue($actual, string $message = ''): void {
			$this->assertSame(true, $actual, $message);
		}

		public function assertFalse($actual, string $message = ''): void {
			$this->assertSame(false, $actual, $message);
		}

		public function assertNull($actual, string $message = ''): void {
			$this->assertSame(null, $actual, $message);
		}

		public function assertNotNull($actual, string $message = ''): void {
			if ($actual === null) {
				throw new AssertionFailedError(self::message($message, 'Value is null'));
			}
		}

		public function assertEmpty($actual, string $message = ''): void {
			if (!empty($actual)) {
				throw new AssertionFailedError(self::message($message, 'Value is not empty: ' . self::export($actual)));
			}
		}

		public function assertNotEmpty($actual, string $message = ''): void {
			if (empty($actual)) {
				throw new AssertionFailedError(self::message($message, 'Value is empty'));
			}
		}

		public function assertSame($expected, $actual, string $message = ''): void {
			if ($expected !== $actual) {
				throw new AssertionFailedError(
					self::message($message, 'Expected ' . self::export($expected) . ', got ' . self::export($actual))
				);
			}
		}

		public function assertNotSame($expected, $actual, string $message = ''): void {
			if ($expected === $actual) {
				throw new AssertionFailedError(self::message($message, 'Value unexpectedly same: ' . self::export($actual)));
			}
		}

		public function assertEquals($expected, $actual, string $message = ''): void {
			if ($expected != $actual) {
				throw new AssertionFailedError(
					self::message($message, 'Expected ' . self::export($expected) . ', got ' . self::export($actual))
				);
			}
		}

		public function assertNotEquals($expected, $actual, string $message = ''): void {
			if ($expected == $actual) {
				throw new AssertionFailedError(self::message($message, 'Value unexpectedly equal: ' . self::export($actual)));
			}
		}

		public function assertEqualsWithDelta($expected, $actual, float $delta, string $message = ''): void {
			if (abs((float) $expected - (float) $actual) > $delta) {
				throw new AssertionFailedError(
					self::message($message, 'Expected ' . self::export($expected) . ' within ' . $delta . ', got ' . self::export($actual))
				);
			}
		}

		public function assertCount(int $expected, $actual, string $message = ''): void {
			if (!is_countable($actual)) {
				throw new AssertionFailedError(self::message($message, 'Value is not countable'));
			}
			$this->assertSame($expected, count($actual), $message);
		}

		public function assertContains($needle, $haystack, string $message = ''): void {
			if (!is_iterable($haystack)) {
				throw new AssertionFailedError(self::message($message, 'Haystack is not iterable'));
			}
			foreach ($haystack as $item) {
				if ($item == $needle) {
					return;
				}
			}
			throw new AssertionFailedError(self::message($message, 'Value was not found'));
		}

		public function assertNotFalse($actual, string $message = ''): void {
			if ($actual === false) {
				throw new AssertionFailedError(self::message($message, 'Value is false'));
			}
		}

		public function assertGreaterThan($expected, $actual, string $message = ''): void {
			if (!($actual > $expected)) {
				throw new AssertionFailedError(self::message($message, self::export($actual) . ' is not greater than ' . self::export($expected)));
			}
		}

		public function assertGreaterThanOrEqual($expected, $actual, string $message = ''): void {
			if (!($actual >= $expected)) {
				throw new AssertionFailedError(self::message($message, self::export($actual) . ' is not greater than or equal to ' . self::export($expected)));
			}
		}

		public function assertLessThan($expected, $actual, string $message = ''): void {
			if (!($actual < $expected)) {
				throw new AssertionFailedError(self::message($message, self::export($actual) . ' is not less than ' . self::export($expected)));
			}
		}

		public function assertInstanceOf(string $expected, $actual, string $message = ''): void {
			if (!($actual instanceof $expected)) {
				throw new AssertionFailedError(self::message($message, 'Expected instance of ' . $expected));
			}
		}

		public function assertIsArray($actual, string $message = ''): void {
			if (!is_array($actual)) {
				throw new AssertionFailedError(self::message($message, 'Value is not an array'));
			}
		}

		public function assertIsObject($actual, string $message = ''): void {
			if (!is_object($actual)) {
				throw new AssertionFailedError(self::message($message, 'Value is not an object'));
			}
		}

		public function assertRegExp(string $pattern, string $actual, string $message = ''): void {
			if (preg_match($pattern, $actual) !== 1) {
				throw new AssertionFailedError(self::message($message, self::export($actual) . ' does not match ' . $pattern));
			}
		}

		public function assertStringContainsString(string $needle, string $haystack, string $message = ''): void {
			if (strpos($haystack, $needle) === false) {
				throw new AssertionFailedError(self::message($message, self::export($needle) . ' not found in ' . self::export($haystack)));
			}
		}

		public function assertStringNotContainsString(string $needle, string $haystack, string $message = ''): void {
			if (strpos($haystack, $needle) !== false) {
				throw new AssertionFailedError(self::message($message, self::export($needle) . ' unexpectedly found'));
			}
		}

		public function assertStringStartsWith(string $prefix, string $actual, string $message = ''): void {
			if (strncmp($actual, $prefix, strlen($prefix)) !== 0) {
				throw new AssertionFailedError(self::message($message, self::export($actual) . ' does not start with ' . self::export($prefix)));
			}
		}

		public function assertStringStartsNotWith(string $prefix, string $actual, string $message = ''): void {
			if (strncmp($actual, $prefix, strlen($prefix)) === 0) {
				throw new AssertionFailedError(self::message($message, self::export($actual) . ' starts with ' . self::export($prefix)));
			}
		}

		private static function message(string $message, string $fallback): string {
			return $message === '' ? $fallback : $message . ': ' . $fallback;
		}

		private static function export($value): string {
			return var_export($value, true);
		}
	}
}

namespace {
	use PHPUnit\Framework\AssertionFailedError;
	use PHPUnit\Framework\SkippedTestError;

	error_reporting(E_ALL);
	mysqli_report(MYSQLI_REPORT_OFF);

	class WP_SQLite_Driver_Exception extends Exception {}
	class WP_SQLite_Information_Schema_Exception extends Exception {}

	class WP_PDO_MySQL_On_SQLite {
		public const MINIMUM_SQLITE_VERSION = '3.38.0';
		public const RESERVED_PREFIX = '_';
	}

	class WP_SQLite_Fake_PDO {
		private WP_SQLite_Connection $connection;

		public function __construct(WP_SQLite_Connection $connection) {
			$this->connection = $connection;
		}

		public function exec(string $sql) {
			return $this->connection->query($sql)->rowCount();
		}

		public function query(string $sql): WP_SQLite_MyLite_Statement {
			return $this->connection->query($sql);
		}

		public function setAttribute($attribute, $value): bool {
			return true;
		}
	}

	class WP_SQLite_MyLite_Statement {
		private array $rows;
		private int $affected_rows;
		private array $fields;
		private int $cursor = 0;

		public function __construct(array $rows, int $affected_rows, array $fields) {
			$this->rows = $rows;
			$this->affected_rows = $affected_rows;
			$this->fields = $fields;
		}

		public function execute($params = null): bool {
			return true;
		}

		public function columnCount(): int {
			return count($this->fields);
		}

		public function rowCount(): int {
			return $this->affected_rows;
		}

		public function fetchAll($mode = PDO::FETCH_DEFAULT, ...$args): array {
			if ($mode === PDO::FETCH_ASSOC) {
				return $this->rows;
			}
			if ($mode === PDO::FETCH_NUM) {
				return array_map('array_values', $this->rows);
			}
			return array_map(static fn(array $row): object => (object) $row, $this->rows);
		}

		public function fetch($mode = PDO::FETCH_DEFAULT) {
			if ($this->cursor >= count($this->rows)) {
				return false;
			}
			$row = $this->rows[$this->cursor++];
			if ($mode === PDO::FETCH_ASSOC) {
				return $row;
			}
			if ($mode === PDO::FETCH_NUM) {
				return array_values($row);
			}
			return (object) $row;
		}

		public function fetchColumn($column = 0) {
			$row = $this->fetch(PDO::FETCH_NUM);
			return $row === false ? false : ($row[$column] ?? false);
		}

		public function getColumnMeta($column): array {
			return $this->fields[$column] ?? array();
		}
	}

	class WP_SQLite_Connection {
		private mysqli $mysqli;
		private string $path;
		private $query_logger = null;

		public function __construct(array $options) {
			$this->path = isset($options['path']) && is_string($options['path']) ? $options['path'] : ':memory:';
			$host = $this->path === ':memory:' ? 'mylite::memory:' : 'mylite:' . $this->path;
			$this->mysqli = new mysqli($host);
			if ($this->mysqli->connect_errno !== 0) {
				throw new WP_SQLite_Driver_Exception($this->mysqli->connect_error, $this->mysqli->connect_errno);
			}
			$this->mysqli->query('CREATE DATABASE IF NOT EXISTS wp');
			$this->mysqli->select_db('wp');
		}

		public function query(string $sql, array $params = array()): WP_SQLite_MyLite_Statement {
			if ($this->query_logger !== null) {
				($this->query_logger)($sql, $params);
			}
			$result = $this->mysqli->query($sql);
			if ($result === false) {
				throw new WP_SQLite_Driver_Exception($this->mysqli->error, $this->mysqli->errno);
			}
			if ($result instanceof mysqli_result) {
				$fields = array();
				foreach ($result->fetch_fields() as $field) {
					$fields[] = array(
						'name' => $field->name,
						'table' => $field->table,
						'native_type' => self::native_type($field->type),
						'len' => $field->length,
					);
				}
				$rows = array();
				while (($row = $result->fetch_assoc()) !== null) {
					$rows[] = $row;
				}
				return new WP_SQLite_MyLite_Statement($rows, count($rows), $fields);
			}
			return new WP_SQLite_MyLite_Statement(array(), max(0, $this->mysqli->affected_rows), array());
		}

		public function prepare(string $sql): WP_SQLite_MyLite_Statement {
			return $this->query($sql);
		}

		public function get_last_insert_id(): string {
			return (string) $this->mysqli->insert_id;
		}

		public function quote($value, int $type = PDO::PARAM_STR): string {
			if ($value === null) {
				return 'NULL';
			}
			return "'" . $this->mysqli->real_escape_string((string) $value) . "'";
		}

		public function quote_identifier(string $unquoted_identifier): string {
			return '`' . str_replace('`', '``', $unquoted_identifier) . '`';
		}

		public function get_pdo(): WP_SQLite_Fake_PDO {
			return new WP_SQLite_Fake_PDO($this);
		}

		public function get_mysqli(): mysqli {
			return $this->mysqli;
		}

		public function set_query_logger(callable $logger): void {
			$this->query_logger = $logger;
		}

		private static function native_type(int $type): string {
			return match ($type) {
				MYSQLI_TYPE_LONG, MYSQLI_TYPE_LONGLONG, MYSQLI_TYPE_INT24, MYSQLI_TYPE_SHORT, MYSQLI_TYPE_TINY => 'integer',
				MYSQLI_TYPE_FLOAT, MYSQLI_TYPE_DOUBLE, MYSQLI_TYPE_DECIMAL, MYSQLI_TYPE_NEWDECIMAL => 'double',
				default => 'string',
			};
		}
	}

	class WP_SQLite_Driver {
		public string $client_info = 'MyLite mysqli experiment';
		private WP_SQLite_Connection $connection;
		private string $main_db_name;
		private $last_result = null;
		private array $last_column_meta = array();
		private array $last_queries = array();

		public function __construct(WP_SQLite_Connection $connection, string $main_db_name) {
			$this->connection = $connection;
			$this->main_db_name = $main_db_name;
		}

		public function get_connection(): WP_SQLite_Connection {
			return $this->connection;
		}

		public function get_sqlite_version(): string {
			return '3.45.0';
		}

		public function get_saved_driver_version(): string {
			return 'experiment';
		}

		public function is_sql_mode_active(string $mode): bool {
			$result = $this->query('SELECT @@SESSION.sql_mode AS sql_mode');
			$modes = explode(',', (string) ($result[0]->sql_mode ?? ''));
			return in_array(strtoupper($mode), array_map('strtoupper', $modes), true);
		}

		public function get_last_mysql_query(): ?string {
			return $this->last_queries[0]['sql'] ?? null;
		}

		public function get_last_sqlite_queries(): array {
			return $this->last_queries;
		}

		public function get_insert_id() {
			return $this->connection->get_last_insert_id();
		}

		public function query(string $query, $fetch_mode = PDO::FETCH_OBJ, ...$fetch_mode_args) {
			$this->last_queries = array(array('sql' => $query, 'params' => array()));
			$stmt = $this->connection->query($query);
			$this->last_column_meta = array();
			for ($i = 0; $i < $stmt->columnCount(); ++$i) {
				$this->last_column_meta[] = $stmt->getColumnMeta($i);
			}
			if ($stmt->columnCount() > 0) {
				$this->last_result = $stmt->fetchAll($fetch_mode, ...$fetch_mode_args);
			} else {
				$this->last_result = $stmt->rowCount();
			}
			return $this->last_result;
		}

		public function create_parser(string $query) {
			throw new WP_SQLite_Driver_Exception('Parser access is not available in the MyLite experiment harness.');
		}

		public function get_query_results() {
			return $this->last_result;
		}

		public function get_last_return_value() {
			return $this->last_result;
		}

		public function get_last_column_count(): int {
			return count($this->last_column_meta);
		}

		public function get_last_column_meta(): array {
			return $this->last_column_meta;
		}

		public function execute_sqlite_query(string $sql, array $params = array()): WP_SQLite_MyLite_Statement {
			return $this->connection->query($sql, $params);
		}

		public function beginTransaction(): void {
			$this->begin_transaction();
		}

		public function begin_transaction(): void {
			$this->connection->query('START TRANSACTION');
		}

		public function commit(): void {
			$this->connection->query('COMMIT');
		}

		public function rollback(): void {
			$this->connection->query('ROLLBACK');
		}

		public function __set(string $name, $value): void {
			if ($name === 'main_db_name') {
				$this->main_db_name = (string) $value;
			}
		}
	}

	class WP_SQLite_Information_Schema_Builder {
		public function __construct(string $reserved_prefix, WP_SQLite_Connection $connection) {}
	}

	class WP_SQLite_Information_Schema_Reconstructor {
		public function __construct(WP_SQLite_Driver $driver, WP_SQLite_Information_Schema_Builder $builder) {}
		public function ensure_correct_information_schema(): void {}
	}

	function do_action(...$args) {}
	function apply_filters($tag, $value, ...$args) { return $value; }
	function wp_die($message = '', $title = '', $args = array()) { throw new RuntimeException((string) $message); }
	function is_multisite() { return false; }
	function wp_get_db_schema() { return ''; }

	function parse_options(array $argv): array {
		$options = array('tests-dir' => null, 'results' => null);
		foreach (array_slice($argv, 1) as $arg) {
			if (str_starts_with($arg, '--tests-dir=')) {
				$options['tests-dir'] = substr($arg, strlen('--tests-dir='));
			} elseif (str_starts_with($arg, '--results=')) {
				$options['results'] = substr($arg, strlen('--results='));
			}
		}
		if ($options['tests-dir'] === null) {
			fwrite(STDERR, "--tests-dir is required\n");
			exit(2);
		}
		return $options;
	}

	function data_sets_for(ReflectionMethod $method, string $class): array {
		$doc = $method->getDocComment() ?: '';
		if (preg_match('/@dataProvider\s+([A-Za-z0-9_]+)/', $doc, $matches) !== 1) {
			return array(array('name' => null, 'args' => array()));
		}
		$provider = $matches[1];
		$reflection = new ReflectionMethod($class, $provider);
		$provider_result = $reflection->isStatic()
			? $reflection->invoke(null)
			: $reflection->invoke(new $class());
		$data_sets = array();
		foreach ($provider_result as $name => $args) {
			$data_sets[] = array(
				'name' => is_int($name) ? (string) $name : (string) $name,
				'args' => is_array($args) ? $args : array($args),
			);
		}
		return $data_sets;
	}

	function classify_failure(Throwable $throwable): string {
		$message = $throwable->getMessage();
		if (str_contains($message, 'Parser access is not available') || str_contains($message, 'WP_Parser_')) {
			return 'harness-parser-or-translation';
		}
		if (str_contains($message, 'Error occurred while creating tables or indexes')) {
			return 'mylite-query-error';
		}
		if (stripos($message, 'pragma') !== false || stripos($message, 'sqlite') !== false) {
			return 'sqlite-internal-assumption';
		}
		if ($throwable instanceof AssertionFailedError) {
			return 'assertion-divergence';
		}
		if ($throwable instanceof WP_SQLite_Driver_Exception) {
			return 'mylite-query-error';
		}
		return 'harness-or-runtime-error';
	}

	function summarize_message(Throwable $throwable): string {
		$message = trim($throwable->getMessage());
		if ($message === '') {
			$message = get_class($throwable);
		}
		$message = preg_replace('/\s+/', ' ', $message);
		return substr($message, 0, 1000);
	}

	$options = parse_options($argv);
	$tests_dir = rtrim($options['tests-dir'], '/');
	$excluded_classes = array(
		'WP_SQLite_Driver_Translation_Tests',
		'WP_SQLite_Information_Schema_Reconstructor_Tests',
	);
	$files = glob($tests_dir . '/WP_SQLite_*.php');
	sort($files);

	$classes_before = get_declared_classes();
	foreach ($files as $file) {
		if (in_array(basename($file, '.php'), $excluded_classes, true)) {
			continue;
		}
		require_once $file;
	}
	$classes_after = get_declared_classes();
	$test_classes = array_values(array_filter(
		array_diff($classes_after, $classes_before),
		static fn(string $class): bool => str_starts_with($class, 'WP_SQLite_') && str_ends_with($class, '_Tests')
	));
	sort($test_classes);

	$results = array();
	$summary = array(
		'total' => 0,
		'passed' => 0,
		'failed' => 0,
		'skipped' => 0,
		'by_class' => array(),
		'by_failure_class' => array(),
	);

	foreach ($test_classes as $class) {
		$summary['by_class'][$class] = array('total' => 0, 'passed' => 0, 'failed' => 0, 'skipped' => 0);
		try {
			$class::setUpBeforeClass();
		} catch (Throwable $throwable) {
			$reflection = new ReflectionClass($class);
			foreach ($reflection->getMethods(ReflectionMethod::IS_PUBLIC) as $method) {
				if (!str_starts_with($method->getName(), 'test')) {
					continue;
				}
				$name = $class . '::' . $method->getName();
				++$summary['total'];
				++$summary['failed'];
				++$summary['by_class'][$class]['total'];
				++$summary['by_class'][$class]['failed'];
				$failure_class = classify_failure($throwable);
				$summary['by_failure_class'][$failure_class] = ($summary['by_failure_class'][$failure_class] ?? 0) + 1;
				$results[] = array(
					'test' => $name,
					'status' => 'failed',
					'failure_class' => $failure_class,
					'exception' => get_class($throwable),
					'message' => summarize_message($throwable),
				);
			}
			continue;
		}

		$reflection = new ReflectionClass($class);
		foreach ($reflection->getMethods(ReflectionMethod::IS_PUBLIC) as $method) {
			if (!str_starts_with($method->getName(), 'test')) {
				continue;
			}
			foreach (data_sets_for($method, $class) as $data_set) {
				$test_name = $class . '::' . $method->getName();
				if ($data_set['name'] !== null) {
					$test_name .= '#' . $data_set['name'];
				}
				++$summary['total'];
				++$summary['by_class'][$class]['total'];
				$instance = new $class();
				try {
					$instance->setUp();
					try {
						$method->invokeArgs($instance, $data_set['args']);
						$instance->assertExpectedExceptionWasRaised();
					} catch (Throwable $throwable) {
						$instance->checkExpectedException($throwable);
					}
					$instance->tearDown();
					++$summary['passed'];
					++$summary['by_class'][$class]['passed'];
					$results[] = array('test' => $test_name, 'status' => 'passed');
				} catch (SkippedTestError $throwable) {
					++$summary['skipped'];
					++$summary['by_class'][$class]['skipped'];
					$results[] = array(
						'test' => $test_name,
						'status' => 'skipped',
						'message' => summarize_message($throwable),
					);
				} catch (Throwable $throwable) {
					try {
						$instance->tearDown();
					} catch (Throwable $tear_down_throwable) {
					}
					++$summary['failed'];
					++$summary['by_class'][$class]['failed'];
					$failure_class = classify_failure($throwable);
					$summary['by_failure_class'][$failure_class] = ($summary['by_failure_class'][$failure_class] ?? 0) + 1;
					$results[] = array(
						'test' => $test_name,
						'status' => 'failed',
						'failure_class' => $failure_class,
						'exception' => get_class($throwable),
						'message' => summarize_message($throwable),
					);
				}
			}
		}

		try {
			$class::tearDownAfterClass();
		} catch (Throwable $throwable) {
		}
	}

	$output = array(
		'upstream_tests_dir' => $tests_dir,
		'excluded_classes' => $excluded_classes,
		'mysqli_extension_version' => phpversion('mysqli'),
		'summary' => $summary,
		'results' => $results,
	);

	if ($options['results'] !== null) {
		file_put_contents($options['results'], json_encode($output, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES) . "\n");
	}

	echo 'Total: ' . $summary['total'] . PHP_EOL;
	echo 'Passed: ' . $summary['passed'] . PHP_EOL;
	echo 'Failed: ' . $summary['failed'] . PHP_EOL;
	echo 'Skipped: ' . $summary['skipped'] . PHP_EOL;
	echo 'Excluded classes: ' . implode(', ', $excluded_classes) . PHP_EOL;
	foreach ($summary['by_class'] as $class => $class_summary) {
		echo sprintf(
			"%s: %d passed, %d failed, %d skipped / %d\n",
			$class,
			$class_summary['passed'],
			$class_summary['failed'],
			$class_summary['skipped'],
			$class_summary['total']
		);
	}
	if (!empty($summary['by_failure_class'])) {
		echo "Failure classes:\n";
		foreach ($summary['by_failure_class'] as $class => $count) {
			echo sprintf("  %s: %d\n", $class, $count);
		}
	}
}
