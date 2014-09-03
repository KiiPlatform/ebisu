#ifndef __WM_TYPE_DEF_H__
#define __WM_TYPE_DEF_H__

#ifdef BOOLEAN
#undef BOOLEAN
#endif
typedef unsigned char BOOLEAN;

#ifdef INT8U
#undef INT8U
#endif
typedef unsigned char INT8U;

#ifdef INT8S
#undef INT8S
#endif
typedef signed char INT8S;

#ifdef INT16U
#undef INT16U
#endif
typedef unsigned short INT16U; 

#ifdef INT16S
#undef INT16S
#endif
typedef signed short INT16S;

#ifdef INT32U
#undef INT32U
#endif
typedef unsigned int INT32U;

#ifdef INT32S
#undef INT32S
#endif
typedef signed int INT32S;

#ifdef FP32
#undef FP32
#endif
typedef float FP32;

#ifdef FP64
#undef FP64
#endif
typedef double FP64;

#ifdef bool
#undef bool
#endif
typedef unsigned char bool;

#ifdef u8
#undef u8
#endif
typedef unsigned char u8;

#ifdef s8
#undef s8
#endif
typedef signed char s8;

#ifdef u16
#undef u16
#endif
typedef unsigned short u16;

#ifdef s16
#undef s16
#endif
typedef signed short s16;

#ifdef u32
#undef u32
#endif
typedef unsigned int u32;

#ifdef s32
#undef s32
#endif
typedef signed int s32;

#ifdef u64
#undef u64
#endif
typedef unsigned long long u64;

#ifdef u8_t
#undef u8_t
#endif
typedef unsigned char u8_t;

#ifdef u16_t
#undef u16_t
#endif
typedef unsigned short u16_t;

#ifdef u32_t
#undef u32_t
#endif
typedef unsigned int u32_t;

#ifdef s8_t
#undef s8_t
#endif
typedef signed char s8_t;

#ifdef s16_t
#undef s16_t
#endif
typedef signed short s16_t;

#ifdef s32_t
#undef s32_t
#endif
typedef signed int s32_t;

#ifdef size_t
#undef size_t
typedef unsigned int size_t;
#endif

#ifdef err_t
#undef err_t
#endif
typedef s8_t err_t;

#ifdef mem_ptr_t
#undef mem_ptr_t
#endif
typedef u32_t mem_ptr_t;

#ifdef socklen_t
#undef socklen_t
#endif
typedef u32_t socklen_t;

#ifdef UINT32
#undef UINT32
#endif
typedef unsigned int UINT32;

#ifdef INT32
#undef INT32
#endif
typedef int INT32;

#ifdef u_long
#undef u_long
#endif
typedef unsigned long u_long;

#ifdef CHAR
#undef CHAR
#endif
typedef char CHAR;

#ifdef UINT16
#undef UINT16
#endif
typedef unsigned short UINT16;

#ifdef BOOL
#undef BOOL
#endif
typedef int BOOL;

#ifdef ULONG
#undef ULONG
#endif
typedef unsigned long ULONG;

#ifdef OS_STK
#undef OS_STK
#endif
typedef unsigned int OS_STK;

#ifdef OS_CPU_SR
#undef OS_CPU_SR
#endif
typedef unsigned int OS_CPU_SR;

#define TRUE				1
#define FALSE				0

#define true				1
#define false				0

#define WM_SUCCESS			0
#define WM_FAILED			-1


#endif
