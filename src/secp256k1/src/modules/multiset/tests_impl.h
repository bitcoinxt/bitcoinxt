/**********************************************************************
 * Copyright (c) 2017 Tomas van der Wansem                            *
 * Distributed under the MIT software license, see the accompanying   *
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.*
 **********************************************************************/

#ifndef _SECP256K1_MODULE_MULTISET_TESTS_
#define _SECP256K1_MODULE_MULTISET_TESTS_


#include "include/secp256k1.h"
#include "include/secp256k1_multiset.h"
#include "util.h"
#include "testrand.h"

#define DATALEN   64*3
#define DATACOUNT 100


#define CHECK_EQUAL(a,b) { \
    unsigned char hash1[32]; \
    unsigned char hash2[32]; \
    secp256k1_multiset_finalize(ctx, hash1, (a)); \
    secp256k1_multiset_finalize(ctx, hash2, (b)); \
    CHECK(memcmp(hash1,hash2,sizeof(hash1))==0); \
}

#define CHECK_NOTEQUAL(a,b) { \
    unsigned char hash1[32]; \
    unsigned char hash2[32]; \
    secp256k1_multiset_finalize(ctx, hash1, (a)); \
    secp256k1_multiset_finalize(ctx, hash2, (b)); \
    CHECK(memcmp(hash1,hash2,sizeof(hash1))!=0); \
}

static unsigned char elements[DATACOUNT][DATALEN];

/* Create random data */
static void initdata(void) {
    int n,m;
    for(n=0; n < DATACOUNT; n++) {
        for(m=0; m < DATALEN/4; m++) {
            ((uint32_t*) elements[n])[m] = secp256k1_rand32();
        }

    }
}

void test_unordered(void) {

    /* Check if multisets are uneffected by order */

    secp256k1_multiset empty, r1,r2,r3;

    secp256k1_multiset_init(ctx, &empty);
    secp256k1_multiset_init(ctx, &r1);
    secp256k1_multiset_init(ctx, &r2);
    secp256k1_multiset_init(ctx, &r3);

    secp256k1_multiset_add(ctx, &r1, elements[0], DATALEN);
    secp256k1_multiset_add(ctx, &r2, elements[1], DATALEN);

    CHECK_NOTEQUAL(&r1,&r2); /* M(0,1)!=M() */

    secp256k1_multiset_add(ctx, &r1, elements[1], DATALEN);
    secp256k1_multiset_add(ctx, &r2, elements[0], DATALEN);
    CHECK_EQUAL(&r1,&r2); /* M(0,1)==M(1,0) */

    secp256k1_multiset_init(ctx, &r1);
    secp256k1_multiset_init(ctx, &r2);
    secp256k1_multiset_init(ctx, &r3);

    CHECK_EQUAL(&r1,&r2); /* M()==M() */

    secp256k1_multiset_add(ctx, &r1, elements[0], DATALEN);
    secp256k1_multiset_add(ctx, &r1, elements[1], DATALEN);
    secp256k1_multiset_add(ctx, &r1, elements[2], DATALEN);

    secp256k1_multiset_add(ctx, &r2, elements[2], DATALEN);
    secp256k1_multiset_add(ctx, &r2, elements[0], DATALEN);
    secp256k1_multiset_add(ctx, &r2, elements[1], DATALEN);

    secp256k1_multiset_add(ctx, &r3, elements[1], DATALEN);
    secp256k1_multiset_add(ctx, &r3, elements[0], DATALEN);
    secp256k1_multiset_add(ctx, &r3, elements[2], DATALEN);

    CHECK_EQUAL(&r1,&r2); /* M(0,1,2)==M(2,0,1) */
    CHECK_EQUAL(&r1,&r3); /* M(0,1,2)==M(1,0,2) */


    secp256k1_multiset_combine(ctx, &r3, &empty);
    CHECK_EQUAL(&r1,&r3); /* M(1,0,2)+M()=M(0,1,2) */

    secp256k1_multiset_combine(ctx, &r3, &r2);
    CHECK_NOTEQUAL(&r1,&r3); /* M(1,0,2)+M(0,1,2)!=M(0,1,2) */

}

