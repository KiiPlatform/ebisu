#include "ebisu_dns.h"

#include <stdio.h>
#include <string.h>

static bool is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static char to_lower(char c)
{
    return c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c;
}

/* The value of the digit c, as a hexadecimal one, or -1. */
static int digit_value(char c)
{
    if (is_digit(c)) {
        return c - '0';
    }
    c = to_lower(c);
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    return -1;
}

/* s, of length bytes, as an IPv4 address in a form inet_aton() takes, without
   anything after it, in out, in network order. Whether it is one. */
static bool parse_ipv4(const char* s, size_t length, unsigned char out[4])
{
    uint32_t parts[4];
    size_t count = 0;
    size_t i = 0;
    uint32_t value;
    /* Up to 4 numbers, each decimal, octal after a 0 or hexadecimal after 0x,
       between dots. */
    for (;;) {
        uint32_t base = 10;
        bool digits = false;
        if (i == length || !is_digit(s[i])) {
            return false;
        }
        value = 0;
        if (s[i] == '0') {
            ++i;
            if (i < length && (s[i] == 'x' || s[i] == 'X')) {
                base = 16;
                ++i;
            } else {
                base = 8;
                digits = true;
            }
        }
        for (; i < length; ++i) {
            int digit = digit_value(s[i]);
            if (digit < 0 || (uint32_t)digit >= base) {
                break;
            }
            if (value > (UINT32_MAX - (uint32_t)digit) / base) {
                return false;
            }
            value = value * base + (uint32_t)digit;
            digits = true;
        }
        if (!digits) {
            return false;
        }
        parts[count++] = value;
        if (i < length && s[i] == '.' && count < 4) {
            ++i;
        } else {
            break;
        }
    }
    if (i != length) {
        return false;
    }
    /* The last number fills the bytes the others leave. */
    for (i = 0; i + 1 < count; ++i) {
        if (parts[i] > 0xff) {
            return false;
        }
    }
    if (count < 4 && parts[count - 1] > (UINT32_MAX >> (8 * (count - 1)))) {
        return false;
    }
    if (count == 4 && parts[3] > 0xff) {
        return false;
    }
    value = parts[count - 1];
    for (i = 0; i + 1 < count; ++i) {
        value |= parts[i] << (8 * (3 - i));
    }
    out[0] = (unsigned char)(value >> 24);
    out[1] = (unsigned char)((value >> 16) & 0xff);
    out[2] = (unsigned char)((value >> 8) & 0xff);
    out[3] = (unsigned char)(value & 0xff);
    return true;
}

/* s, of length bytes, as the dotted decimal IPv4 an IPv6 address ends with, as
   inet_pton() takes it: 4 numbers of at most 3 digits, none after a leading
   0, each at most 255. Whether it is one. */
static bool parse_dotted_quad(const char* s, size_t length, unsigned char out[4])
{
    size_t i = 0;
    size_t part;
    for (part = 0; part < 4; ++part) {
        unsigned int value = 0;
        size_t digits = 0;
        if (part > 0) {
            if (i == length || s[i] != '.') {
                return false;
            }
            ++i;
        }
        while (i < length && is_digit(s[i])) {
            if (digits > 0 && value == 0) {
                return false;
            }
            value = value * 10 + (unsigned int)(s[i] - '0');
            if (++digits > 3 || value > 255) {
                return false;
            }
            ++i;
        }
        if (digits == 0) {
            return false;
        }
        out[part] = (unsigned char)value;
    }
    return i == length;
}

/* s, of length bytes, as an IPv6 address, as inet_pton() takes it: 8 groups of
   1 to 4 hexadecimal digits, a "::" for one or more groups of 0 once at most,
   and the last 32 bits in dotted decimal or not. Whether it is one. */
static bool parse_ipv6(const char* s, size_t length, unsigned char out[16])
{
    unsigned char bytes[16];
    size_t count = 0;
    bool has_gap = false;
    size_t gap = 0;
    size_t i = 0;
    if (length >= 2 && s[0] == ':' && s[1] == ':') {
        has_gap = true;
        i = 2;
    } else if (length == 0 || s[0] == ':') {
        return false;
    }
    while (i < length) {
        size_t start = i;
        unsigned int value = 0;
        size_t digits = 0;
        while (i < length && digit_value(s[i]) >= 0) {
            value = value * 16 + (unsigned int)digit_value(s[i]);
            if (++digits > 4) {
                return false;
            }
            ++i;
        }
        if (i < length && s[i] == '.') {
            /* The last 32 bits, which take 2 groups. */
            if (count > 12 || !parse_dotted_quad(s + start, length - start, bytes + count)) {
                return false;
            }
            count += 4;
            break;
        }
        if (digits == 0 || count == 16) {
            return false;
        }
        bytes[count++] = (unsigned char)(value >> 8);
        bytes[count++] = (unsigned char)(value & 0xff);
        if (i == length) {
            break;
        }
        if (s[i] != ':' || ++i == length) {
            return false;
        }
        if (s[i] == ':') {
            if (has_gap) {
                return false;
            }
            has_gap = true;
            gap = count;
            ++i;
        }
    }
    if (!has_gap) {
        if (count != 16) {
            return false;
        }
        memcpy(out, bytes, 16);
        return true;
    }
    /* "::" stands for one group at least. */
    if (count > 14) {
        return false;
    }
    memset(out, 0, 16);
    memcpy(out, bytes, gap);
    memcpy(out + 16 - (count - gap), bytes + gap, count - gap);
    return true;
}

