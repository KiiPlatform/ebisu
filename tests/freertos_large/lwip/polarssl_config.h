/* PolarSSL 1.3.8 for the large tests on lwIP, as Realtek's SDK for AmebaZ
   configures it, with its config_rsa.h: SSL 3.0 to TLS 1.2, RSA key exchange
   alone, AES in CBC mode and RC4, and records of 5120 bytes at the most. No
   time, no threads, no CTR_DRBG and no entropy module: random_polarssl.c
   gives TLS a generator of its own, and ebisu's port checks a certificate's
   validity period at a clock's time. Of its options, all but POLARSSL_NET_C,
   BSD sockets, which ebisu doesn't use, and POLARSSL_PLATFORM_NO_STD_FUNCTIONS,
   whose functions the SDK gives from its own C library. See PolarSSL's
   include/polarssl/config.h for each. */
#ifndef POLARSSL_CONFIG_H
#define POLARSSL_CONFIG_H

#define POLARSSL_HAVE_LONGLONG

/* Its allocator the application's, with platform_set_malloc_free(), as
   random_init() sets it. */
#define POLARSSL_PLATFORM_C
#define POLARSSL_PLATFORM_MEMORY

#define POLARSSL_AES_C
#define POLARSSL_AES_ROM_TABLES
#define POLARSSL_ARC4_C
#define POLARSSL_CIPHER_C
#define POLARSSL_CIPHER_MODE_CBC
#define POLARSSL_CIPHER_MODE_CFB
#define POLARSSL_CIPHER_MODE_CTR
#define POLARSSL_CIPHER_PADDING_PKCS7
#define POLARSSL_CIPHER_PADDING_ONE_AND_ZEROS
#define POLARSSL_CIPHER_PADDING_ZEROS_AND_LEN
#define POLARSSL_CIPHER_PADDING_ZEROS
#define POLARSSL_MD_C
#define POLARSSL_MD5_C
#define POLARSSL_SHA1_C
#define POLARSSL_SHA256_C
#define POLARSSL_SHA512_C

#define POLARSSL_BIGNUM_C
#define POLARSSL_GENPRIME
#define POLARSSL_RSA_C
#define POLARSSL_PKCS1_V15
#define POLARSSL_PK_C
#define POLARSSL_PK_PARSE_C
#define POLARSSL_ECP_C
#define POLARSSL_ECP_DP_SECP192R1_ENABLED
#define POLARSSL_ECP_DP_SECP224R1_ENABLED
#define POLARSSL_ECP_DP_SECP256R1_ENABLED
#define POLARSSL_ECP_DP_SECP384R1_ENABLED
#define POLARSSL_ECP_DP_SECP521R1_ENABLED
#define POLARSSL_ECP_DP_SECP192K1_ENABLED
#define POLARSSL_ECP_DP_SECP224K1_ENABLED
#define POLARSSL_ECP_DP_SECP256K1_ENABLED
#define POLARSSL_ECP_DP_BP256R1_ENABLED
#define POLARSSL_ECP_DP_BP384R1_ENABLED
#define POLARSSL_ECP_DP_BP512R1_ENABLED
#define POLARSSL_ECP_DP_M255_ENABLED
#define POLARSSL_ECP_NIST_OPTIM

#define POLARSSL_ASN1_PARSE_C
#define POLARSSL_ASN1_WRITE_C
#define POLARSSL_BASE64_C
#define POLARSSL_OID_C
#define POLARSSL_PEM_PARSE_C
#define POLARSSL_X509_USE_C
#define POLARSSL_X509_CRT_PARSE_C

#define POLARSSL_KEY_EXCHANGE_RSA_ENABLED
#define POLARSSL_SSL_CLI_C
#define POLARSSL_SSL_TLS_C
#define POLARSSL_SSL_PROTO_SSL3
#define POLARSSL_SSL_PROTO_TLS1
#define POLARSSL_SSL_PROTO_TLS1_1
#define POLARSSL_SSL_PROTO_TLS1_2
#define POLARSSL_SSL_ALERT_MESSAGES
#define POLARSSL_SSL_MAX_FRAGMENT_LENGTH
#define POLARSSL_SSL_SESSION_TICKETS
#define POLARSSL_SSL_SERVER_NAME_INDICATION
#define POLARSSL_SSL_TRUNCATED_HMAC
#define POLARSSL_SSL_CACHE_C
#define SSL_MAX_CONTENT_LEN 5120

#define POLARSSL_ERROR_STRERROR_BC
#define POLARSSL_ERROR_STRERROR_DUMMY
#define POLARSSL_VERSION_C

#include "polarssl/check_config.h"

#endif /* POLARSSL_CONFIG_H */