void test_combine(void) {

    /* Testing if combining is effectively the same as adding the elements */

    secp256k1_multiset empty, r1,r2,r3;

    secp256k1_multiset_init(ctx, &empty);
    secp256k1_multiset_init(ctx, &r1);
    secp256k1_multiset_init(ctx, &r2);
    secp256k1_multiset_init(ctx, &r3);

    secp256k1_multiset_add(ctx, &r1, elements[0], DATALEN);
    secp256k1_multiset_add(ctx, &r1, elements[1], DATALEN);
    secp256k1_multiset_add(ctx, &r2, elements[0], DATALEN);
    secp256k1_multiset_add(ctx, &r2, elements[1], DATALEN);

    secp256k1_multiset_init(ctx, &r1);
    secp256k1_multiset_init(ctx, &r2);
    secp256k1_multiset_init(ctx, &r3);

    CHECK_EQUAL(&r1,&r2); /* M() == M() */

    secp256k1_multiset_add(ctx, &r1, elements[0], DATALEN);
    secp256k1_multiset_add(ctx, &r1, elements[1], DATALEN);
    secp256k1_multiset_add(ctx, &r1, elements[2], DATALEN);

    secp256k1_multiset_add(ctx, &r2, elements[2], DATALEN);
    secp256k1_multiset_add(ctx, &r3, elements[0], DATALEN);
    secp256k1_multiset_add(ctx, &r3, elements[1], DATALEN);
    secp256k1_multiset_combine(ctx, &r2, &r3);
    CHECK_EQUAL(&r1,&r2); /* M(0,1,2) == M(2)+M(0,1) */

    secp256k1_multiset_init(ctx, &r2);
    secp256k1_multiset_init(ctx, &r3);
    secp256k1_multiset_add(ctx, &r2, elements[2], DATALEN);
    secp256k1_multiset_add(ctx, &r2, elements[0], DATALEN);
    secp256k1_multiset_add(ctx, &r3, elements[1], DATALEN);
    secp256k1_multiset_combine(ctx, &r2, &r3);
    CHECK_EQUAL(&r1,&r2); /* M(0,1,2) == M(2,0)+M(1) */

    secp256k1_multiset_combine(ctx, &r2, &empty);
    CHECK_EQUAL(&r1,&r2); /* M(0,1,2)+M() == M(0,1,2) */
    secp256k1_multiset_combine(ctx, &r2, &r1);
    CHECK_NOTEQUAL(&r1,&r2); /* M(0,1,2)+M(0,1,2) != M(0,1,2) */
}


void test_remove(void) {

    /* Testing removal of elements */
    secp256k1_multiset empty, r1,r2,r3;

    secp256k1_multiset_init(ctx, &empty);
    secp256k1_multiset_init(ctx, &r1);
    secp256k1_multiset_init(ctx, &r2);
    secp256k1_multiset_init(ctx, &r3);

    CHECK_EQUAL(&r1,&r2); /* M()==M() */

    secp256k1_multiset_add   (ctx, &r1, elements[0], DATALEN);
    secp256k1_multiset_add   (ctx, &r1, elements[1], DATALEN);
    secp256k1_multiset_add   (ctx, &r1, elements[3], DATALEN);
    secp256k1_multiset_add   (ctx, &r1, elements[9], DATALEN);
    secp256k1_multiset_add   (ctx, &r1, elements[8], DATALEN);

    secp256k1_multiset_add   (ctx, &r2, elements[1], DATALEN);
    secp256k1_multiset_add   (ctx, &r2, elements[9], DATALEN);
    secp256k1_multiset_add   (ctx, &r2, elements[11], DATALEN);
    secp256k1_multiset_add   (ctx, &r2, elements[10], DATALEN);
    secp256k1_multiset_add   (ctx, &r2, elements[0], DATALEN);
    secp256k1_multiset_remove(ctx, &r2, elements[10], DATALEN);
    secp256k1_multiset_add   (ctx, &r2, elements[3], DATALEN);
    secp256k1_multiset_add   (ctx, &r2, elements[8], DATALEN);
    secp256k1_multiset_remove(ctx, &r2, elements[11], DATALEN);

    secp256k1_multiset_add   (ctx, &r3, elements[9], DATALEN);
    secp256k1_multiset_add   (ctx, &r3, elements[15], DATALEN);
    secp256k1_multiset_add   (ctx, &r3, elements[15], DATALEN);
    secp256k1_multiset_add   (ctx, &r3, elements[1], DATALEN);
    secp256k1_multiset_add   (ctx, &r3, elements[9], DATALEN);
    secp256k1_multiset_remove(ctx, &r3, elements[15], DATALEN);
    secp256k1_multiset_add   (ctx, &r3, elements[0], DATALEN);
    secp256k1_multiset_remove(ctx, &r3, elements[15], DATALEN);
    secp256k1_multiset_remove(ctx, &r3, elements[9], DATALEN);
    secp256k1_multiset_add   (ctx, &r3, elements[3], DATALEN);
    secp256k1_multiset_add   (ctx, &r3, elements[8], DATALEN);

    CHECK_EQUAL(&r1,&r2); /* M(0,1,3,9,8)==M(1,9,11,10,9,3,8)-M(10,11) */
    CHECK_EQUAL(&r1,&r3); /* M(0,1,3,9,8)==M(9,15,15,1,9,0,3,8)-M(15,15,9) */
    CHECK_NOTEQUAL(&r1,&empty); /* M(0,1,3,9,8)!=M() */

    secp256k1_multiset_remove(ctx, &r3, elements[8], DATALEN);
    CHECK_NOTEQUAL(&r1,&r3); /* M(0,1,3,9,8)-M(8)!=M(0,1,3,9,8) */

    secp256k1_multiset_remove(ctx, &r2, elements[0], DATALEN);
    secp256k1_multiset_remove(ctx, &r2, elements[9], DATALEN);
    secp256k1_multiset_remove(ctx, &r2, elements[8], DATALEN);
    secp256k1_multiset_remove(ctx, &r2, elements[1], DATALEN);
    secp256k1_multiset_remove(ctx, &r2, elements[3], DATALEN);

    CHECK_EQUAL(&r2,&empty); /* M(0,1,3,9,8)-M(0,1,3,9,8)==M() */
}

