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

## Callback functions

Application developer needs to prepare following callback functions.

### Task callbacks

`tio_handler_t` needs executing task asynchronously since it keeps waiting for commands sent to MQTT topic.

`tio` needs abstraction with callbacks since the way of dispatch async tasks varies in different environments.

#### Task creation

Task create callback function signature is following.

```c
typedef kii_task_code_t
(*KII_TASK_CREATE)
    (const char* name,
     KII_TASK_ENTRY entry,
     void* param);
```

Typical implemetation with `pthread`:

```c
kii_task_code_t task_create_cb
    (const char* name,
     KII_TASK_ENTRY entry,
     void* param)
{
    pthread_t pthid;
    int ret = pthread_create(&pthid, NULL, entry, param);
    if(ret == 0)
    {
        return KII_TASKC_OK;
    }
    else
    {
        return KII_TASKC_FAIL;
    }
}
```

#### Delay callback

In some situation progam needs to wait for speicified period of time to avoid the loop runs too fast or making too many requests to servers, etc.

`tio` needs abstraction with callbacks since the way varies in different environments.

Delay callback function signature is following.

```
typedef void
(*KII_DELAY_MS)
    (unsigned int msec);
```

Typical implemetation with `usleep` provided by libc:

```c
void delay_ms_cb(unsigned int msec)
{
    usleep(msec * 1000);
}
```

### Socket callbacks.

`tio` provides abstraction of socket related functions.

Socket callback functions signature:

```c
typedef khc_sock_code_t
    (*KHC_CB_SOCK_CONNECT)
    (void* sock_ctx, const char* host, unsigned int port);

typedef khc_sock_code_t
    (*KHC_CB_SOCK_SEND)
    (void* sock_ctx, const char* buffer, size_t length);

typedef khc_sock_code_t
    (*KHC_CB_SOCK_RECV)
    (void* sock_ctx, char* buffer, size_t length_to_read,
     size_t* out_actual_length);

typedef khc_sock_code_t
    (*KHC_CB_SOCK_CLOSE)(void* sock_ctx);
```

You can see implementation witn OpenSSL at [linux-sample](linux-sample/sock_cb_linux.c)

`sock_ctx` argument is arbitrary data context which application can use.
Application should pass the pointer of the data when calling
`tio_handler_set_cb_sock_connect_http()`,
`tio_handler_set_cb_sock_send_http()`,
`tio_handler_set_cb_sock_recv_http()`,
`tio_handler_set_cb_sock_close_http()`,
`tio_handler_set_cb_sock_connect_mqtt()`,
`tio_handler_set_cb_sock_send_mqtt()`,
`tio_handler_set_cb_sock_recv_mqtt()`,
`tio_handler_set_cb_sock_close_mqtt()`
 methods.

Note that, `tio_handler` uses MQTT(s) to receive remote controll commands and uses HTTP(s) to send the result of command execution.

