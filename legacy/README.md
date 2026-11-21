# Legacy samples

Two samples from ebisu's first versions, kept for reference. **Don't start a
product from them.**

- **They don't protect the connection.** The cc3200 sample talks to Kii Cloud
  and to the MQTT broker over plain HTTP and MQTT. The gt202 sample does too by
  default, and its TLS mode accepts any server's certificate. Tokens and the
  thing's password cross the network readable, and nothing says the server is
  Kii's.
- **Nothing builds or runs them.** They need their vendors' SDKs, which can't
  be downloaded without an account. CI compiles only ebisu's own sources, as
  each sample's Makefile copies them, for a Cortex-M4. Whether the samples'
  own code still builds against this version's API isn't checked.
- **Their system, sockets and TLS are callbacks of their own,** written before
  ebisu had ports.

For a new device, start from one of the
[samples under tio](https://github.com/KiiPlatform/ebisu/blob/master/tio/README.md#samples),
which use platform's ports and verify the server: the freertos sample for a
microcontroller.

| Sample | Target | Built with |
|---|---|---|
| [cc3200](https://github.com/KiiPlatform/ebisu/blob/master/legacy/cc3200-sample/README.md) | TI's CC3200, a Cortex-M4 | the CC3200 SDK and Code Composer Studio |
| [gt202](https://github.com/KiiPlatform/ebisu/blob/master/legacy/gt202-sample/README.md) | the GT202 Wi-Fi module, on a FRDM-K22F board, a Cortex-M4 | MQX 4.1 and IAR Embedded Workbench for ARM |