void test_duplicate(void) {

    /* Test if the multiset properly handles duplicates */
    secp256k1_multiset empty, r1,r2,r3;

    secp256k1_multiset_init(ctx, &empty);
    secp256k1_multiset_init(ctx, &r1);
    secp256k1_multiset_init(ctx, &r2);
    secp256k1_multiset_init(ctx, &r3);

    secp256k1_multiset_add   (ctx, &r1, elements[0], DATALEN);
    secp256k1_multiset_add   (ctx, &r1, elements[0], DATALEN);
    secp256k1_multiset_add   (ctx, &r1, elements[1], DATALEN);
    secp256k1_multiset_add   (ctx, &r1, elements[1], DATALEN);

    secp256k1_multiset_add   (ctx, &r2, elements[0], DATALEN);
    secp256k1_multiset_add   (ctx, &r2, elements[1], DATALEN);

    CHECK_NOTEQUAL(&r1, &r2); /* M(0,0,1,1)!=M(0,1) */

    secp256k1_multiset_add   (ctx, &r2, elements[0], DATALEN);
    secp256k1_multiset_add   (ctx, &r2, elements[1], DATALEN);
    CHECK_EQUAL(&r1, &r2); /* M(0,0,1,1)!=M(0,0,1,1) */

    secp256k1_multiset_add   (ctx, &r1, elements[0], DATALEN);
    secp256k1_multiset_add   (ctx, &r1, elements[1], DATALEN);
    secp256k1_multiset_add   (ctx, &r3, elements[0], DATALEN);
    secp256k1_multiset_add   (ctx, &r3, elements[1], DATALEN);

    secp256k1_multiset_combine(ctx, &r2, &r3);
    CHECK_EQUAL(&r1, &r2); /* M(0,0,0,1,1,1)!=M(0,0,1,1)+M(0,1) */
}

void test_remove_nonexistant(void) {
    secp256k1_multiset empty, r1;
    secp256k1_multiset_init(ctx, &empty);
    secp256k1_multiset_init(ctx, &r1);

    secp256k1_multiset_remove(ctx, &r1, elements[0], DATALEN);
    CHECK_NOTEQUAL(&empty,&r1);
    secp256k1_multiset_add(ctx, &r1, elements[0], DATALEN);
    CHECK_EQUAL(&empty,&r1);
}

