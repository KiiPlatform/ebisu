Kii Thing SDK Embedded.
==============

Kii Thing SDK for limited resource environments.

 - Written in pure C. (c89)
 - No dynamic memory allocation.

## Macros

Kii Thing SDK Embedded use 2 macros.

  - KII_PUSH_KEEP_ALIVE_INTERVAL_SECONDS
  - KII_JSON_FIXED_TOKEN_NUM

 These macros should be defined by applications programmers.

KII_PUSH_KEEP_ALIVE_INTERVAL_SECONDS requires seconds of ping request
interval. If the interval is too short, MQTT client often disconnect
connection to a MQTT server. We recommend 60 seconds or upper.

If KII_JSON_FIXED_TOKEN_NUM macro is defined, KII JSON library takes
resources by myself on stack memory. In KII_JSON_FIXED_TOKEN_NUM case,
token size of Kii JSON library is number defined by
KII_JSON_FIXED_TOKEN_NUM. If your environment has small stack size,
you should use this field and manage this resources by yourself.
