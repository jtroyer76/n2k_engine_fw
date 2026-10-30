# n2k_engine_fw

Engine temperature monitoring on NMEA 2000 for the [Hat Labs HALMET](https://docs.hatlabs.fi/halmet/). DS18B20 1-Wire probes on the coolant outlet, the wet exhaust hose and the engine room air; readings go out on the NMEA 2000 bus for chartplotters and Signal K. WiFi is used only for setup, updates and a debug view.

Status: specification only. See [docs/SPEC.md](docs/SPEC.md).

Based on the [HALMET example firmware](https://github.com/hatlabs/HALMET-example-firmware) by Hat Labs (Apache-2.0).

## License

Apache-2.0. See [LICENSE](LICENSE).
