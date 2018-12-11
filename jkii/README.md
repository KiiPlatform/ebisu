# jkii

jkii is JSON parser library written in c (conforms to c99).
It allows to specify multiple fields by jsonpath notation to obtain its values.
This design is inspired by [microjson](http://www.catb.org/esr/microjson/).

jkii is designed to fit resource limited environment.
You can choose static or dynamic memory allocation.

## Build jkii

Execute following command on this directory.

```sh
mkdir -p build
cd build
cmake ..
make
```


### Debug Build
```sh
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make
```

## Example

If you want to extract "hoo" value from following JSON string:

```json
{
    "hoo" : "bar"
}
```

We can extract "hoo" value by following code:

```c
const char json_string[] = "{\"hoo\":\"bar\"}"; // target json string.

jkii_field_t fields[2];
memset(fields, 0x00, sizeof(fields));
char buf[16];

fields[0].path = "/hoo";
fields[0].type = JKII_FIELD_TYPE_STRING;
fields[0].field_copy.string = buf;
fields[0].field_copy_buff_size = sizeof(buf) / sizeof(buf[0]);
fields[1].path = NULL;

jkii_token_t tokens[8];
jkii_resource_t resource = {tokens, 8};

jkii_parse_err_t result = jkii_parse(
        json_string,
        sizeof(json_string) / sizeof(json_string[0]),
        &fields,
        &resource);

if (result == JKII_ERR_OK) {
    // success to extract "hoo" value.
    // you can find "hoo" value in buf array.
}
```

## Dependency.

jkii uses [jsmn](http://zserge.com/jsmn.html).
It is statically linked by default.

jsmn is published under [MIT
License](http://opensource.org/licenses/mit-license.php).

