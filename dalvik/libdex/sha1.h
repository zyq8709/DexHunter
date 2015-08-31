/*
 * See "sha1.cpp" for author info.
 */
#ifndef LIBDEX_SHA1_H_
#define LIBDEX_SHA1_H_

struct SHA1_CTX {
    unsigned long state[5];
    unsigned long count[2];
    unsigned char buffer[64];
};

#define HASHSIZE 20

void SHA1Init(SHA1_CTX* context);
void SHA1Update(SHA1_CTX* context, const unsigned char* data,
    unsigned long len);
void SHA1Final(unsigned char digest[HASHSIZE], SHA1_CTX* context);

#endif  // LIBDEX_SHA1_H_