void test_empty(void) {

    /* Test if empty set properties hold */

    secp256k1_multiset empty, r1,r2;

    secp256k1_multiset_init(ctx, &empty);
    secp256k1_multiset_init(ctx, &r1);
    secp256k1_multiset_init(ctx, &r2);

    CHECK_EQUAL(&empty,&r1); /* M()==M() */

    /* empty + empty = empty */
    secp256k1_multiset_combine(ctx, &r1, &r2);
    CHECK_EQUAL(&empty, &r1); /* M()+M()==M() */
}

void test_testvector(void) {
    /* Tests known values from the specification */

    const unsigned char d1[117] = {
        0x98,0x20,0x51,0xfd,0x1e,0x4b,0xa7,0x44,0xbb,0xbe,0x68,0x0e,0x1f,0xee,0x14,0x67,0x7b,0xa1,0xa3,0xc3,0x54,0x0b,0xf7,0xb1,0xcd,0xb6,0x06,0xe8,0x57,0x23,0x3e,0x0e,
        0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,0x00,0xf2,0x05,0x2a,0x01,0x00,0x00,0x00,0x43,0x41,0x04,0x96,0xb5,0x38,0xe8,0x53,0x51,0x9c,0x72,0x6a,0x2c,0x91,0xe6,
        0x1e,0xc1,0x16,0x00,0xae,0x13,0x90,0x81,0x3a,0x62,0x7c,0x66,0xfb,0x8b,0xe7,0x94,0x7b,0xe6,0x3c,0x52,0xda,0x75,0x89,0x37,0x95,0x15,0xd4,0xe0,0xa6,0x04,0xf8,0x14,
        0x17,0x81,0xe6,0x22,0x94,0x72,0x11,0x66,0xbf,0x62,0x1e,0x73,0xa8,0x2c,0xbf,0x23,0x42,0xc8,0x58,0xee,0xac };

    const unsigned char d2[117] = {
        0xd5,0xfd,0xcc,0x54,0x1e,0x25,0xde,0x1c,0x7a,0x5a,0xdd,0xed,0xf2,0x48,0x58,0xb8,0xbb,0x66,0x5c,0x9f,0x36,0xef,0x74,0x4e,0xe4,0x2c,0x31,0x60,0x22,0xc9,0x0f,0x9b,
        0x00,0x00,0x00,0x00,0x02,0x00,0x00,0x00,0x01,0x00,0xf2,0x05,0x2a,0x01,0x00,0x00,0x00,0x43,0x41,0x04,0x72,0x11,0xa8,0x24,0xf5,0x5b,0x50,0x52,0x28,0xe4,0xc3,0xd5,
        0x19,0x4c,0x1f,0xcf,0xaa,0x15,0xa4,0x56,0xab,0xdf,0x37,0xf9,0xb9,0xd9,0x7a,0x40,0x40,0xaf,0xc0,0x73,0xde,0xe6,0xc8,0x90,0x64,0x98,0x4f,0x03,0x38,0x52,0x37,0xd9,
        0x21,0x67,0xc1,0x3e,0x23,0x64,0x46,0xb4,0x17,0xab,0x79,0xa0,0xfc,0xae,0x41,0x2a,0xe3,0x31,0x6b,0x77,0xac };

    const unsigned char d3[117] = {
        0x44,0xf6,0x72,0x22,0x60,0x90,0xd8,0x5d,0xb9,0xa9,0xf2,0xfb,0xfe,0x5f,0x0f,0x96,0x09,0xb3,0x87,0xaf,0x7b,0xe5,0xb7,0xfb,0xb7,0xa1,0x76,0x7c,0x83,0x1c,0x9e,0x99,
        0x00,0x00,0x00,0x00,0x03,0x00,0x00,0x00,0x01,0x00,0xf2,0x05,0x2a,0x01,0x00,0x00,0x00,0x43,0x41,0x04,0x94,0xb9,0xd3,0xe7,0x6c,0x5b,0x16,0x29,0xec,0xf9,0x7f,0xff,
        0x95,0xd7,0xa4,0xbb,0xda,0xc8,0x7c,0xc2,0x60,0x99,0xad,0xa2,0x80,0x66,0xc6,0xff,0x1e,0xb9,0x19,0x12,0x23,0xcd,0x89,0x71,0x94,0xa0,0x8d,0x0c,0x27,0x26,0xc5,0x74,
        0x7f,0x1d,0xb4,0x9e,0x8c,0xf9,0x0e,0x75,0xdc,0x3e,0x35,0x50,0xae,0x9b,0x30,0x08,0x6f,0x3c,0xd5,0xaa,0xac };

    /* Expected resulting multisets */
    const unsigned char exp_empty[32]  = { 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 };
    const unsigned char exp_m1[32]     = { 0xf8,0x83,0x19,0x59,0x33,0xa6,0x87,0x17,0x0c,0x34,0xfa,0x1a,0xde,0xc6,0x6f,0xe2,0x86,0x18,0x89,0x27,0x9f,0xb1,0x2c,0x03,0xa3,0xfb,0x0c,0xa6,0x8a,0xd8,0x78,0x93 };
    const unsigned char exp_m2[32]     = { 0xef,0x85,0xd1,0x23,0xa1,0x5d,0xa9,0x5d,0x8a,0xff,0x92,0x62,0x3a,0xd1,0xe1,0xc9,0xfc,0xda,0x3b,0xaa,0x80,0x1b,0xd4,0x0b,0xc5,0x67,0xa8,0x3a,0x6f,0xdc,0xf3,0xe2 };
    const unsigned char exp_m3[32]     = { 0xcf,0xad,0xf4,0x0f,0xc0,0x17,0xfa,0xff,0x5e,0x04,0xcc,0xc0,0xa2,0xfa,0xe0,0xfd,0x61,0x6e,0x42,0x26,0xdd,0x7c,0x03,0xb1,0x33,0x4a,0x7a,0x61,0x04,0x68,0xed,0xff };
    const unsigned char exp_m1m2[32]   = { 0xfa,0xba,0xfd,0x38,0xd0,0x73,0x70,0x98,0x2a,0x34,0x54,0x7d,0xaf,0x5b,0x57,0xb8,0xa4,0x39,0x86,0x96,0xd6,0xfd,0x22,0x94,0x78,0x8a,0xbd,0xa0,0x7b,0x1f,0xaa,0xaf };
    const unsigned char exp_m1m2m3[32] = { 0x1c,0xbc,0xcd,0xa2,0x3d,0x7c,0xe8,0xc5,0xa8,0xb0,0x08,0x00,0x8e,0x17,0x38,0xe6,0xbf,0x9c,0xff,0xb1,0xd5,0xb8,0x6a,0x92,0xa4,0xe6,0x2b,0x53,0x94,0xa6,0x36,0xe2 };

    unsigned char m0[32],m1[32],m2[32],m3[32],m1m2[32],m1m2m3[32];
    secp256k1_multiset r0,r1,r2,r3;

    secp256k1_multiset_init(ctx, &r0);
    secp256k1_multiset_init(ctx, &r1);
    secp256k1_multiset_init(ctx, &r2);
    secp256k1_multiset_init(ctx, &r3);

    secp256k1_multiset_add    (ctx, &r1, d1, sizeof(d1));
    secp256k1_multiset_add    (ctx, &r2, d2, sizeof(d2));
    secp256k1_multiset_add    (ctx, &r3, d3, sizeof(d3));

    secp256k1_multiset_finalize(ctx, m0, &r0);
    secp256k1_multiset_finalize(ctx, m1, &r1);
    secp256k1_multiset_finalize(ctx, m2, &r2);
    secp256k1_multiset_finalize(ctx, m3, &r3);

    secp256k1_multiset_combine(ctx, &r1, &r2);
    secp256k1_multiset_finalize(ctx, m1m2, &r1);

    secp256k1_multiset_combine(ctx, &r1, &r3);
    secp256k1_multiset_finalize(ctx, m1m2m3, &r1);

    CHECK(memcmp(m0,exp_empty,32)==0);
    CHECK(memcmp(m1,exp_m1,32)==0);
    CHECK(memcmp(m2,exp_m2,32)==0);
    CHECK(memcmp(m3,exp_m3,32)==0);
    CHECK(memcmp(m1m2,exp_m1m2,32)==0);
    CHECK(memcmp(m1m2m3,exp_m1m2m3,32)==0);
}

void run_multiset_tests(void) {

    initdata();
    test_unordered();
    test_combine();
    test_remove();
    test_empty();
    test_duplicate();
    test_remove_nonexistant();
    test_testvector();
}

#endif /* _SECP256K1_MODULE_MULTISET_TESTS_ */
