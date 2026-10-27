#include "kii.h"
#include "kii_impl.h"
#include "ebisu_version.h"
#include "kii_session_impl.h"
#include <string.h>

const char _APP_KEY_HEADER[] = "X-Kii-Appkey: k";
const char _CONTENT_LENGTH_ZERO[] = "Content-Length: 0";

size_t _cb_write_buff(char *buffer, size_t size, void *userdata)
{
    kii_t *kii = (kii_t *)userdata;
    if (kii->_rw_buff_written == 0) {
        // A response refusing the session's token has the request sent again,
        // so it may have to be read past the request's body.
        kii->_resp_offset = _kii_session_resp_offset(kii);
        memset(kii->_rw_buff + kii->_resp_offset, '\0',
                kii->_rw_buff_size - kii->_resp_offset);
    }
    if (kii->_rw_buff_size - kii->_resp_offset - kii->_rw_buff_written < size) {
        // No room after the request's body, which is given up.
        _kii_resp_to_start(kii);
    }
    size_t remain = kii->_rw_buff_size - kii->_resp_offset - kii->_rw_buff_written;
    size_t req_size = size;
    if (remain < req_size)
    {
        // Insufficient buffer size.
        return 0;
    }
    memcpy(kii->_rw_buff + kii->_resp_offset + kii->_rw_buff_written, buffer, req_size);
    kii->_rw_buff_written += req_size;
    return req_size;
}

size_t _cb_read_buff(char *buffer, size_t size, void *userdata)
{
    kii_t *kii = (kii_t *)userdata;
    size_t remain = kii->_rw_buff_req_size - kii->_rw_buff_read;
    if (remain <= 0)
    {
        return 0;
    }
    size_t to_read = (size > remain) ? (remain) : (size);
    memcpy(buffer, kii->_rw_buff + kii->_rw_buff_read, to_read);
    kii->_rw_buff_read += to_read;
    return to_read;
}

size_t _cb_write_header(char *buffer, size_t size, void *userdata)
{
    // TODO: implement it later for getting Etag, etc.
    char* etag_buff = ((kii_t*)userdata)->_etag;
    _parse_etag(buffer, size, etag_buff, KII_ETAG_MAX_SIZE);
    _record_resp_date((kii_t*)userdata, buffer, size);
    return size;
}

void kii_init(kii_t* kii)
{
    memset(kii, 0x00, sizeof(kii_t));
    khc_init(&kii->_khc);
    khc_set_cb_read(&kii->_khc, _cb_read_buff, kii);
    khc_set_cb_write(&kii->_khc, _cb_write_buff, kii);
    khc_set_cb_header(&kii->_khc, _cb_write_header, kii);
    kii->_etag[0] = '\0';
    kii->_resp_date = -1;
    /* A week's margin, for a committer whose clock was a little ahead. */
    kii->_resp_date_floor = EBISU_SOURCE_TIME - 7 * 86400;
    kii->_cb_slist_alloc = khc_cb_slist_alloc;
    kii->_cb_slist_free = khc_cb_slist_free;
    kii->_slist_alloc_data = NULL;
    kii->_slist_free_data = NULL;
    kii->_cb_task_create = NULL;
    kii->_task_create_data = NULL;
    kii->_cb_task_continue = NULL;
    kii->_task_continue_data = NULL;
    kii->_cb_task_exit = NULL;
    kii->_task_exit_data = NULL;
    kii->_cb_delay_ms = NULL;
    kii->_delay_ms_data = NULL;
    kii->_cb_mqtt_sock_recv_timed_out = NULL;
    kii->_mqtt_sock_recv_timed_out_ctx = NULL;
    kii->_insecure_mqtt = KII_FALSE;
    kii->_use_m_0_header = KII_FALSE;
}

/* Copies src into a fixed-size field, truncating rather than overflowing, and
 * always terminating.
 *
 * These fields used to be filled with
 * strncpy(kii->_app_id, app_id, sizeof(kii->_app_id)). strncpy writes no
 * terminator when the source is at least as long as the count, so an app id or
 * host of 128 characters or more left the field unterminated, and every later
 * strlen or strcat over it ran off the end of the struct field.
 *
 * khc_set_host() rejects over-long input with KHC_ERR_TOO_LARGE_DATA instead,
 * which is the better contract, but these two are void and part of the public
 * API. */
