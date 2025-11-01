# Database Internals Educational Repository

An educational database internals repository designed to teach core data structures and algorithms used in modern database systems. This project provides modular implementations of key database components including B+ trees, hash indexes, columnar storage, LSM trees, and a query execution engine.

## Project Structure

```
database-internals/
├── btree/              # B+ Tree implementation
├── hash_index/         # Hash Index implementation  
├── columnar_storage/   # Columnar Storage with compression
├── lsm/               # LSM Tree implementation
├── query_engine/      # Query execution engine
├── tests/             # Unit and integration tests
│   ├── unit/          # Unit tests for each module
│   └── integration/   # Integration tests
├── benchmarks/        # Performance benchmarks
├── examples/          # Example programs and demos
└── CMakeLists.txt     # Main build configuration
```

## Prerequisites

- C++17 compatible compiler (GCC 7+, Clang 6+, MSVC 2017+)
- CMake 3.16 or higher
- Git (for fetching dependencies)

## Building the Project

### Quick Start

```bash
# Clone the repository
git clone <repository-url>
cd database-internals

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake ..

# Build the project
make -j$(nproc)
```

### Build Types

```bash
# Debug build (with AddressSanitizer)
cmake -DCMAKE_BUILD_TYPE=Debug ..

# Release build (optimized)
cmake -DCMAKE_BUILD_TYPE=Release ..

# Release with debug info
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
```

## Running Tests

```bash
# Build and run all tests
make run_all_tests

# Run specific test categories
ctest -L unit          # Run only unit tests
ctest -L integration   # Run only integration tests

# Run tests with verbose output
ctest --output-on-failure
```

## Running Benchmarks

```bash
# Build benchmarks
make run_benchmarks

# Run specific benchmarks (after implementation)
./benchmarks/scan_vs_index_bench
./benchmarks/compression_bench
```

## Running Examples

```bash
# Build examples
make build_examples

# Run specific examples (after implementation)
./examples/btree_demo
./examples/query_engine_demo
```

## Development

### Adding New Tests

Tests use Google Test framework. Add new test files to `tests/unit/` or `tests/integration/` and update the corresponding CMakeLists.txt.

### Adding New Benchmarks

Add benchmark files to `benchmarks/` directory and use the `create_benchmark()` function in CMakeLists.txt.

### Code Style

- Follow C++17 best practices
- Use meaningful variable and function names
- Add comprehensive comments for educational purposes
- Maintain consistent indentation (4 spaces)

## Educational Goals

This repository is designed to help students understand:

1. **B+ Trees**: Balanced tree structures for range queries
2. **Hash Indexes**: Fast point lookups with collision handling
3. **Columnar Storage**: Column-oriented data layout with compression
4. **LSM Trees**: Write-optimized storage with background compaction
5. **Query Processing**: SQL parsing, optimization, and execution

## License

[Add appropriate license information]

## Contributing

[Add contribution guidelines]