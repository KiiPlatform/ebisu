#include <string.h>
#include <stdarg.h>
#include "kii_core_impl.h"

#include <LTask.h>

#if 0
void logger_cb(const char* format, ...)
{
    va_list list;
    va_start(list, format);
    vprintf(format, list);
    va_end(list);
}
#endif
static void kiiHal_itoa(char **buf, int i, int base)
{
	char *s;
#define LEN	20
	int rem;
	static char rev[LEN+1];

	rev[LEN] = 0;
	if (i == 0)
		{
		(*buf)[0] = '0';
		++(*buf);
		return;
		}
	s = &rev[LEN];
	while (i)
		{
		rem = i % base;
		if (rem < 10)
			*--s = rem + '0';
		else if (base == 16)
			*--s = "abcdef"[rem - 10];
		i /= base;
		}
	while (*s)
		{
		(*buf)[0] = *s++;
		++(*buf);
		}
}

void logger_cb(const char *fmt,...)
{
  char print_buf[512*2];
   va_list ap;
   double dval;
   int ival;
   char *p, *sval;
   char *bp, cval;
   int fract;

	bp= print_buf;
	*bp= 0;
	
	va_start (ap, fmt);
	for (p= (char*)fmt; *p; p++)
	{
		if (*p != '%')
		{
			*bp++= *p;
			continue;
		}
		switch (*++p) {
		case 'd':
			ival= va_arg(ap, int);
			if (ival < 0){
				*bp++= '-';
			     ival= -ival;
			}
			kiiHal_itoa (&bp, ival, 10);
			break;
			
        	case 'o':
			ival= va_arg(ap, int);
			if (ival < 0){
				*bp++= '-';
			     ival= -ival;
			}
			*bp++= '0';
			kiiHal_itoa (&bp, ival, 8);
			break;
			
		case 'x':
			ival= va_arg(ap, int);
			if (ival < 0){
			     *bp++= '-';
			     ival= -ival;
			}
			*bp++= '0';
			*bp++= 'x';
			kiiHal_itoa (&bp, ival, 16);
			break;
			
		case 'c':
			cval= va_arg(ap, int);
			*bp++= cval;
			break;
			
		case 's':
			for (sval = va_arg(ap, char *) ; *sval ; sval++ )
			    *bp++= *sval;
			break;
		}
	}
	*bp= 0;
	va_end (ap);
	Serial.println(print_buf);
}

/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
