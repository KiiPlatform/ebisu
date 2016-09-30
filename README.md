# Kii Thing SDK Embedded.

Kii Thing SDK for limited resource environments.

 - Written in pure C. (c89)
 - No dynamic memory allocation.

## Guides

Please refer to the
[document](http://docs.kii.com/en/guides/thingifsdk/thing/).

## Samples

- [Intel Edison] (./INTEL/EDISON)
- [Ti CC3200] (./TI/CC3200)
- [Linux] (./Linux)

## Create tasks

This SDK requests creating tasks by KII\_TASK\_CREATE callback.  

When you need to set stack size yourself, please check task's stack size.

### Stack size on Mac (El Caption).

This is hint.

- KII\_TASK\_NAME\_RECV\_MSG: 4184 bytes

- KII\_TASK\_NAME\_PING\_REQ: 80 bytes

Attention: KII\_TASK\_NAME\_PING\_REQ task is requested only when KII\_PUSH\_KEEP\_ALIVE\_INTERVAL\_SECONDS is defined.

