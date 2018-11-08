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

## Set-up

## Start module

# Use `tio_updater_t`

TODO: write

## Callback functions

## Set-up

## Start module

