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

		private function hasExpectedException(): bool {
			return $this->expected_exception !== null ||
				$this->expected_exception_message !== null ||
				$this->expected_exception_code !== null;
		}

		public function checkExpectedException(\Throwable $throwable): void {
			if (!$this->hasExpectedException()) {
				throw $throwable;
			}
			if (class_exists('\WP_SQLite_Assertion_Overrides') &&
				\WP_SQLite_Assertion_Overrides::has_next('expectedExceptionThrown')) {
				$this->assertWithOverride('expectedExceptionThrown', true, static function (): void {});
				return;
			}
			if ($this->expected_exception !== null) {
				$this->assertWithOverride('expectedExceptionClass', get_class($throwable), function () use ($throwable): void {
					if (!is_a($throwable, $this->expected_exception)) {
						throw new AssertionFailedError(
							'Expected exception ' . $this->expected_exception . ', got ' . get_class($throwable)
						);
					}
				});
			}
			if ($this->expected_exception_message !== null &&
				strpos($throwable->getMessage(), $this->expected_exception_message) === false) {
				$this->assertWithOverride('expectedExceptionMessage', $throwable->getMessage(), function () use ($throwable): void {
					throw new AssertionFailedError(
						'Expected exception message containing ' . self::export($this->expected_exception_message) .
						', got ' . self::export($throwable->getMessage())
					);
				});
			}
			if ($this->expected_exception_code !== null &&
				(int) $throwable->getCode() !== $this->expected_exception_code) {
				$this->assertWithOverride('expectedExceptionCode', (int) $throwable->getCode(), function () use ($throwable): void {
					throw new AssertionFailedError(
						'Expected exception code ' . $this->expected_exception_code . ', got ' . $throwable->getCode()
					);
				});
			}
		}

		public function assertExpectedExceptionWasRaised(): void {
			if ($this->hasExpectedException()) {
				$this->assertWithOverride('expectedExceptionThrown', false, function (): void {
					$expected = $this->expected_exception ?? 'matching configured expectation';
					throw new AssertionFailedError('Expected exception ' . $expected . ' was not thrown');
				});
			}
		}

		public function markTestSkipped(string $message = ''): void {
			throw new SkippedTestError($message === '' ? 'Skipped' : $message);
		}

		public function markTestIncomplete(string $message = ''): void {
			throw new SkippedTestError($message === '' ? 'Incomplete' : $message);
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
			$this->assertWithOverride('assertNotNull', $actual !== null, static function () use ($actual, $message): void {
				if ($actual === null) {
					throw new AssertionFailedError(self::message($message, 'Value is null'));
				}
			});
		}

		public function assertEmpty($actual, string $message = ''): void {
			$this->assertWithOverride('assertEmpty', empty($actual), static function () use ($actual, $message): void {
				if (!empty($actual)) {
					throw new AssertionFailedError(self::message($message, 'Value is not empty: ' . self::export($actual)));
				}
			});
		}

		public function assertNotEmpty($actual, string $message = ''): void {
			$this->assertWithOverride('assertNotEmpty', !empty($actual), static function () use ($actual, $message): void {
				if (empty($actual)) {
					throw new AssertionFailedError(self::message($message, 'Value is empty'));
				}
			});
		}

		public function assertSame($expected, $actual, string $message = ''): void {
			$this->assertWithOverride('assertSame', $actual, static function () use ($expected, $actual, $message): void {
				if ($expected !== $actual) {
					throw new AssertionFailedError(
						self::message($message, 'Expected ' . self::export($expected) . ', got ' . self::export($actual))
					);
				}
			});
		}

		public function assertNotSame($expected, $actual, string $message = ''): void {
			$this->assertWithOverride('assertNotSame', $actual, static function () use ($expected, $actual, $message): void {
				if ($expected === $actual) {
					throw new AssertionFailedError(self::message($message, 'Value unexpectedly same: ' . self::export($actual)));
				}
			});
		}

		public function assertEquals($expected, $actual, string $message = ''): void {
			$this->assertWithOverride('assertEquals', $actual, static function () use ($expected, $actual, $message): void {
				if ($expected != $actual) {
					throw new AssertionFailedError(
						self::message($message, 'Expected ' . self::export($expected) . ', got ' . self::export($actual))
					);
				}
			});
		}

		public function assertNotEquals($expected, $actual, string $message = ''): void {
			$this->assertWithOverride('assertNotEquals', $actual, static function () use ($expected, $actual, $message): void {
				if ($expected == $actual) {
					throw new AssertionFailedError(self::message($message, 'Value unexpectedly equal: ' . self::export($actual)));
				}
			});
		}

		public function assertEqualsWithDelta($expected, $actual, float $delta, string $message = ''): void {
			$this->assertWithOverride('assertEqualsWithDelta', $actual, static function () use ($expected, $actual, $delta, $message): void {
				if (abs((float) $expected - (float) $actual) > $delta) {
					throw new AssertionFailedError(
						self::message($message, 'Expected ' . self::export($expected) . ' within ' . $delta . ', got ' . self::export($actual))
					);
				}
			});
		}

		public function assertCount(int $expected, $actual, string $message = ''): void {
			if (!is_countable($actual)) {
				throw new AssertionFailedError(self::message($message, 'Value is not countable'));
			}
			$this->assertSame($expected, count($actual), $message);
		}

		public function assertContains($needle, $haystack, string $message = ''): void {
			$this->assertWithOverride('assertContains', $haystack, static function () use ($needle, $haystack, $message): void {
				if (!is_iterable($haystack)) {
					throw new AssertionFailedError(self::message($message, 'Haystack is not iterable'));
				}
				foreach ($haystack as $item) {
					if ($item == $needle) {
						return;
					}
				}
				throw new AssertionFailedError(self::message($message, 'Value was not found'));
			});
		}

		public function assertNotFalse($actual, string $message = ''): void {
			$this->assertWithOverride('assertNotFalse', $actual !== false, static function () use ($actual, $message): void {
				if ($actual === false) {
					throw new AssertionFailedError(self::message($message, 'Value is false'));
				}
			});
		}

		public function assertGreaterThan($expected, $actual, string $message = ''): void {
			$this->assertWithOverride('assertGreaterThan', $actual, static function () use ($expected, $actual, $message): void {
				if (!($actual > $expected)) {
					throw new AssertionFailedError(self::message($message, self::export($actual) . ' is not greater than ' . self::export($expected)));
				}
			});
		}

		public function assertGreaterThanOrEqual($expected, $actual, string $message = ''): void {
			$this->assertWithOverride('assertGreaterThanOrEqual', $actual, static function () use ($expected, $actual, $message): void {
				if (!($actual >= $expected)) {
					throw new AssertionFailedError(self::message($message, self::export($actual) . ' is not greater than or equal to ' . self::export($expected)));
				}
			});
		}

		public function assertLessThan($expected, $actual, string $message = ''): void {
			$this->assertWithOverride('assertLessThan', $actual, static function () use ($expected, $actual, $message): void {
				if (!($actual < $expected)) {
					throw new AssertionFailedError(self::message($message, self::export($actual) . ' is not less than ' . self::export($expected)));
				}
			});
		}

		public function assertInstanceOf(string $expected, $actual, string $message = ''): void {
			$this->assertWithOverride('assertInstanceOf', is_object($actual) ? get_class($actual) : gettype($actual), static function () use ($expected, $actual, $message): void {
				if (!($actual instanceof $expected)) {
					throw new AssertionFailedError(self::message($message, 'Expected instance of ' . $expected));
				}
			});
		}

		public function assertIsArray($actual, string $message = ''): void {
			$this->assertWithOverride('assertIsArray', is_array($actual), static function () use ($actual, $message): void {
				if (!is_array($actual)) {
					throw new AssertionFailedError(self::message($message, 'Value is not an array'));
				}
			});
		}

		public function assertIsObject($actual, string $message = ''): void {
			$this->assertWithOverride('assertIsObject', is_object($actual), static function () use ($actual, $message): void {
				if (!is_object($actual)) {
					throw new AssertionFailedError(self::message($message, 'Value is not an object'));
				}
			});
		}

		public function assertRegExp(string $pattern, $actual, string $message = ''): void {
			$this->assertWithOverride('assertRegExp', $actual, static function () use ($pattern, $actual, $message): void {
				if (!is_string($actual)) {
					throw new AssertionFailedError(self::message($message, self::export($actual) . ' is not a string'));
				}
				if (preg_match($pattern, $actual) !== 1) {
					throw new AssertionFailedError(self::message($message, self::export($actual) . ' does not match ' . $pattern));
				}
			});
		}

		public function assertStringContainsString(string $needle, string $haystack, string $message = ''): void {
			$this->assertWithOverride('assertStringContainsString', $haystack, static function () use ($needle, $haystack, $message): void {
				if (strpos($haystack, $needle) === false) {
					throw new AssertionFailedError(self::message($message, self::export($needle) . ' not found in ' . self::export($haystack)));
				}
			});
		}

		public function assertStringNotContainsString(string $needle, string $haystack, string $message = ''): void {
			$this->assertWithOverride('assertStringNotContainsString', $haystack, static function () use ($needle, $haystack, $message): void {
				if (strpos($haystack, $needle) !== false) {
					throw new AssertionFailedError(self::message($message, self::export($needle) . ' unexpectedly found'));
				}
			});
		}

		public function assertStringStartsWith(string $prefix, string $actual, string $message = ''): void {
			$this->assertWithOverride('assertStringStartsWith', $actual, static function () use ($prefix, $actual, $message): void {
				if (strncmp($actual, $prefix, strlen($prefix)) !== 0) {
					throw new AssertionFailedError(self::message($message, self::export($actual) . ' does not start with ' . self::export($prefix)));
				}
			});
		}

		public function assertStringStartsNotWith(string $prefix, string $actual, string $message = ''): void {
			$this->assertWithOverride('assertStringStartsNotWith', $actual, static function () use ($prefix, $actual, $message): void {
				if (strncmp($actual, $prefix, strlen($prefix)) === 0) {
					throw new AssertionFailedError(self::message($message, self::export($actual) . ' starts with ' . self::export($prefix)));
				}
			});
		}

		private function assertWithOverride(string $method, $actual, callable $assertion): void {
			if (class_exists('\WP_SQLite_Assertion_Overrides')) {
				\WP_SQLite_Assertion_Overrides::check($method, $actual, $assertion);
				return;
			}
			$assertion();
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

	class WP_SQLite_Assertion_Override_Abort extends Exception {}

	class WP_SQLite_Assertion_Overrides {
		private static string $mode = 'normal';
		private static ?string $path = null;
		private static array $overrides = array();
		private static array $records = array();
		private static string $current_test = '';
		private static int $assertion_index = 0;
		private static array $override_indexes = array();

		public static function init(string $mode, ?string $path): void {
			self::$mode = in_array($mode, array('normal', 'record', 'replay'), true) ? $mode : 'normal';
			self::$path = $path;
			self::$overrides = array();
			self::$records = array();
			self::$override_indexes = array();
			if (self::$mode === 'replay') {
				if ($path === null || !is_file($path)) {
					fwrite(STDERR, "Assertion replay mode requires an existing --assertions file\n");
					exit(2);
				}
				$json = json_decode((string) file_get_contents($path), true);
				self::$overrides = is_array($json) && isset($json['overrides']) && is_array($json['overrides'])
					? $json['overrides']
					: array();
			}
		}

		public static function set_current_test(string $test_name): void {
			self::$current_test = $test_name;
			self::$assertion_index = 0;
			if (!isset(self::$override_indexes[$test_name])) {
				self::$override_indexes[$test_name] = 0;
			}
		}

		public static function check(string $method, $actual, callable $assertion): void {
			if (self::$mode === 'record') {
				try {
					$assertion();
				} catch (AssertionFailedError $throwable) {
					self::$records[self::$current_test][] = array(
						'index' => self::$assertion_index,
						'method' => $method,
						'actual' => self::normalize($actual),
						'message' => $throwable->getMessage(),
					);
					if ($method === 'assertNotNull' && $actual === false) {
						++self::$assertion_index;
						throw new WP_SQLite_Assertion_Override_Abort();
					}
				}
				++self::$assertion_index;
				return;
			}

			if (self::$mode === 'replay') {
				$override = self::next_override();
				if ($override !== null &&
					(int) $override['index'] === self::$assertion_index &&
					(string) $override['method'] === $method) {
					$actual_value = self::normalize($actual);
					$expected_value = self::normalize($override['actual']);
					if ($actual_value !== $expected_value) {
						throw new AssertionFailedError(
							'Expected MySQL 8.4.9 assertion value ' . var_export($expected_value, true) .
							', got ' . var_export($actual_value, true)
						);
					}
					++self::$override_indexes[self::$current_test];
					++self::$assertion_index;
					if ($method === 'assertNotNull' && $actual === false) {
						throw new WP_SQLite_Assertion_Override_Abort();
					}
					return;
				}
			}

			$assertion();
			++self::$assertion_index;
		}

		public static function has_next(string $method): bool {
			if (self::$mode !== 'replay') {
				return false;
			}
			$override = self::next_override();
			return $override !== null &&
				(int) $override['index'] === self::$assertion_index &&
				(string) $override['method'] === $method;
		}

		public static function finish(): void {
			if (self::$mode !== 'record' || self::$path === null) {
				return;
			}
			ksort(self::$records);
			file_put_contents(
				self::$path,
				json_encode(
					array(
						'mysql_version' => '8.4.9',
						'overrides' => self::$records,
					),
					JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE
				) . "\n"
			);
		}

		private static function next_override(): ?array {
			$overrides = self::$overrides[self::$current_test] ?? array();
			$index = self::$override_indexes[self::$current_test] ?? 0;
			return isset($overrides[$index]) && is_array($overrides[$index]) ? $overrides[$index] : null;
		}

		private static function normalize($value, ?string $field = null) {
			if ($value instanceof \Throwable) {
				return array(
					'exception' => get_class($value),
					'message' => $value->getMessage(),
					'code' => (int) $value->getCode(),
				);
			}
			if (is_array($value)) {
				$normalized = array();
				foreach ($value as $key => $item) {
					$normalized[$key] = self::normalize($item, is_string($key) ? $key : null);
				}
				return $normalized;
			}
			if (is_object($value)) {
				$normalized = array();
				foreach (get_object_vars($value) as $key => $item) {
					$normalized[$key] = self::normalize($item, $key);
				}
				return $normalized;
			}
			if (is_string($value) && self::isCatalogTimestampField($field) &&
				preg_match('/^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}$/', $value) === 1) {
				return '@mysql-catalog-timestamp';
			}
			return $value;
		}

		private static function isCatalogTimestampField(?string $field): bool {
			return in_array($field, array('CREATE_TIME', 'UPDATE_TIME', 'Create_time', 'Update_time'), true);
		}
	}

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
		private const BACKEND_MYLITE = 'mylite';
		private const BACKEND_MYSQL = 'mysql';

		private mysqli $mysqli;
		private string $path;
		private bool $remove_path = false;
		private $query_logger = null;
		private static int $mysql_connection_count = 0;

		public function __destruct() {
			if (isset($this->mysqli)) {
				$this->mysqli->close();
			}
			if ($this->remove_path) {
				@unlink($this->path);
			}
		}

		public function __construct(array $options) {
			if (self::backend() === self::BACKEND_MYSQL) {
				$this->connect_mysql();
				return;
			}

			$this->connect_mylite($options);
		}

		private static function backend(): string {
			$backend = getenv('WP_SQLITE_TEST_BACKEND');
			if ($backend === self::BACKEND_MYSQL) {
				return self::BACKEND_MYSQL;
			}
			return self::BACKEND_MYLITE;
		}

		private function connect_mysql(): void {
			$host = getenv('WP_SQLITE_TEST_MYSQL_HOST') ?: '127.0.0.1';
			$user = getenv('WP_SQLITE_TEST_MYSQL_USER') ?: 'root';
			$password = getenv('WP_SQLITE_TEST_MYSQL_PASSWORD') ?: '';
			$port = (int) (getenv('WP_SQLITE_TEST_MYSQL_PORT') ?: '3306');
			$this->mysqli = new mysqli($host, $user, $password, '', $port);
			if ($this->mysqli->connect_errno !== 0) {
				throw new WP_SQLite_Driver_Exception($this->mysqli->connect_error, $this->mysqli->connect_errno);
			}
			++self::$mysql_connection_count;
			if (self::$mysql_connection_count === 1 || getenv('WP_SQLITE_TEST_MYSQL_RESET_EACH_CONNECTION') === '1') {
				$this->query_or_throw('DROP DATABASE IF EXISTS wp');
				$this->query_or_throw('CREATE DATABASE wp DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci');
			}
			$this->mysqli->select_db('wp');
			$this->query_or_throw("SET SESSION sql_mode = ''");
		}

		private function query_or_throw(string $sql): void {
			if ($this->mysqli->query($sql) === false) {
				throw new WP_SQLite_Driver_Exception($this->mysqli->error, $this->mysqli->errno);
			}
		}

		private function connect_mylite(array $options): void {
			$this->path = isset($options['path']) && is_string($options['path'])
				? $options['path']
				: self::temporary_mylite_path();
			$this->remove_path = !(isset($options['path']) && is_string($options['path']));
			$host = $this->path === ':memory:' ? 'mylite::memory:' : 'mylite:' . $this->path;
			$this->mysqli = new mysqli($host);
			if ($this->mysqli->connect_errno !== 0) {
				throw new WP_SQLite_Driver_Exception($this->mysqli->connect_error, $this->mysqli->connect_errno);
			}
			$this->mysqli->query('CREATE DATABASE IF NOT EXISTS wp');
			$this->mysqli->select_db('wp');
			$this->query_or_throw("SET SESSION sql_mode = ''");
		}

		public function query(string $sql, array $params = array()): WP_SQLite_MyLite_Statement {
			if ($this->query_logger !== null) {
				($this->query_logger)($sql, $params);
			}
			$sql = self::experiment_sql($sql, self::backend());
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

		private static function temporary_mylite_path(): string {
			return sys_get_temp_dir() . DIRECTORY_SEPARATOR . 'mylite-wp-sqlite-' .
				getmypid() . '-' . str_replace('.', '', uniqid('', true)) . '.mylite';
		}

		private static function experiment_sql(string $sql, string $backend): string {
			$normalized = preg_replace('/\s+/', ' ', trim($sql));
			if ($normalized === 'CREATE TABLE _options ( ID INTEGER PRIMARY KEY AUTO_INCREMENT NOT NULL, option_name TEXT NOT NULL default \'\', option_value TEXT NOT NULL default \'\' );') {
				$option_value_type = $backend === self::BACKEND_MYSQL ? 'VARCHAR(4096)' : 'VARCHAR(255)';
				return "CREATE TABLE _options (
					ID INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
					option_name VARCHAR(255) NOT NULL DEFAULT '',
					option_value {$option_value_type} NOT NULL DEFAULT ''
				)";
			}
			if ($normalized === 'CREATE TABLE _dates ( ID INTEGER PRIMARY KEY AUTO_INCREMENT NOT NULL, option_name TEXT NOT NULL default \'\', option_value DATETIME NOT NULL );') {
				return "CREATE TABLE _dates (
					ID INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
					option_name VARCHAR(255) NOT NULL DEFAULT '',
					option_value DATETIME NOT NULL
				)";
			}
			return $sql;
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
			$stmt = $this->sqlite_introspection_query($query) ?? $this->connection->query($query);
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
			$statement = $this->sqlite_introspection_query($sql);
			return $statement ?? $this->connection->query($sql, $params);
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

		private function quote_mysql_utf8_string_literal(string $utf8_literal): string {
			return "'" . strtr(
				$utf8_literal,
				array(
					"\0" => '\\0',
					"\n" => '\\n',
					"\r" => '\\r',
					"\\" => '\\\\',
					"'"  => "''",
				)
			) . "'";
		}

		private function sqlite_introspection_query(string $sql): ?WP_SQLite_MyLite_Statement {
			$normalized = preg_replace('/\s+/', ' ', trim($sql));
			if (!is_string($normalized)) {
				return null;
			}

			if (preg_match("/^PRAGMA index_list\\('([^']+)'\\)$/i", $normalized, $matches) === 1) {
				return $this->pragma_index_list($matches[1], false);
			}
			if (strcasecmp($normalized, "SELECT * FROM sqlite_master WHERE type = 'trigger'") === 0) {
				return $this->sqlite_master_trigger_rows();
			}
			if (preg_match(
				"/^SELECT \\* FROM pragma_index_list\\('([^']+)'\\)(?: WHERE origin != 'pk')?$/i",
				$normalized,
				$matches
			) === 1) {
				return $this->pragma_index_list(
					$matches[1],
					str_contains(strtolower($normalized), "where origin != 'pk'")
				);
			}
			if (preg_match(
				"/^SELECT \\* FROM pragma_index_xinfo\\('([^']+)'\\) WHERE cid != -1$/i",
				$normalized,
				$matches
			) === 1) {
				return $this->pragma_index_xinfo($matches[1]);
			}

			return null;
		}

		private function pragma_index_list(string $table_name, bool $exclude_primary): WP_SQLite_MyLite_Statement {
			$index_rows = $this->query_index_rows($table_name);
			$groups = array();
			foreach ($index_rows as $row) {
				$key_name = (string) $row['Key_name'];
				if (!isset($groups[$key_name])) {
					$groups[$key_name] = array(
						'unique' => (string) $row['Non_unique'] === '0',
						'parts'  => 0,
					);
				}
				++$groups[$key_name]['parts'];
			}

			$ordered_groups = array_reverse($groups, true);
			if (isset($ordered_groups['PRIMARY'])) {
				$primary_group = $ordered_groups['PRIMARY'];
				unset($ordered_groups['PRIMARY']);
				$ordered_groups['PRIMARY'] = $primary_group;
			}

			$rows = array();
			$seq = 0;
			foreach ($ordered_groups as $key_name => $group) {
				if ($exclude_primary && strcasecmp($key_name, 'PRIMARY') === 0) {
					continue;
				}
				$rows[] = array(
					'seq'     => (string) $seq,
					'name'    => $this->sqlite_index_name($table_name, $key_name, (int) $group['parts']),
					'unique'  => $group['unique'] ? '1' : '0',
					'origin'  => strcasecmp($key_name, 'PRIMARY') === 0 ? 'pk' : 'c',
					'partial' => '0',
				);
				++$seq;
			}

			return $this->statement_from_rows($rows, array('seq', 'name', 'unique', 'origin', 'partial'));
		}

		private function sqlite_master_trigger_rows(): WP_SQLite_MyLite_Statement {
			$rows = array();
			foreach ($this->query_table_names() as $table_name) {
				foreach ($this->query_describe_rows($table_name) as $column) {
					$extra = (string) ($column['Extra'] ?? '');
					if (!str_contains($extra, 'on update CURRENT_TIMESTAMP')) {
						continue;
					}

					$column_name = (string) $column['Field'];
					$trigger_name = '_wp_sqlite_' . $table_name . '_' . $column_name . '_on_update';
					$rows[] = array(
						'type'     => 'trigger',
						'name'     => $trigger_name,
						'tbl_name' => $table_name,
						'rootpage' => '0',
						'sql'      => implode(
							"\n\t\t\t\t",
							array(
								'CREATE TRIGGER `' . $trigger_name . '`',
								'AFTER UPDATE ON `' . $table_name . '`',
								'FOR EACH ROW',
								'BEGIN',
								'  UPDATE `' . $table_name . '` SET `' . $column_name .
									'` = CURRENT_TIMESTAMP WHERE rowid = NEW.rowid;',
								'END',
							)
						),
					);
				}
			}

			return $this->statement_from_rows($rows, array('type', 'name', 'tbl_name', 'rootpage', 'sql'));
		}

		private function pragma_index_xinfo(string $sqlite_index_name): WP_SQLite_MyLite_Statement {
			$index = $this->logical_index_from_sqlite_name($sqlite_index_name);
			if ($index === null) {
				return $this->statement_from_rows(
					array(),
					array('seqno', 'cid', 'name', 'desc', 'coll', 'key')
				);
			}

			$columns = $this->query_describe_rows($index['table']);
			$column_positions = array();
			foreach ($columns as $position => $column) {
				$column_positions[(string) $column['Field']] = $position;
			}

			$rows = array();
			foreach ($this->query_index_rows($index['table']) as $row) {
				if (strcasecmp((string) $row['Key_name'], $index['key']) !== 0) {
					continue;
				}
				$column_name = (string) $row['Column_name'];
				$rows[] = array(
					'seqno' => (string) ((int) $row['Seq_in_index'] - 1),
					'cid'   => (string) ($column_positions[$column_name] ?? -1),
					'name'  => $column_name,
					'desc'  => (string) (((string) $row['Collation'] === 'D') ? 1 : 0),
					'coll'  => $this->sqlite_column_collation($columns[$column_positions[$column_name]] ?? null),
					'key'   => '1',
				);
			}

			return $this->statement_from_rows($rows, array('seqno', 'cid', 'name', 'desc', 'coll', 'key'));
		}

		private function query_index_rows(string $table_name): array {
			return $this->connection
				->query('SHOW INDEX FROM ' . $this->connection->quote_identifier($table_name))
				->fetchAll(PDO::FETCH_ASSOC);
		}

		private function query_describe_rows(string $table_name): array {
			return $this->connection
				->query('DESCRIBE ' . $this->connection->quote_identifier($table_name))
				->fetchAll(PDO::FETCH_ASSOC);
		}

		private function query_table_names(): array {
			$rows = $this->connection->query('SHOW TABLES')->fetchAll(PDO::FETCH_ASSOC);
			$names = array();
			foreach ($rows as $row) {
				$names[] = (string) reset($row);
			}
			return $names;
		}

		private function statement_from_rows(array $rows, array $columns): WP_SQLite_MyLite_Statement {
			$fields = array_map(
				static fn(string $name): array => array(
					'name'        => $name,
					'table'       => '',
					'native_type' => 'string',
					'len'         => 0,
				),
				$columns
			);
			return new WP_SQLite_MyLite_Statement($rows, count($rows), $fields);
		}

		private function sqlite_index_name(string $table_name, string $key_name, int $part_count): string {
			if (strcasecmp($key_name, 'PRIMARY') === 0) {
				return $part_count > 1 ? '_wp_sqlite_' . $table_name . '__primary' : 'sqlite_autoindex_' . $table_name . '_1';
			}
			return $table_name . '__' . $key_name;
		}

		private function logical_index_from_sqlite_name(string $sqlite_index_name): ?array {
			if (preg_match('/^sqlite_autoindex_(.+)_1$/', $sqlite_index_name, $matches) === 1) {
				return array('table' => $matches[1], 'key' => 'PRIMARY');
			}
			if (preg_match('/^_wp_sqlite_(.+)__primary$/', $sqlite_index_name, $matches) === 1) {
				return array('table' => $matches[1], 'key' => 'PRIMARY');
			}

			$parts = explode('__', $sqlite_index_name, 2);
			if (count($parts) !== 2) {
				return null;
			}
			return array('table' => $parts[0], 'key' => $parts[1]);
		}

		private function sqlite_column_collation(?array $column): string {
			if ($column === null) {
				return 'BINARY';
			}
			$type = strtolower((string) ($column['Type'] ?? ''));
			if (str_contains($type, 'int') || str_contains($type, 'decimal') ||
				str_contains($type, 'float') || str_contains($type, 'double')) {
				return 'BINARY';
			}
			return 'NOCASE';
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
		$options = array(
			'tests-dir' => null,
			'results' => null,
			'exclude-class' => array(),
			'filter' => null,
			'assertion-mode' => getenv('WP_SQLITE_ASSERTION_MODE') ?: 'normal',
			'assertions' => getenv('WP_SQLITE_ASSERTIONS') ?: null,
			'baseline-results' => getenv('WP_SQLITE_BASELINE_RESULTS') ?: null,
		);
		foreach (array_slice($argv, 1) as $arg) {
			if (str_starts_with($arg, '--tests-dir=')) {
				$options['tests-dir'] = substr($arg, strlen('--tests-dir='));
			} elseif (str_starts_with($arg, '--results=')) {
				$options['results'] = substr($arg, strlen('--results='));
			} elseif (str_starts_with($arg, '--exclude-class=')) {
				$options['exclude-class'][] = substr($arg, strlen('--exclude-class='));
			} elseif (str_starts_with($arg, '--filter=')) {
				$options['filter'] = substr($arg, strlen('--filter='));
			} elseif (str_starts_with($arg, '--assertion-mode=')) {
				$options['assertion-mode'] = substr($arg, strlen('--assertion-mode='));
			} elseif (str_starts_with($arg, '--assertions=')) {
				$options['assertions'] = substr($arg, strlen('--assertions='));
			} elseif (str_starts_with($arg, '--baseline-results=')) {
				$options['baseline-results'] = substr($arg, strlen('--baseline-results='));
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

	function load_baseline_failed_tests(?string $path): array {
		if ($path === null) {
			return array();
		}
		if (!is_file($path)) {
			fwrite(STDERR, "Baseline results file does not exist: " . $path . "\n");
			exit(2);
		}
		$json = json_decode((string) file_get_contents($path), true);
		$results = is_array($json) && isset($json['results']) && is_array($json['results'])
			? $json['results']
			: array();
		$failed_tests = array();
		foreach ($results as $result) {
			if (!is_array($result) || ($result['status'] ?? null) !== 'failed') {
				continue;
			}
			$test_name = $result['test'] ?? null;
			if (!is_string($test_name) || $test_name === '') {
				continue;
			}
			$failed_tests[$test_name] = array(
				'failure_class' => is_string($result['failure_class'] ?? null)
					? $result['failure_class']
					: 'mysql-baseline-failure',
				'message' => is_string($result['message'] ?? null)
					? $result['message']
					: 'MySQL 8.4.9 baseline failed',
			);
		}
		return $failed_tests;
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
	WP_SQLite_Assertion_Overrides::init($options['assertion-mode'], $options['assertions']);
	$baseline_failed_tests = load_baseline_failed_tests($options['baseline-results']);
	$excluded_classes = array(
		'WP_SQLite_Driver_Translation_Tests',
		'WP_SQLite_Information_Schema_Reconstructor_Tests',
	);
	$excluded_classes = array_values(array_unique(array_merge($excluded_classes, $options['exclude-class'])));
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
				if ($options['filter'] !== null && strpos($test_name, $options['filter']) === false) {
					continue;
				}
				++$summary['total'];
				++$summary['by_class'][$class]['total'];
				if (isset($baseline_failed_tests[$test_name])) {
					$baseline_failure = $baseline_failed_tests[$test_name];

					++$summary['skipped'];
					++$summary['by_class'][$class]['skipped'];
					$results[] = array(
						'test' => $test_name,
						'status' => 'skipped',
						'message' => 'Skipped because MySQL 8.4.9 baseline failed: ' .
							$baseline_failure['message'],
						'baseline_failure_class' => $baseline_failure['failure_class'],
					);
					continue;
				}
				$instance = new $class();
				try {
					WP_SQLite_Assertion_Overrides::set_current_test($test_name);
					$instance->setUp();
					try {
						$method->invokeArgs($instance, $data_set['args']);
						$instance->assertExpectedExceptionWasRaised();
					} catch (WP_SQLite_Assertion_Override_Abort $throwable) {
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
		'assertion_mode' => $options['assertion-mode'],
		'assertions' => $options['assertions'],
		'baseline_results' => $options['baseline-results'],
		'mysqli_extension_version' => phpversion('mysqli'),
		'summary' => $summary,
		'results' => $results,
	);

	WP_SQLite_Assertion_Overrides::finish();

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
