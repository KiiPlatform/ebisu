# ebisu - Embedded IoT Suite

`ebisu` provides access to `Kii IoT cloud` to the limited resource devices.

`ebisu` consists of following components.

## tio

You may mainly use this module when you implement your IoT devices connected to Kii.

The module provides following functionalities

- Upload state data read from sensors.
- Receive remote control command via MQTT and propagate it to the application on the devices.

For more details, please refer to [./tio/README.md](./tio/README.md)

## kii

While `tio` covers basic functionality required by IoT devices, `kii` provides

generic cloud data storage functionalities.

### Key-Value storage

Write/ read arbitrary key-value structured data to/ from Kii Cloud.

### File storage

Write/ read arbitrary binary data to/ from Kii Cloud.

For example, you can use these storage to store configuration data.

For more details, please refer to [./kii/README.md](./kii/README.md)

### khc

`khc` is a simple HTTP client library.

The module is developed to implement `tio` and `kii`.

In most cases, you don't have to use this module directly in you IoT device application.

For more details, please refer to [./kii/README.md](./kii/README.md)

## jkii

`jkii` is a simple json parser library.

The module is developed to implement `tio` and `kii`.

You can use this module to parse json encoded string or you can choose other 3rd party libraries.

For more details, please refer to [./jkii/README.md](./jkii/README.md)

## API references

Details of API are available in [API references](https://kiiplatform.github.io/ebisu-doc).
