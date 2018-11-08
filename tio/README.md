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
If you choose to pass same pointer of the function, Please see the also check [Thread safety](#thread-safety) section as well.

For both MQTT and HTTP, using them over secure connection is highly recommended.
Our cloud supports non-secure connection for now. However, we may terminate supports of unsecure connections in the future. 

## Set-up

## Start module

# Use `tio_updater_t`

TODO: write

## Callback functions

## Set-up

## Start module

# Thread safety

