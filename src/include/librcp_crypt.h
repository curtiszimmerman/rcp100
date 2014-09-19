#ifndef LIBRCPCRYPT_H
#define LIBRCPCRYPT_H

#ifndef LIBRCP_H
#include <stdint.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* MD5.H - header file for MD5C.C
 from RFC 1321              MD5 Message-Digest Algorithm            April 1992
 */

/* Copyright (C) 1991-2, RSA Data Security, Inc. Created 1991. All
rights reserved.

License to copy and use this software is granted provided that it
is identified as the "RSA Data Security, Inc. MD5 Message-Digest
Algorithm" in all material mentioning or referencing this software
or this function.

License is also granted to make and use derivative works provided
that such works are identified as "derived from the RSA Data
Security, Inc. MD5 Message-Digest Algorithm" in all material
mentioning or referencing the derived work.

RSA Data Security, Inc. makes no representations concerning either
the merchantability of this software or the suitability of this
software for any particular purpose. It is provided "as is"
without express or implied warranty of any kind.

These notices must be retained in any copies of any part of this
documentation and/or software.
 */

/* MD5 context. */
typedef struct {
  uint32_t state[4];                                   /* state (ABCD) */
  uint32_t count[2];        /* number of bits, modulo 2^64 (lsb first) */
  unsigned char buffer[64];                         /* input buffer */
} MD5_CTX;

void MD5Init(MD5_CTX *);
void MD5Update(MD5_CTX *, unsigned char *, unsigned int);
void MD5Final(unsigned char [16], MD5_CTX *);

#define RCP_CRYPT_SALT_LEN 8
char *rcpCrypt(const char *pw, const char *salt);

#if 0
Example:
  unsigned char *string = ...
  MD5_CTX context;
  unsigned char digest[16];
  unsigned int len = strlen (string);

  MD5Init (&context);
  MD5Update (&context, string, len);
  MD5Final (digest, &context);
#endif

#ifdef __cplusplus
}
#endif

#endif