Application can choose to pass different implementations of socket functions or same one.
If you choose to pass same pointer of the function, Please also check [Thread safety](#thread-safety) section as well.

For both MQTT and HTTP, using them over secure connection is highly recommended.
Our cloud supports non-secure connection for now. However, we may terminate supports of unsecure connections in the future. 

## Set-up `tio_handler_t` instance

Here's the extracte set-up code from example app. 
The full code can be checked [handler_init() in example.c](linux-sample/example.c)

```c
    tio_handler_init(handler);

    tio_handler_set_app(handler, KII_APP_ID, KII_APP_HOST);

    tio_handler_set_cb_task_create(handler, task_create_cb_impl);
    tio_handler_set_cb_delay_ms(handler, delay_ms_cb_impl);

    tio_handler_set_cb_sock_connect_http(handler, sock_cb_connect, http_ssl_ctx);
    tio_handler_set_cb_sock_send_http(handler, sock_cb_send, http_ssl_ctx);
    tio_handler_set_cb_sock_recv_http(handler, sock_cb_recv, http_ssl_ctx);
    tio_handler_set_cb_sock_close_http(handler, sock_cb_close, http_ssl_ctx);

    tio_handler_set_cb_sock_connect_mqtt(handler, sock_cb_connect, mqtt_ssl_ctx);
    tio_handler_set_cb_sock_send_mqtt(handler, sock_cb_send, mqtt_ssl_ctx);
    tio_handler_set_cb_sock_recv_mqtt(handler, sock_cb_recv, mqtt_ssl_ctx);
    tio_handler_set_cb_sock_close_mqtt(handler, sock_cb_close, mqtt_ssl_ctx);

    tio_handler_set_http_buff(handler, http_buffer, http_buffer_size);
    tio_handler_set_mqtt_buff(handler, mqtt_buffer, mqtt_buffer_size);

    tio_handler_set_keep_alive_interval(handler, COMMAND_HANDLER_MQTT_KEEP_ALIVE_INTERVAL);

    tio_handler_set_json_parser_resource(handler, resource);
```

Here's anatomy of set-up calls.

### `tio_handler_init` call

This function must be called prior to any other functions of `tio_handler_t`.

### `tio_handler_set_app` call

Set the Kii application information.

In the example, `KII_APP_ID` and `KII_APP_HOST` is defined as Macro.
Those are used to identify work space in the cloud and the value is determined when you Kii Cloud App has been created.
To create your Kii Cloud App, Sign-up to [Developer console](https://developer.kii.com).

### Set-up callbacks

#### Task callbacks.

Set callback function pointers.

```c
    tio_handler_set_cb_task_create(handler, task_create_cb_impl);
    tio_handler_set_cb_delay_ms(handler, delay_ms_cb_impl);
```

### Socket callbacks.

Set callback function pointers and context data pointers.
If you application allocates memory/ resources for context data, application is responsible to free those memory/ resources.

Different context objects named `http_ssl_ctx` and `mqtt_ssl_ctx` is used since the connection and it's life-cycle is different between them.

```c
    tio_handler_set_cb_sock_connect_http(handler, sock_cb_connect, http_ssl_ctx);
    tio_handler_set_cb_sock_send_http(handler, sock_cb_send, http_ssl_ctx);
    tio_handler_set_cb_sock_recv_http(handler, sock_cb_recv, http_ssl_ctx);
    tio_handler_set_cb_sock_close_http(handler, sock_cb_close, http_ssl_ctx);
```

```c
    tio_handler_set_cb_sock_connect_mqtt(handler, sock_cb_connect, mqtt_ssl_ctx);
    tio_handler_set_cb_sock_send_mqtt(handler, sock_cb_send, mqtt_ssl_ctx);
    tio_handler_set_cb_sock_recv_mqtt(handler, sock_cb_recv, mqtt_ssl_ctx);
    tio_handler_set_cb_sock_close_mqtt(handler, sock_cb_close, mqtt_ssl_ctx);
```

### Set-up buffers

`tio_handler` needs memory buffer to store HTTP/ MQTT payloads.

In this example, assigned 4Kb for HTTP payloads and 2Kb for MQTT payloads.
This size may covers most use-cases.
However, If you're remote command definition is more and larger, you may need to allocate larger size.

Note that buffer for HTTP and MQTT must be isolated. Passing overlapping memory causes undefined behavior.

```c
    tio_handler_set_http_buff(handler, http_buffer, http_buffer_size);
    tio_handler_set_mqtt_buff(handler, mqtt_buffer, mqtt_buffer_size);
```

### Set-up MQTT Keep Alive interval.

In the example, `COMMAND_HANDLER_MQTT_KEEP_ALIVE_INTERVAL` is defined as Macro and value is 300 (in seconds).

MQTT have mechanism called `Keep Alive` detecting stale connection between the MQTT broker.

`tio_handler_t` acts as MQTT clients and send `PingReq` to MQTT broaker periodically with the specified interval. 
If `PingResp` from MQTT broaker is not present, `tio_handler_t` would close the current connection and make fresh connection again.

If interval is set to 0, `Keep Alive` is turned off and no `PingReq` message is send to MQTT broaker.

We highly recommend setting Keep Alive interval greater than 0 to detect disconnection.
Recommended interval is few minutes since too small interval may cause network congestion and increases cloud cost.

```c
    tio_handler_set_keep_alive_interval(handler, COMMAND_HANDLER_MQTT_KEEP_ALIVE_INTERVAL);
```

### Set-up json parser resource

`tio_handler_t` uses `jkii` json parser library.
`jkii` uses array of tokens to parse json string.

In this example, allocates 256 tokens statically.

```c
    tio_handler_set_json_parser_resource(handler, resource);
```

Number of tokens to be used to parse json varies depending on how complex the target json string is.
If you defined complex(i.e, lot of fields or long arrays in the commands) controll command, you would need to give larger number.
Alternatively, you can use dynamic allocation for tokens by using followig API:

```c
void tio_handler_set_json_parser_resource_cb(
    tio_handler_t* handler,
    JKII_RESOURCE_ALLOC_CB alloc_cb,
    JKII_RESOURCE_FREE_CB free_cb);
```

`alloc_cb` is called when the token is required and it's number is exactly same as numbers need to parse json string.

`free_cb` is called when the parse has been done.

## Start module

Now, it's ready to start `tio_handler_t` module.



# Use `tio_updater_t`

TODO: write

## Callback functions

## Set-up

## Start module

# Thread safety

