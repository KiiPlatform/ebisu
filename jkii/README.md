# jkii {#jkii_readme}

jkii is JSON parser library written in c (conforms to c99).
It allows to specify multiple fields by jsonpath notation to obtain its values.
This design is inspired by [microjson](http://www.catb.org/esr/microjson/).

jkii is designed to fit resource limited environment.
You can choose static or dynamic memory allocation.

It writes JSON too, with `jkii_writer.h`: see Writing JSON below.

## Build jkii

Execute following command on the top directory of the repository.

```sh
cmake --preset release
cmake --build --preset release --target jkii
```

The library is in `build/release/jkii`.

### Debug Build
```sh
cmake --preset debug
cmake --build --preset debug --target jkii
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

## Writing JSON

A `jkii_writer_t` writes JSON into a buffer, or out through a callback a buffer
at a time. It puts in the commas, colons and quotes itself, escapes strings as
`jkii_escape_str()` does, and refuses what wouldn't be JSON: a value where a key
is expected, the wrong end of a container, or NaN. It never truncates: a buffer
too small is `JKII_WRITE_ERR_TOO_LARGE`, and `jkii_writer_finish()` says how
much was needed. Each call returns the first error so far, so the result of
`jkii_writer_finish()` is the one to check.

```c
char body[64];
size_t len = 0;
jkii_writer_t w;
jkii_writer_init_buff(&w, body, sizeof(body));
jkii_write_object_start(&w);
jkii_write_key(&w, "power");
jkii_write_bool(&w, 1);
jkii_write_key(&w, "temperature");
jkii_write_double(&w, 21.5, 1);
jkii_write_object_end(&w);
if (jkii_writer_finish(&w, &len) == JKII_WRITE_OK) {
    // body holds {"power":true,"temperature":21.5}, terminated, and len is 33.
}
```

With a NULL buffer of size 0, nothing is written and `jkii_writer_finish()` gives
the length, to size a buffer. `jkii_writer_init_cb()` gives the output to a
callback whenever its buffer is full, so the JSON can be larger than any buffer,
as a request body or a file can.

It writes JSON Lines too, one value a line, as for a log or a batch of records:
`jkii_write_record_end()` after each value ends its line, and the next value
starts the next, with no array around them and no commas between. Through a
callback, the batch can go out as a request body, or be appended to a file,
without being held whole.

```c
for (size_t i = 0; i < count; ++i) {
    jkii_write_object_start(&w);
    jkii_write_key(&w, "t");
    jkii_write_uint(&w, samples[i].time);
    jkii_write_key(&w, "v");
    jkii_write_double(&w, samples[i].value, 2);
    jkii_write_object_end(&w);
    jkii_write_record_end(&w);
}
```

To append a record to a file of JSON Lines, write it with a writer of its own,
into a buffer, and append the buffer with one write, so that a write cut short,
as by a power loss, leaves at most that line cut short. If the file doesn't end
with `\n`, because its last write was cut short, write one first, so that the new
record starts a line of its own rather than running on from that one:

```c
char line[256];
size_t len = 0;
jkii_writer_t w;
jkii_writer_init_buff(&w, line, sizeof(line));
jkii_write_object_start(&w);
jkii_write_key(&w, "t");
jkii_write_uint(&w, now);
jkii_write_object_end(&w);
jkii_write_record_end(&w);
if (jkii_writer_finish(&w, &len) == JKII_WRITE_OK) {
    // Append len bytes of line to the file: a write() with O_APPEND, or
    // fwrite() to a FILE opened with "a".
}
```

Numbers are written without `printf`: integers of 64 bits, and doubles with a
fixed number of decimals, with `.` whatever the locale. Objects and arrays nest
up to `JKII_WRITER_MAX_DEPTH`, 16 unless defined otherwise for jkii and for
everything using it.

## Dependency.

jkii uses [jsmn](http://zserge.com/jsmn.html).
It is statically linked by default.

jsmn is published under [MIT
License](http://opensource.org/licenses/mit-license.php).