static void _kii_copy_bounded(char* dest, size_t dest_size, const char* src)
{
    /* Reads at most dest_size - 1 bytes of src, since strncpy stops at the
     * source's terminator. Measuring src with strlen first would instead scan
     * all of it, however long it is, to copy at most this many bytes. */
    strncpy(dest, src, dest_size - 1);
    dest[dest_size - 1] = '\0';
}

void kii_set_site(
        kii_t* kii,
        const char* site)
{
    char* host;
    if(strcmp(site, "CN3") == 0)
    {
        host = "api-cn3.kii.com";
    }
    else if(strcmp(site, "JP") == 0)
    {
        host = "api-jp.kii.com";
    }
    else if(strcmp(site, "US") == 0)
    {
        host = "api.kii.com";
    }
    else if(strcmp(site, "SG") == 0)
    {
        host = "api-sg.kii.com";
    }
    else if (strcmp(site, "EU") == 0)
    {
        host = "api-eu.kii.com";
    }
    else
    {
        /* Let's enable to set custom host */
        host = (char*)site;
    }
    _kii_copy_bounded(kii->_app_host, sizeof(kii->_app_host), host);
}

void kii_set_app_id(
        kii_t* kii,
        const char* app_id)
{
    _kii_copy_bounded(kii->_app_id, sizeof(kii->_app_id), app_id);
}

void kii_set_buff(kii_t* kii, char* buff, size_t buff_size) {
    kii->_rw_buff = buff;
    kii->_rw_buff_size = buff_size;
    kii->_rw_buff_read = 0;
    kii->_rw_buff_written = 0;
    khc_set_cb_read(&kii->_khc, _cb_read_buff, kii);
    khc_set_cb_write(&kii->_khc, _cb_write_buff, kii);
}

void kii_set_stream_buff(kii_t* kii, char* buff, size_t buff_size) {
    kii->_stream_buff = buff;
    kii->_stream_buff_size = buff_size;
    khc_set_stream_buff(&kii->_khc, buff, buff_size);
}

void kii_set_resp_header_buff(kii_t* kii, char* buff, size_t buff_size) {
    kii->_resp_header_buff = buff;
    kii->_resp_header_buff_size = buff_size;
    khc_set_resp_header_buff(&kii->_khc, buff, buff_size);
}

void kii_set_cb_http_sock_connect(kii_t* kii, KHC_CB_SOCK_CONNECT cb, void* userdata) {
    khc_set_cb_sock_connect(&kii->_khc, cb, userdata);
}

void kii_set_cb_http_sock_send(kii_t* kii, KHC_CB_SOCK_SEND cb, void* userdata) {
    khc_set_cb_sock_send(&kii->_khc, cb, userdata);
}

void kii_set_cb_http_sock_recv(kii_t* kii, KHC_CB_SOCK_RECV cb, void* userdata) {
    khc_set_cb_sock_recv(&kii->_khc, cb, userdata);
}

void kii_set_cb_http_sock_close(kii_t* kii, KHC_CB_SOCK_CLOSE cb, void* userdata) {
    khc_set_cb_sock_close(&kii->_khc, cb, userdata);
}

void kii_set_mqtt_buff(kii_t* kii, char* buff, size_t buff_size) {
    kii->_mqtt_buffer = buff;
    kii->_mqtt_buffer_size = buff_size;
}

void kii_set_cb_mqtt_sock_connect(kii_t* kii, KHC_CB_SOCK_CONNECT cb, void* userdata) {
    kii->_cb_mqtt_sock_connect = cb;
    kii->_mqtt_sock_connect_ctx = userdata;
}

void kii_set_cb_mqtt_sock_send(kii_t* kii, KHC_CB_SOCK_SEND cb, void* userdata) {
    kii->_cb_mqtt_sock_send = cb;
    kii->_mqtt_sock_send_ctx = userdata;
}

void kii_set_cb_mqtt_sock_recv(kii_t* kii, KHC_CB_SOCK_RECV cb, void* userdata) {
    kii->_cb_mqtt_sock_recv = cb;
    kii->_mqtt_sock_recv_ctx = userdata;
}

void kii_set_cb_mqtt_sock_recv_timed_out(kii_t* kii, KII_CB_SOCK_RECV_TIMED_OUT cb, void* userdata) {
    kii->_cb_mqtt_sock_recv_timed_out = cb;
    kii->_mqtt_sock_recv_timed_out_ctx = userdata;
}

