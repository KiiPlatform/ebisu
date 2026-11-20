/* mbedTLS 2.4.0 for the large tests on lwIP, as Realtek's SDKs for AmebaZ and
   AmebaZ2 configure it, with their config_rsa.h: TLS 1.2 at the most, RSA key
   exchange alone, AES in CBC mode and RC4, and records of 4096 bytes at the
   most. No time, no threads, no CTR_DRBG and no entropy module: random.c
   gives TLS a generator of its own, and ebisu's port checks a certificate's
   validity period at a clock's time. Of their options, all but
   MBEDTLS_NET_C, BSD sockets, which ebisu doesn't use,
   MBEDTLS_PLATFORM_NO_STD_FUNCTIONS, whose functions the SDKs give from
   their own C library, and MBEDTLS_SSL_TICKET_C, a server's tickets, which
   calls AEAD functions this configuration has none of; and with MBEDTLS_ECP_C, of the SDK for AmebaZ's, and
   MBEDTLS_DEBUG_C, of the SDK for AmebaZ2's, both. See mbedTLS's
   include/mbedtls/config.h for each. */
#ifndef MBEDTLS_CONFIG_H
#define MBEDTLS_CONFIG_H

/* Its allocator the application's, with mbedtls_platform_set_calloc_free(),
   as random_init() sets it. */
#define MBEDTLS_PLATFORM_C
#define MBEDTLS_PLATFORM_MEMORY

#define MBEDTLS_AES_C
#define MBEDTLS_AES_ROM_TABLES
#define MBEDTLS_ARC4_C
#define MBEDTLS_CIPHER_C
#define MBEDTLS_CIPHER_MODE_CBC
#define MBEDTLS_CIPHER_MODE_CFB
#define MBEDTLS_CIPHER_MODE_CTR
#define MBEDTLS_CIPHER_PADDING_PKCS7
#define MBEDTLS_CIPHER_PADDING_ONE_AND_ZEROS
#define MBEDTLS_CIPHER_PADDING_ZEROS_AND_LEN
#define MBEDTLS_CIPHER_PADDING_ZEROS
#define MBEDTLS_MD_C
#define MBEDTLS_MD5_C
#define MBEDTLS_SHA1_C
#define MBEDTLS_SHA256_C
#define MBEDTLS_SHA512_C

#define MBEDTLS_BIGNUM_C
#define MBEDTLS_GENPRIME
#define MBEDTLS_RSA_C
#define MBEDTLS_PKCS1_V15
#define MBEDTLS_PK_C
#define MBEDTLS_PK_PARSE_C
#define MBEDTLS_PK_RSA_ALT_SUPPORT
#define MBEDTLS_ECP_C
#define MBEDTLS_ECP_DP_SECP192R1_ENABLED
#define MBEDTLS_ECP_DP_SECP224R1_ENABLED
#define MBEDTLS_ECP_DP_SECP256R1_ENABLED
#define MBEDTLS_ECP_DP_SECP384R1_ENABLED
#define MBEDTLS_ECP_DP_SECP521R1_ENABLED
#define MBEDTLS_ECP_DP_SECP192K1_ENABLED
#define MBEDTLS_ECP_DP_SECP224K1_ENABLED
#define MBEDTLS_ECP_DP_SECP256K1_ENABLED
#define MBEDTLS_ECP_DP_BP256R1_ENABLED
#define MBEDTLS_ECP_DP_BP384R1_ENABLED
#define MBEDTLS_ECP_DP_BP512R1_ENABLED
#define MBEDTLS_ECP_DP_CURVE25519_ENABLED
#define MBEDTLS_ECP_NIST_OPTIM

#define MBEDTLS_ASN1_PARSE_C
#define MBEDTLS_ASN1_WRITE_C
#define MBEDTLS_BASE64_C
#define MBEDTLS_OID_C
#define MBEDTLS_PEM_PARSE_C
#define MBEDTLS_X509_USE_C
#define MBEDTLS_X509_CRT_PARSE_C

#define MBEDTLS_KEY_EXCHANGE_RSA_ENABLED
#define MBEDTLS_SSL_CLI_C
#define MBEDTLS_SSL_TLS_C
#define MBEDTLS_SSL_PROTO_TLS1
#define MBEDTLS_SSL_PROTO_TLS1_1
#define MBEDTLS_SSL_PROTO_TLS1_2
#define MBEDTLS_SSL_ALL_ALERT_MESSAGES
#define MBEDTLS_SSL_ENCRYPT_THEN_MAC
#define MBEDTLS_SSL_EXTENDED_MASTER_SECRET
#define MBEDTLS_SSL_FALLBACK_SCSV
#define MBEDTLS_SSL_CBC_RECORD_SPLITTING
#define MBEDTLS_SSL_RENEGOTIATION
#define MBEDTLS_SSL_MAX_FRAGMENT_LENGTH
#define MBEDTLS_SSL_SESSION_TICKETS
#define MBEDTLS_SSL_EXPORT_KEYS
#define MBEDTLS_SSL_SERVER_NAME_INDICATION
#define MBEDTLS_SSL_TRUNCATED_HMAC
#define MBEDTLS_SSL_CACHE_C
#define MBEDTLS_SSL_COOKIE_C
#define MBEDTLS_SSL_MAX_CONTENT_LEN 4096

#define MBEDTLS_DEBUG_C
#define MBEDTLS_ERROR_STRERROR_DUMMY
#define MBEDTLS_VERSION_C

#include "mbedtls/check_config.h"

#endif /* MBEDTLS_CONFIG_H */
