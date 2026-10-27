#ifndef SAMPLE_MBEDTLS_CONFIG_H
#define SAMPLE_MBEDTLS_CONFIG_H

/* mbedTLS for the sample: a TLS 1.2 client, which verifies the server's
   certificate chain, RSA or ECDSA, to the CA certificates of EBISU_SAMPLE_CA_FILE,
   with an ECDHE key exchange, and AES-GCM or ChaCha20-Poly1305. Not named
   mbedtls_config.h: mbedTLS would include its own by that name instead. */

/* No MBEDTLS_HAVE_TIME_DATE: mbedTLS would check the certificates' validity
   periods at the system's time, which a device without a real-time clock does
   not have. sock_cb_freertos.c checks them at a kii_clock_t's instead. */

/* No MBEDTLS_THREADING_C either: the tasks share only the TLS configuration,
   which they read, and the random generator, which random.c locks. */

/* The board's entropy source, mbedtls_hardware_poll(), and no other. */
#define MBEDTLS_NO_PLATFORM_ENTROPY
#define MBEDTLS_ENTROPY_HARDWARE_ALT
#define MBEDTLS_ENTROPY_C
#define MBEDTLS_CTR_DRBG_C

#define MBEDTLS_SSL_TLS_C
#define MBEDTLS_SSL_CLI_C
#define MBEDTLS_SSL_PROTO_TLS1_2
#define MBEDTLS_SSL_SERVER_NAME_INDICATION
#define MBEDTLS_SSL_EXTENDED_MASTER_SECRET
#define MBEDTLS_SSL_ENCRYPT_THEN_MAC
/* A server may send records of up to 16 KB. Requests are split into smaller
   ones. */
#define MBEDTLS_SSL_IN_CONTENT_LEN 16384
#define MBEDTLS_SSL_OUT_CONTENT_LEN 4096

#define MBEDTLS_KEY_EXCHANGE_ECDHE_RSA_ENABLED
#define MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED
#define MBEDTLS_ECDH_C
#define MBEDTLS_ECDSA_C
#define MBEDTLS_ECP_C
#define MBEDTLS_ECP_NIST_OPTIM
#define MBEDTLS_ECP_DP_SECP256R1_ENABLED
#define MBEDTLS_ECP_DP_SECP384R1_ENABLED
#define MBEDTLS_ECP_DP_CURVE25519_ENABLED
#define MBEDTLS_RSA_C
#define MBEDTLS_PKCS1_V15
#define MBEDTLS_PKCS1_V21
#define MBEDTLS_BIGNUM_C

#define MBEDTLS_X509_USE_C
#define MBEDTLS_X509_CRT_PARSE_C
#define MBEDTLS_PK_C
#define MBEDTLS_PK_PARSE_C
#define MBEDTLS_PEM_PARSE_C
#define MBEDTLS_BASE64_C
#define MBEDTLS_ASN1_PARSE_C
#define MBEDTLS_ASN1_WRITE_C
#define MBEDTLS_OID_C

#define MBEDTLS_MD_C
#define MBEDTLS_SHA1_C
#define MBEDTLS_SHA224_C
#define MBEDTLS_SHA256_C
#define MBEDTLS_SHA384_C
#define MBEDTLS_SHA512_C

#define MBEDTLS_CIPHER_C
#define MBEDTLS_AES_C
#define MBEDTLS_AES_ROM_TABLES
#define MBEDTLS_GCM_C
#define MBEDTLS_CHACHA20_C
#define MBEDTLS_POLY1305_C
#define MBEDTLS_CHACHAPOLY_C

/* For mbedtls_strerror(), in the log. */
#define MBEDTLS_ERROR_C

#endif /* SAMPLE_MBEDTLS_CONFIG_H */