/* Whether the IPv6 address bytes is link-local, unicast or multicast: the
   addresses whose scope both C libraries take an interface's name for. */
static bool is_link_local(const unsigned char bytes[16])
{
    return (bytes[0] == 0xfe && (bytes[1] & 0xc0) == 0x80) ||
        (bytes[0] == 0xff && (bytes[1] & 0x0f) == 0x02);
}

/* The scope s, of length bytes without a NUL, of the IPv6 address in out, in
   its scope_id. Whether it is one: an interface's number, in decimal digits
   only, or, for a link-local address, an interface's name, as dns has it. */
static bool parse_scope(ebisu_dns_t* dns, const char* s, size_t length, ebisu_dns_addr_t* out)
{
    char name[64];
    uint32_t value = 0;
    size_t i;
    if (length == 0) {
        return false;
    }
    for (i = 0; i < length && is_digit(s[i]); ++i) {
        uint32_t digit = (uint32_t)(s[i] - '0');
        if (value > (UINT32_MAX - digit) / 10) {
            return false;
        }
        value = value * 10 + digit;
    }
    if (i == length) {
        out->scope_id = value;
        return true;
    }
    if (!is_link_local(out->bytes) || length >= sizeof(name) || dns == NULL ||
            dns->ops->scope_of == NULL) {
        return false;
    }
    memcpy(name, s, length);
    name[length] = '\0';
    out->scope_id = dns->ops->scope_of(dns, name);
    return out->scope_id != 0;
}

bool ebisu_dns_parse_address(ebisu_dns_t* dns, const char* s, size_t length, uint16_t port,
        bool keep_bad_scope, ebisu_dns_addr_t* out)
{
    const char* scope;
    size_t address_length;
    memset(out, 0, sizeof(*out));
    out->port = port;
    if (s == NULL || memchr(s, '\0', length) != NULL) {
        return false;
    }
    if (parse_ipv4(s, length, out->bytes)) {
        out->family = EBISU_DNS_IPV4;
        return true;
    }
    scope = (const char*)memchr(s, '%', length);
    address_length = scope != NULL ? (size_t)(scope - s) : length;
    if (!parse_ipv6(s, address_length, out->bytes)) {
        return false;
    }
    out->family = EBISU_DNS_IPV6;
    /* Left out, its scope_id is still 0. */
    if (scope != NULL && !parse_scope(dns, scope + 1, length - address_length - 1, out)) {
        return keep_bad_scope;
    }
    return true;
}

/* bytes as RFC 5952 writes an IPv6 address, in out, of size bytes at least 46. */
static void format_ipv6(const unsigned char bytes[16], char* out, size_t size)
{
    static const unsigned char mapped[12] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff };
    unsigned int groups[8];
    size_t best = 8, best_length = 0;
    size_t run = 8, run_length = 0;
    size_t used = 0;
    size_t i;
    if (memcmp(bytes, mapped, sizeof(mapped)) == 0) {
        snprintf(out, size, "::ffff:%u.%u.%u.%u", bytes[12], bytes[13], bytes[14], bytes[15]);
        return;
    }
    for (i = 0; i < 8; ++i) {
        groups[i] = (unsigned int)bytes[2 * i] << 8 | bytes[2 * i + 1];
        if (groups[i] == 0) {
            if (run_length == 0) {
                run = i;
            }
            if (++run_length > best_length) {
                best = run;
                best_length = run_length;
            }
        } else {
            run_length = 0;
        }
    }
    /* A single group of 0 is written as one. */
    if (best_length < 2) {
        best = 8;
    }
    out[0] = '\0';
    for (i = 0; i < 8 && used < size; ++i) {
        int n;
        if (i == best) {
            n = snprintf(out + used, size - used, "::");
            i += best_length - 1;
        } else {
            n = snprintf(out + used, size - used, "%s%x",
                    i > 0 && i != best + best_length ? ":" : "", groups[i]);
        }
        if (n < 0) {
            return;
        }
        used += (size_t)n;
    }
}

void ebisu_dns_format_address(ebisu_dns_t* dns, const ebisu_dns_addr_t* addr, char* out,
        size_t size)
{
    char text[46];
    char scope[1 + 64];
    if (size == 0) {
        return;
    }
    if (addr->family != EBISU_DNS_IPV6) {
        snprintf(out, size, "%u.%u.%u.%u:%u", addr->bytes[0], addr->bytes[1], addr->bytes[2],
                addr->bytes[3], (unsigned int)addr->port);
        return;
    }
    format_ipv6(addr->bytes, text, sizeof(text));
    scope[0] = '\0';
    if (addr->scope_id != 0) {
        char name[64];
        if (dns != NULL && dns->ops->scope_name != NULL &&
                dns->ops->scope_name(dns, addr->scope_id, name, sizeof(name))) {
            snprintf(scope, sizeof(scope), "%%%s", name);
        } else {
            snprintf(scope, sizeof(scope), "%%%lu", (unsigned long)addr->scope_id);
        }
    }
    snprintf(out, size, "[%s%s]:%u", text, scope, (unsigned int)addr->port);
}
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
