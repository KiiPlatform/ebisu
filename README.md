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

This SDK requests creating two tasks by KII\_TASK\_CREATE callback.  

List of task name symbols.

- KII\_TASK\_NAME\_RECV\_MSG

  The task of receiving push messages.

- KII\_TASK\_NAME\_PING\_REQ

  The task of sending ping request.

You'll specify stack size, depth and priority, etc depending on the environment.
Please check samples for reference.
For example, [Ti CC3200 kii\_task\_impl.c](./TI/CC3200/wlan\_station/kii/kii\_task\_impl.c).