void kii_set_cb_mqtt_sock_close(kii_t* kii, KHC_CB_SOCK_CLOSE cb, void* userdata) {
    kii->_cb_mqtt_sock_close_cb = cb;
    kii->_mqtt_sock_close_ctx = userdata;
}

void kii_set_mqtt_to_sock_recv(kii_t* kii, uint32_t to_sock_recv_sec) {
    kii->_mqtt_to_recv_sec = to_sock_recv_sec;
}

void kii_set_mqtt_to_sock_send(kii_t* kii, uint32_t to_sock_send_sec) {
    kii->_mqtt_to_send_sec = to_sock_send_sec;
}

void kii_set_cb_task_create(kii_t* kii, KII_CB_TASK_CREATE cb, void* userdata) {
    kii->_cb_task_create = cb;
    kii->_task_create_data = userdata;
}

void kii_set_cb_task_continue(kii_t* kii, KII_CB_TASK_CONTINUE cb, void* userdata) {
    kii->_cb_task_continue = cb;
    kii->_task_continue_data = userdata;
}

void kii_set_cb_task_exit(kii_t* kii, KII_CB_TASK_EXIT cb, void* userdata) {
    kii->_cb_task_exit = cb;
    kii->_task_exit_data = userdata;
}

void kii_set_cb_delay_ms(kii_t* kii, KII_CB_DELAY_MS cb, void* userdata) {
    kii->_cb_delay_ms = cb;
    kii->_delay_ms_data = userdata;
}

void kii_set_json_parser_resource(kii_t* kii, jkii_resource_t* resource) {
    kii->_json_resource = resource;
}

void kii_set_cb_json_parser_resource(
    kii_t* kii,
    JKII_CB_RESOURCE_ALLOC cb_alloc,
    JKII_CB_RESOURCE_FREE cb_free)
{
    kii->_cb_json_alloc = cb_alloc;
    kii->_cb_json_free = cb_free;
}

void kii_set_cb_slist_resource(
        kii_t* kii,
        KHC_CB_SLIST_ALLOC cb_alloc,
        KHC_CB_SLIST_FREE cb_free,
        void* cb_alloc_data,
        void* cb_free_data) {
    kii->_cb_slist_alloc = cb_alloc;
    kii->_cb_slist_free = cb_free;
    kii->_slist_alloc_data = cb_alloc_data;
    kii->_slist_free_data = cb_free_data;
}

void kii_enable_insecure_http(
    kii_t* kii,
    kii_bool_t enable_insecure_http) {
    if (enable_insecure_http == KII_TRUE) {
        khc_enable_insecure(&kii->_khc, 1);
    } else {
        khc_enable_insecure(&kii->_khc, 0);
    }
}

void kii_enable_insecure_mqtt(
    kii_t* kii,
    kii_bool_t enable_insecure_mqtt) {
    kii->_insecure_mqtt = enable_insecure_mqtt;
}

const char* kii_get_etag(kii_t* kii) {
    return kii->_etag;
}

uint16_t kii_get_resp_status(kii_t* kii) {
    return khc_get_status_code(&kii->_khc);
}

int64_t kii_get_resp_date(kii_t* kii) {
    return (kii->_resp_date >= 0) ? kii->_resp_date : -1;
}

size_t kii_get_resp_body_length(kii_t* kii) {
    return kii->_rw_buff_written;
}

void kii_get_author(kii_t* kii, kii_author_t* out_author) {
    if (kii->_session != NULL) {
        kii_session_get_author(kii->_session, out_author);
    } else {
        *out_author = kii->_author;
    }
}

void kii_set_use_m_0_header_flag(kii_t* kii, kii_bool_t flag) {
    kii->_use_m_0_header = flag;
}

void kii_cancel_request_being_prepared(kii_t* kii) {
    _req_headers_free_all(kii);
}


