## Building and executing Test

The tests are configured when the application they run against is known, from
the environment or from `-DEBISU_APP_ID=` and `-DEBISU_DEFAULT_SITE=`. Execute
following commands on the top directory of the repository:

```sh
APP_ID=... DEFAULT_SITE=... cmake --preset debug
cmake --build --preset debug
ctest --preset large
```

- APP_ID : specify the app-id used for the test.

- DEFAULT_SITE : specify the API hostname used for the test.

Options, given to `cmake --preset debug`:

- `-DEBISU_TEST_JUNIT=ON` : generate a JUnit test report under `build/debug/test-results`.

- `-DEBISU_PLAIN_HTTP=ON` : use raw HTTP instead of HTTPS, useful for packet capturing.

- `-DEBISU_SOCKET_LOG=ON` : output send/recv buffer to stdout. (this will break your console when send/recv binary data.) The output holds tokens and passwords: never enable it where it is kept, as in CI, whose logs are public.

To run them under valgrind, use `ctest --preset large --test-dir build/debug -T memcheck`.

## tio suite

`large_test_tio` runs tio's handler and updater end to end, with a user owning
the thing. It sets the application up itself as its admin, so it needs
`CLIENT_ID` and `CLIENT_SECRET` in the environment when it runs, as CI exports
them; without them it is skipped, except in CI, where it fails. The user and
the thing each run creates are deleted at its end, and the admin and user
tokens are requested to expire within minutes.

## using non-default app

To run this test against the different app-id, you should preapare the followings,

- create a app scope topic `test_topic`.

- upload and enable the following server code.

```
function echo(params, context) {
  return params.message;
}
```

You can do them by
```
DEFAULT_SITE="${DEFAULT_SITE}" \
APP_ID="${APP_ID}" \
CLIENT_ID="${CLIENT_ID}" \
CLIENT_SECRET="${CLIENT_SECRET}" \
tests/large_test/initapp.sh
```

Also you need to ensure that app has server-code enabled.