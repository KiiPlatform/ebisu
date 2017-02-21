#include <gtest/gtest.h>
#include <stdio.h>

#include "kii_cl_parser.h"

TEST(contentLengthParser, normal)
{
    const char* response =
"HTTP/1.1 200 OK\r\n"
"Accept-Ranges: bytes\r\n"
"Access-Control-Allow-Origin: *\r\n"
"Access-Control-Expose-Headers: Content-Type, Authorization, Content-Length, X-Requested-With, ETag, X-Step-Count\r\n"
"Age: 0\r\n"
"Cache-Control: max-age=0, no-cache, no-store\r\n"
"Content-Type: application/json;charset=UTF-8\r\n"
"Date: Fri, 25 Sep 2015 11:07:16 GMT\r\n"
"Server: nginx/1.2.3\r\n"
"Via: 1.1 varnish\r\n"
"X-HTTP-Status-Code: 200\r\n"
"X-Varnish: 726929556\r\n"
"Content-Length: 176\r\n"
"Connection: keep-alive\r\n"
"\r\nBody";
    long content_length = kii_parse_content_length((char*)response);
    ASSERT_EQ(176, content_length);
}

TEST(contentLengthParser, noContentLength)
{
    const char* response =
"HTTP/1.1 200 OK\r\n"
"Accept-Ranges: bytes\r\n"
"Access-Control-Allow-Origin: *\r\n"
"Access-Control-Expose-Headers: Content-Type, Authorization, Content-Length, X-Requested-With, ETag, X-Step-Count\r\n"
"Age: 0\r\n"
"Cache-Control: max-age=0, no-cache, no-store\r\n"
"Content-Type: application/json;charset=UTF-8\r\n"
"Date: Fri, 25 Sep 2015 11:07:16 GMT\r\n"
"Server: nginx/1.2.3\r\n"
"Via: 1.1 varnish\r\n"
"X-HTTP-Status-Code: 200\r\n"
"Connection: keep-alive\r\n"
"\r\nBody";
    long content_length = kii_parse_content_length((char*)response);
    ASSERT_EQ(0, content_length);
}

TEST(contentLengthParser, noBody)
{
    const char* response =
"HTTP/1.1 200 OK\r\n"
"Accept-Ranges: bytes\r\n"
"Access-Control-Allow-Origin: *\r\n"
"Access-Control-Expose-Headers: Content-Type, Authorization, Content-Length, X-Requested-With, ETag, X-Step-Count\r\n"
"Age: 0\r\n"
"Cache-Control: max-age=0, no-cache, no-store\r\n"
"Content-Type: application/json;charset=UTF-8\r\n"
"Date: Fri, 25 Sep 2015 11:07:16 GMT\r\n"
"Server: nginx/1.2.3\r\n"
"Via: 1.1 varnish\r\n"
"X-HTTP-Status-Code: 200\r\n"
"Connection: keep-alive\r\n"
"\r\n";
    long content_length = kii_parse_content_length(response);
    ASSERT_EQ(0, content_length);
}

TEST(contentLengthParser, max)
{
    const char* response =
"HTTP/1.1 200 OK\r\n"
"Accept-Ranges: bytes\r\n"
"Access-Control-Allow-Origin: *\r\n"
"Access-Control-Expose-Headers: Content-Type, Authorization, Content-Length, X-Requested-With, ETag, X-Step-Count\r\n"
"Age: 0\r\n"
"Cache-Control: max-age=0, no-cache, no-store\r\n"
"Content-Type: application/json;charset=UTF-8\r\n"
"Date: Fri, 25 Sep 2015 11:07:16 GMT\r\n"
"Server: nginx/1.2.3\r\n"
"Via: 1.1 varnish\r\n"
"X-HTTP-Status-Code: 200\r\n"
"X-Varnish: 726929556\r\n"
"Content-Length: 2147483647\r\n"
"Connection: keep-alive\r\n"
"\r\nBody";
    long content_length = kii_parse_content_length((char*)response);
    ASSERT_EQ(2147483647, content_length);
}

TEST(contentLengthParser, exceedMax)
{
    const char* response =
"HTTP/1.1 200 OK\r\n"
"Content-Length: 21474836470\r\n"
"\r\nBody";
    long content_length = kii_parse_content_length((char*)response);
    ASSERT_EQ(2147483647, content_length);
}

TEST(contentLengthParser, min)
{
    const char* response =
"HTTP/1.1 200 OK\r\n"
"Content-Length: 1\r\n"
"\r\nBody";
    long content_length = kii_parse_content_length((char*)response);
    ASSERT_EQ(1, content_length);
}