kii_code_t _convert_code(khc_code khc_c) {
    switch(khc_c) {
        case KHC_ERR_OK:
            return KII_ERR_OK;
        case KHC_ERR_SOCK_CONNECT:
            return KII_ERR_SOCK_CONNECT;
        case KHC_ERR_SOCK_CLOSE:
            return KII_ERR_SOCK_CONNECT;
        case KHC_ERR_SOCK_SEND:
            return KII_ERR_SOCK_SEND;
        case KHC_ERR_SOCK_RECV:
            return KII_ERR_SOCK_RECV;
        case KHC_ERR_HEADER_CALLBACK:
            return KII_ERR_HEADER_CALLBACK;
        case KHC_ERR_WRITE_CALLBACK:
            return KII_ERR_WRITE_CALLBACK;
        case KHC_ERR_ALLOCATION:
            return KII_ERR_ALLOCATION;
        case KHC_ERR_TOO_LARGE_DATA:
            return KII_ERR_TOO_LARGE_DATA;
        case KHC_ERR_FAIL:
            return KII_ERR_FAIL;
    }
    return KII_ERR_FAIL;
}

void _reset_buff(kii_t* kii) {
    kii->_rw_buff_read = 0;
    kii->_rw_buff_written = 0;
    kii->_resp_offset = 0;
    kii->_rw_buff_req_size = 0;
    kii->_rw_buff[0] = '\0';
    kii->_etag[0] = '\0';
    kii->_resp_date = -1;
}

/* khc treats its stream and response header buffers as per-request, so
 * khc_reset_except_cb() clears them along with the rest of the request state.
 * kii's setters are per kii_t, and every API resets khc before its request,
 * so the buffers have to be handed back to khc each time. Resetting khc
 * directly instead left them NULL, and khc allocated its defaults for every
 * request. */
void _reset_khc(kii_t* kii) {
    khc_reset_except_cb(&kii->_khc);
    khc_set_stream_buff(&kii->_khc, kii->_stream_buff, kii->_stream_buff_size);
    khc_set_resp_header_buff(&kii->_khc, kii->_resp_header_buff, kii->_resp_header_buff_size);
}

void _req_headers_free_all(kii_t* kii) {
    khc_slist_free_all_using_cb_free(kii->_req_headers, kii->_cb_slist_free, kii->_slist_free_data);
    kii->_req_headers = NULL;
}

int _parse_etag(char* header, size_t header_len, char* buff, size_t buff_len) {
    const char etag_lower[] = "etag";
    const char etag_upper[] = "ETAG";
    size_t key_len = strlen(etag_lower);
    int state = 0;
    int j = 0;
    for (int i = 0; i < header_len; ++i) {
        char c = header[i];

        if (state == 0) {
            if (c == etag_lower[i] || c == etag_upper[i]) {
                if (i == key_len - 1) {
                    state = 1;
                }
                continue;
            } else {
                // Not Etag.
                return -1;
            }
        } else if (state == 1) { // Skip WP before :
            if (c == ' ' || c == '\t') {
                continue;
            } else if ( c == ':') {
                state = 2;
                continue;
            } else {
                // Inalid Format.
                return -2;
            }
        } else if (state == 2) { // Skip WP after :
            if (c == ' ' || c == '\t') {
                continue;
            } else {
                state = 3;
                buff[0] = c;
                j++;
                continue;
            }
        } else if (state == 3) { // Extract value
            if (c == ' ' || c == '\t' || c == '\r') {
                break;
            } else {
                if (j < buff_len - 1) {
                    buff[j] = c;
                    ++j;
                    continue;
                } else {
                    // Etag too large.
                    return -3;
                }
            }
        }
    }
    buff[j] = '\0';
    return j;
}

static const char _DAY_NAMES[7][3] = {
    { 'S', 'u', 'n' }, { 'M', 'o', 'n' }, { 'T', 'u', 'e' }, { 'W', 'e', 'd' },
    { 'T', 'h', 'u' }, { 'F', 'r', 'i' }, { 'S', 'a', 't' }
};
/* RFC 850's whole names, in the same order. */
static const char* const _DAY_NAMES_LONG[7] = {
    "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
};
static const char _MONTH_NAMES[12][3] = {
    { 'J', 'a', 'n' }, { 'F', 'e', 'b' }, { 'M', 'a', 'r' }, { 'A', 'p', 'r' },
    { 'M', 'a', 'y' }, { 'J', 'u', 'n' }, { 'J', 'u', 'l' }, { 'A', 'u', 'g' },
    { 'S', 'e', 'p' }, { 'O', 'c', 't' }, { 'N', 'o', 'v' }, { 'D', 'e', 'c' }
};

/* The fields of an HTTP-date, as written: any of them may be out of range, or
 * -1 where the text held no number or name. */
typedef struct {
    int weekday;
    int day;
    int month;
    int year;
    int hour;
    int minute;
    int second;
} _http_date_t;

