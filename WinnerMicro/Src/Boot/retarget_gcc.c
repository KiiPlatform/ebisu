#include <stdio.h>
#include <stdlib.h>
#include <reent.h>

#define HR_UART0_INT_MASK           (0x0F000800 + 0x14)
#define HR_UART0_FIFO_STATUS        (0x0F000800 + 0x1C)
#define HR_UART0_TX_WIN             (0x0F000800 + 0x20)
typedef volatile unsigned int TLS_REG; 

static __inline void wm_reg_write32(unsigned int reg, unsigned int val)
{
    *(TLS_REG *)reg = val;
}

static __inline unsigned int wm_reg_read32(unsigned int reg)
{
    unsigned int val = *(TLS_REG *)reg;
    return val;
}

int sendchar(int ch)
{
	wm_reg_write32(HR_UART0_INT_MASK, 0x3);
    if(ch == '\n')  
	{
		while (wm_reg_read32(HR_UART0_FIFO_STATUS) & 0x3F);
		wm_reg_write32(HR_UART0_TX_WIN, '\r');
    }
    while(wm_reg_read32(HR_UART0_FIFO_STATUS) & 0x3F);
    wm_reg_write32(HR_UART0_TX_WIN, (char)ch);
    wm_reg_write32(HR_UART0_INT_MASK, 0x0);
    return ch;
}

_ssize_t _write_r (struct _reent *r, int file, const void *ptr, size_t len)
{
	int i;
	const unsigned char *p;
	
	p = (const unsigned char*) ptr;
	
	for (i = 0; i < len; i++) 
	{
		if (*p == '\n') 
		{
			sendchar('\r');
		}
		sendchar(*p++);
	}
	return len;
}
_ssize_t _read_r(struct _reent *r, int file, void *ptr, size_t len)
{
	return 0;
}

int _close_r(struct _reent *r, int file)
{
	return 0;
}

_off_t _lseek_r(struct _reent *r, int file, _off_t ptr, int dir)
{
	return (_off_t)0;	/*  Always indicate we are at file beginning.  */
}

int _fstat_r(struct _reent *r, int file, struct stat *st)
{
	return 0;
}

int _isatty(int file)
{
	return 1;
}

void abort(void)
{
  while(1);
}








extern char end[];
extern char _stack[];
static char *heap_ptr = end;
static char *heap_end = _stack;

void * _sbrk_r(struct _reent *_s_r, ptrdiff_t nbytes)
{
	char *base;

	base = heap_ptr;
	if(base + nbytes > heap_end)
    {
    	printf("kevin debug heap err = %x, %x\r\n", (int)heap_ptr, (int)nbytes);
		return (void *)-1;
    }
	heap_ptr += nbytes;	
	return base;
}

void * tls_reserve_mem_lock(int nbytes)
{
	if(heap_end - (nbytes + 4) <= heap_ptr)
	{
		return NULL;
	}
	heap_end  -= (nbytes + 4);
	return (void *)((((int)heap_end + 3) >> 2) << 2);
}

void tls_reserve_mem_unlock(void)
{
	heap_end = _stack;
}

void print_heap_status(void)
{
	printf("kevin debug heap %d KB\r\n", (int)(heap_end - heap_ptr)/1024);
}
