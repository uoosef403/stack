# Contributing to Network Stack

We welcome contributions to the Network Stack project! This document provides guidelines for contributing to ensure a smooth collaboration process.

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
- [Development Setup](#development-setup)
- [Contributing Process](#contributing-process)
- [Code Style Guidelines](#code-style-guidelines)
- [Testing](#testing)
- [Documentation](#documentation)
- [Issue Reporting](#issue-reporting)

## Code of Conduct

This project adheres to a code of conduct that promotes a welcoming and inclusive environment. By participating, you agree to uphold this code.

## Getting Started

1. **Fork the repository** on GitHub
2. **Clone your fork** locally:
   ```bash
   git clone https://github.com/your-username/stack.git
   cd stack
   ```
3. **Add the upstream remote**:
   ```bash
   git remote add upstream https://github.com/uoosef403/stack.git
   ```

## Development Setup

### Prerequisites

- **C++20** compatible compiler (GCC 11+, Clang 14+, MSVC 19.30+)
- **CMake 3.21+**
- **Git**
- **vcpkg** (automatically bootstrapped)

### Build Instructions

```bash
# Configure and build (Linux/macOS)
./build.sh debug

# Configure and build (Windows)
.\build.ps1 debug

# Run tests
./build.sh test     # Linux/macOS
.\build.ps1 test    # Windows
```

### IDE Setup

#### Visual Studio Code

1. Install recommended extensions:
   - C/C++ Extension Pack
   - CMake Tools
   - clangd (optional, for better IntelliSense)

2. The project includes `.vscode/settings.json` with optimal configurations

#### CLion

1. Open the project root directory
2. CLion will automatically detect the CMake configuration
3. Select the appropriate build profile (Debug/Release)

## Contributing Process

### 1. Create a Feature Branch

```bash
git checkout -b feature/your-feature-name
# or
git checkout -b bugfix/issue-number-description
```

### 2. Make Your Changes

- Follow the [code style guidelines](#code-style-guidelines)
- Write tests for new functionality
- Update documentation as needed
- Ensure all tests pass

### 3. Commit Your Changes

Use clear, descriptive commit messages following conventional commit format:

```bash
git commit -m "feat: add SOCKS5 authentication support"
git commit -m "fix: resolve memory leak in TCP handler"
git commit -m "docs: update configuration examples"
```

### 4. Push and Create Pull Request

```bash
git push origin feature/your-feature-name
```

Then create a pull request on GitHub with:
- Clear description of changes
- Reference to related issues
- Screenshots/examples if applicable

## Code Style Guidelines

### C++ Standards

- **C++20** features are encouraged
- Use **RAII** for resource management
- Prefer **const correctness**
- Use **smart pointers** over raw pointers
- Follow **exception safety** guarantees

### Naming Conventions

```cpp
// Classes: PascalCase
class NetworkStack;

// Functions and variables: snake_case
void process_packet();
int connection_count;

// Constants: UPPER_SNAKE_CASE
constexpr int MAX_CONNECTIONS = 1000;

// Private members: trailing underscore
class Example {
private:
    int member_variable_;
};
```

### Code Formatting

The project uses **clang-format** for consistent formatting:

```bash
# Format all code (if clang-format is available)
./build.sh format     # Linux/macOS
.\build.ps1 format    # Windows (if implemented)
```

### Documentation

- Use **Doxygen-style** comments for public APIs
- Include examples in complex functions
- Keep comments up-to-date with code changes

```cpp
/**
 * @brief Processes incoming network packets
 * @param packet Raw packet data
 * @param size Packet size in bytes
 * @return Processing result code
 * 
 * @example
 * ```cpp
 * auto result = process_packet(data, 1500);
 * if (result == ProcessResult::Success) {
 *     // Handle successful processing
 * }
 * ```
 */
ProcessResult process_packet(const uint8_t* packet, size_t size);
```

## Testing

### Unit Tests

- Write unit tests for all new functionality
- Use **Google Test** framework
- Aim for high code coverage
- Test edge cases and error conditions

```cpp
TEST(TcpHandlerTest, HandlesValidPacket) {
    TcpHandler handler;
    auto packet = create_test_packet();
    
    EXPECT_NO_THROW(handler.process(packet));
    EXPECT_EQ(handler.get_state(), HandlerState::Ready);
}
```

### Integration Tests

- Test with actual network interfaces (requires root/admin)
- Verify end-to-end functionality
- Test with different configurations

### Running Tests

```bash
# Unit tests only
./build.sh test

# Integration tests (requires root)
sudo python3 tests/integration/tcp_test.py
sudo python3 tests/integration/icmp_ping.py
```

## Documentation

### Code Documentation

- Document all public APIs
- Include usage examples
- Explain complex algorithms
- Document configuration options

### User Documentation

- Update README.md for new features
- Add configuration examples
- Update build instructions if needed
- Include troubleshooting information

## Issue Reporting

### Bug Reports

When reporting bugs, include:

1. **Environment Information**:
   - Operating system and version
   - Compiler and version
   - CMake version

2. **Steps to Reproduce**:
   - Minimal code example
   - Configuration used
   - Command-line arguments

3. **Expected vs Actual Behavior**

4. **Additional Context**:
   - Log output
   - Stack traces
   - Network configuration

### Feature Requests

For feature requests, describe:

1. **Use Case**: What problem does this solve?
2. **Proposed Solution**: How should it work?
3. **Alternatives**: What other approaches were considered?
4. **Implementation Details**: Technical considerations

## Review Process

### Pull Request Reviews

- All PRs require at least one review
- Reviewers will check:
  - Code quality and style
  - Test coverage
  - Documentation updates
  - Performance implications

### Continuous Integration

- All tests must pass
- Code coverage should not decrease
- Static analysis warnings should be addressed
- Build must succeed on all platforms

## Getting Help

- **GitHub Discussions**: For questions and general discussion
- **GitHub Issues**: For bug reports and feature requests
- **Documentation**: Check the project wiki for detailed guides

## Recognition

Contributors are recognized in:
- README.md contributors section
- Release notes
- Git commit history

Thank you for contributing to Network Stack! 🚀
