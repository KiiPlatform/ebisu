# tio

With `tio` and Kii Interaction Framework, you can quickly implement IoT devices
which supports following functionalities.

- Read data from sensors and upload data to cloud periodically.
- Receive remote control command from cloud and process it in your IoT device application.

`tio` consists of following modules.

- `tio_handler_t`

Responsible for watching remote control command and propagate the command to
IoT device application.

Once the module has been started,
The loop inside the module keeps watching command arrival from the cloud.

- `tio_updater_t`

Responsible for upload data read from sensors equipped to IoT devices.

Once the module has been started,
The loop inside the module keeps asking for update by sensors periodically with the specified interval.

You can choose to run both `tio_handler_t` and `tio_updater_t` or either one.

# Use `tio_handler_t`

TODO: write

## Callback functions

## Set-up

## Start module

# Use `tio_updater_t`

TODO: write

## Callback functions

## Set-up

## Start module

