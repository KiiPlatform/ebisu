#ifndef __test_env__
#define __test_env__

// stringify the macro value
// ref. https://gcc.gnu.org/onlinedocs/gcc-4.8.5/cpp/Stringification.html
#define XSTR(x) STR(x)
#define STR(x) #x

#ifndef _DEFAULT_SITE
const char DEFAULT_SITE[] = "api-jp.kii.com";
#else
const char DEFAULT_SITE[] = XSTR(_DEFAULT_SITE);
#endif
#ifndef _APP_ID
#error APP_ID needs to be defined
const char APP_ID[] = "UNDEFINED";
#else
const char APP_ID[] = XSTR(_APP_ID);
#endif



#endif // __test_env__