/* The index in names of the three letters at s, compared case-sensitively as
 * RFC 9110 asks, or -1. */
static int _find_name(const char* s, const char (*names)[3], int count)
{
    for (int i = 0; i < count; ++i) {
        if (memcmp(s, names[i], 3) == 0) {
            return i;
        }
    }
    return -1;
}

/* The value of the n decimal digits at s, or -1 if any of them is not one. */
static int _parse_digits(const char* s, size_t n)
{
    int value = 0;
    for (size_t i = 0; i < n; ++i) {
        if (s[i] < '0' || s[i] > '9') {
            return -1;
        }
        value = value * 10 + (s[i] - '0');
    }
    return value;
}

static int _is_leap(int year)
{
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

static int _days_in_month(int year, int month)
{
    static const int days[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (month == 2 && _is_leap(year)) {
        return 29;
    }
    return days[month - 1];
}

/* Days from 1970-01-01 to a valid date of the Gregorian calendar, from 1970
 * on. The year is counted from March, so that a leap day comes last in it.
 * Howard Hinnant's days_from_civil, donated to the public domain, without its
 * handling of years before 0:
 * https://howardhinnant.github.io/date_algorithms.html#days_from_civil */
static int64_t _days_since_epoch(int year, int month, int day)
{
    int64_t y = (month <= 2) ? year - 1 : year;
    int64_t m = (month <= 2) ? month + 9 : month - 3;
    int64_t days = y * 365 + y / 4 - y / 100 + y / 400 + (153 * m + 2) / 5 + day - 1;
    /* The same count for 1970-01-01. */
    return days - 719468;
}

/* The year seconds since the epoch fall in; 1970 for anything earlier. */
static int _year_of(int64_t seconds)
{
    int year = 1970;
    int64_t days = (seconds > 0) ? seconds / 86400 : 0;
    while (days >= (_is_leap(year) ? 366 : 365)) {
        days -= _is_leap(year) ? 366 : 365;
        ++year;
    }
    return year;
}

/* "08:49:37" */
static int _parse_time_of_day(const char* s, _http_date_t* d)
{
    if (s[2] != ':' || s[5] != ':') {
        return -1;
    }
    d->hour = _parse_digits(s, 2);
    d->minute = _parse_digits(s + 3, 2);
    d->second = _parse_digits(s + 6, 2);
    return 0;
}

/* GMT, or UTC, which some servers send for it. */
static int _is_gmt(const char* s)
{
    return memcmp(s, "GMT", 3) == 0 || memcmp(s, "UTC", 3) == 0;
}

/* IMF-fixdate, the form servers must send: "Sun, 06 Nov 1994 08:49:37 GMT". */
static int _parse_imf_fixdate(const char* v, size_t len, _http_date_t* d)
{
    if (len != 29 || v[3] != ',' || v[4] != ' ' || v[7] != ' ' ||
            v[11] != ' ' || v[16] != ' ' || v[25] != ' ' || !_is_gmt(v + 26)) {
        return -1;
    }
    d->weekday = _find_name(v, _DAY_NAMES, 7);
    d->day = _parse_digits(v + 5, 2);
    d->month = _find_name(v + 8, _MONTH_NAMES, 12) + 1;
    d->year = _parse_digits(v + 12, 4);
    return _parse_time_of_day(v + 17, d);
}

/* C's asctime(), with no zone since it is always GMT, and the day padded with
 * a space or written with two digits: "Sun Nov  6 08:49:37 1994". */
static int _parse_asctime(const char* v, size_t len, _http_date_t* d)
{
    if (len != 24 || v[3] != ' ' || v[7] != ' ' || v[10] != ' ' || v[19] != ' ') {
        return -1;
    }
    d->weekday = _find_name(v, _DAY_NAMES, 7);
    d->month = _find_name(v + 4, _MONTH_NAMES, 12) + 1;
    d->day = (v[8] == ' ') ? _parse_digits(v + 9, 1) : _parse_digits(v + 8, 2);
    d->year = _parse_digits(v + 20, 4);
    return _parse_time_of_day(v + 11, d);
}

/* RFC 850, with the day's whole name and two digits of the year:
 * "Sunday, 06-Nov-94 08:49:37 GMT". The year is the one with those two digits
 * within 50 years either side of present_year, never more than 50 years ahead
 * of it, as RFC 9110 asks. */
static int _parse_rfc850(const char* v, size_t len, int present_year, _http_date_t* d)
{
    int weekday = -1;
    size_t name_len = 0;
    for (int i = 0; i < 7; ++i) {
        size_t n = strlen(_DAY_NAMES_LONG[i]);
        if (len > n && memcmp(v, _DAY_NAMES_LONG[i], n) == 0 && v[n] == ',') {
            weekday = i;
            name_len = n;
            break;
        }
    }
    /* ", 06-Nov-94 08:49:37 GMT" after the name. */
    if (weekday < 0 || len != name_len + 24) {
        return -1;
    }
    const char* s = v + name_len + 2;
    if (v[name_len + 1] != ' ' || s[2] != '-' || s[6] != '-' || s[9] != ' ' ||
            s[18] != ' ' || !_is_gmt(s + 19)) {
        return -1;
    }
    d->weekday = weekday;
    d->day = _parse_digits(s, 2);
    d->month = _find_name(s + 3, _MONTH_NAMES, 12) + 1;
    int two_digits = _parse_digits(s + 7, 2);
    if (two_digits < 0) {
        return -1;
    }
    d->year = present_year - present_year % 100 + two_digits;
    if (d->year > present_year + 50) {
        d->year -= 100;
    } else if (d->year <= present_year - 50) {
        d->year += 100;
    }
    return _parse_time_of_day(s + 10, d);
}

int64_t _parse_date(const char* header, size_t header_len, int64_t floor)
{
    /* RFC 9110 allows no whitespace between a field's name and the colon. */
    static const char name[] = "date:";
    const size_t name_len = sizeof(name) - 1;
    if (header == NULL || header_len < name_len) {
        return -1;
    }
    for (size_t i = 0; i < name_len; ++i) {
        char c = header[i];
        if (c >= 'A' && c <= 'Z') {
            c = (char)(c - 'A' + 'a');
        }
        if (c != name[i]) {
            return -1;
        }
    }

    /* The value, without the whitespace around it. */
    const char* v = header + name_len;
    const char* end = header + header_len;
    while (v < end && (*v == ' ' || *v == '\t')) {
        ++v;
    }
    while (end > v && (end[-1] == ' ' || end[-1] == '\t')) {
        --end;
    }
    size_t len = (size_t)(end - v);

    /* The three forms of RFC 9110, section 5.6.7, which a recipient must all
     * take. Each sets every field when its layout matches. */
    _http_date_t d;
    if (_parse_imf_fixdate(v, len, &d) != 0 &&
            _parse_asctime(v, len, &d) != 0 &&
            _parse_rfc850(v, len, _year_of(floor), &d) != 0) {
        return -2;
    }
    /* 60 is a leap second, which only ever ends a UTC day. */
    if (d.weekday < 0 || d.month < 1 || d.year < 1970 ||
            d.day < 1 || d.day > _days_in_month(d.year, d.month) ||
            d.hour < 0 || d.hour > 23 || d.minute < 0 || d.minute > 59 ||
            d.second < 0 || d.second > 60 ||
            (d.second == 60 && (d.hour != 23 || d.minute != 59))) {
        return -2;
    }
    int64_t days = _days_since_epoch(d.year, d.month, d.day);
    /* The day's name has to agree with the date: 1970-01-01 was a Thursday. */
    if ((days + 4) % 7 != d.weekday) {
        return -2;
    }
    int64_t date = ((days * 24 + d.hour) * 60 + d.minute) * 60 + d.second;
    return (date < floor) ? -2 : date;
}

void _record_resp_date(kii_t* kii, const char* line, size_t line_len)
{
    if (line == NULL) {
        return;
    }
    /* khc hands each response's status line to the header callback before its
     * headers, so it starts a response: a token renewal within the same call
     * brings several. */
    if (line_len >= 5 && memcmp(line, "HTTP/", 5) == 0) {
        kii->_resp_date = -1;
        return;
    }
    int64_t date = _parse_date(line, line_len, kii->_resp_date_floor);
    if (date == -1) {
        return;
    }
    /* A malformed Date, or Dates that disagree, leave the response without
     * one rather than with whichever came last. -2 holds that until the next
     * response. */
    if (date == -2 || (kii->_resp_date != -1 && kii->_resp_date != date)) {
        kii->_resp_date = -2;
    } else {
        kii->_resp_date = date;
    }
}
