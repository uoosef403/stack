# Network Stack

An experimental user-space network stack for TUN devices, written in C++20.
It includes TCP, UDP, and ICMP handlers, a SOCKS5 proxy client, and a TOML
configuration file. Network operations use Boost.Asio.

This is a development project, not a production-ready network stack. The
repository includes unit tests and integration test scripts under `tests/`.

## Build

The project uses CMake and vcpkg. If a vcpkg toolchain is not configured,
CMake attempts to download and bootstrap vcpkg. The build also needs a C++20
compiler and network access to fetch dependencies.

```sh
git clone https://github.com/uoosef403/stack.git
cd stack
cmake -S . -B build
cmake --build build
```

The build configuration uses CMake policies introduced after the declared
minimum version. Use CMake 3.30 or later.

## Configuration

See [`config.toml`](config.toml) for the TUN interface, SOCKS5, logging, TCP,
UDP, and ICMP settings. The program also accepts command-line options. Run
`stack --help` for the available options.

Creating and configuring a TUN device can require elevated permissions and
platform-specific setup. Review the configuration before running the program.

## License

[MIT](LICENSE)
