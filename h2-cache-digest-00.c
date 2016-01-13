/*
 * Copyright (c) 2016 Kazuho Oku
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */
#include <assert.h>
#include <getopt.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include "golombset.h"
#include "picohash.h"

static unsigned log2_ceil(unsigned v)
{
    if (v <= 2)
        return 1;
    return sizeof(unsigned) * 8 - __builtin_clz(v - 1);
}

static uint64_t calc_key(const char *uri, unsigned num_bits)
{
    picohash_ctx_t hctx;
    unsigned char digest[PICOHASH_SHA256_DIGEST_LENGTH];
    uint64_t t;
    int i;

    picohash_init_sha256(&hctx);
    picohash_update(&hctx, uri, strlen(uri));
    picohash_final(&hctx, digest);

    t = 0;
    for (i = 0; i != sizeof(t); ++i)
        t = t * 256 + digest[i];
    return t >> (sizeof(t) * 8 - num_bits);
}

static int cmp_uint64(const void *_x, const void *_y)
{
    const uint64_t *x = _x, *y = _y;
    if (*x < *y)
        return -1;
    else if (*x > *y)
        return 1;
    return 0;
}

static int encode(const char **urls, size_t num_urls, unsigned P_log2)
{
    unsigned N_log2 = log2_ceil(num_urls);
    uint64_t keys[num_urls];
    golombset_encoder_t ctx = {};
    unsigned char encoded[1024];

    // build list of keys
    for (size_t i = 0; i != num_urls; ++i)
        keys[i] = calc_key(urls[i], N_log2 + P_log2);
    qsort(keys, num_urls, sizeof(keys[0]), cmp_uint64);

    // encode
    ctx.dst = encoded;
    ctx.dst_max = encoded + sizeof(encoded);
    ctx.fixed_bits = P_log2;
    if (golombset_encode_bits(&ctx, N_log2, 8) != 0 || golombset_encode_bits(&ctx, P_log2, 8) != 0 ||
        golombset_encode(&ctx, keys, num_urls, 0) != 0) {
        fprintf(stderr, "failed to encode bits (buffer too small)\n");
        return 111;
    }
    size_t encoded_len = ctx.dst - encoded;

    // emit to STDOUT
    fwrite(encoded, 1, encoded_len, stdout);

    return 0;
}

static int read_and_decode(uint64_t *keys, size_t *nkeys, unsigned *N_log2, unsigned *P_log2)
{
    unsigned char encoded[1024];
    ssize_t encoded_len = fread(encoded, 1, sizeof(encoded), stdin);
    if (encoded_len <= 0) {
        fprintf(stderr, "failed to read encoded bits from STDIN\n");
        return -1;
    }

    uint64_t tmp;
    golombset_decoder_t ctx = {};

    ctx.src = encoded;
    ctx.src_max = encoded + encoded_len;
    if (golombset_decode_bits(&ctx, &tmp, 8) != 0)
        goto DecodeFailure;
    *N_log2 = (unsigned)tmp;
    if (golombset_decode_bits(&ctx, &tmp, 8) != 0)
        goto DecodeFailure;
    *P_log2 = (unsigned)tmp;
    ctx.fixed_bits = *P_log2;
    if (golombset_decode(&ctx, keys, nkeys, 0) != 0)
        goto DecodeFailure;

    return 0;
DecodeFailure:
    fprintf(stderr, "failed to decode bits\n");
    return -1;
}

static int decode(void)
{
    uint64_t keys[1024];
    size_t nkeys = sizeof(keys) / sizeof(keys[0]);
    unsigned N_log2, P_log2;

    if (read_and_decode(keys, &nkeys, &N_log2, &P_log2) != 0)
        return 111;

    printf("N_log2: %u\n", N_log2);
    printf("P_log2: %u\n", P_log2);
    printf("Values: ");
    for (size_t i = 0; i != nkeys; ++i) {
        if (i != 0)
            printf(", ");
        printf("%" PRIu64, keys[i]);
    }
    printf("\n");

    return 0;
}

static int push(const char **urls, size_t num_urls)
{
    uint64_t keys[1024];
    size_t nkeys = sizeof(keys) / sizeof(keys[0]);
    unsigned N_log2, P_log2;

    if (read_and_decode(keys, &nkeys, &N_log2, &P_log2) != 0)
        return 111;
    for (size_t i = 0; i != num_urls; ++i) {
        printf("%s: ", urls[i]);
        uint64_t key = calc_key(urls[i], N_log2 + P_log2);
        if (bsearch(&key, &keys, nkeys, sizeof(keys[0]), cmp_uint64) != NULL) {
            printf("should NOT push; already cached\n");
        } else {
            printf("should push; not cached\n");
            // TODO use the result of previous binary search to determine the insert position
            keys[nkeys++] = key;
            qsort(keys, nkeys, sizeof(keys[0]), cmp_uint64);
        }
    }

    return 0;
}

static void usage(void)
{
    printf("h2-cache-digest-00\n"
           "\n"
           "SYNOPSIS:\n"
           "  %% h2-cache-digest-00 --encode URL1 URL2 URL3 ... > digest.bin\n"
           "  %% h2-cache-digest-00 --decode < digest.bin\n"
           "  %% h2-cache-digest-00 --check URL1 URL2 URL3 ... < digest.bin\n"
           "\n");
}

int main(int argc, char **argv)
{
    static const struct option longopts[] = {{"encode", no_argument, NULL, 'e'},
                                             {"decode", no_argument, NULL, 'd'},
                                             {"push", no_argument, NULL, 'p'},
                                             {"p-log2", required_argument, NULL, 'P'},
                                             {"help", no_argument, NULL, 'h'},
                                             {}};

    enum { MODE_UNKNOWN, MODE_ENCODE, MODE_DECODE, MODE_PUSH } mode = MODE_UNKNOWN;
    int opt_ch;
    unsigned P_log2 = 8;

    while ((opt_ch = getopt_long(argc, argv, "edpP:h", longopts, NULL)) != -1) {
#define ASSERT_MODE_IS_UNSET()                                                                                                     \
    do {                                                                                                                           \
        if (mode != MODE_UNKNOWN) {                                                                                                \
            fprintf(stderr, "only one of --encode,--decode,--check can be specified");                                             \
            return 111;                                                                                                            \
        }                                                                                                                          \
    } while (0)
        switch (opt_ch) {
        case 'e':
            ASSERT_MODE_IS_UNSET();
            mode = MODE_ENCODE;
            break;
        case 'd':
            ASSERT_MODE_IS_UNSET();
            mode = MODE_DECODE;
            break;
        case 'p':
            ASSERT_MODE_IS_UNSET();
            mode = MODE_PUSH;
            break;
        case 'P':
            if (sscanf(optarg, "%u", &P_log2) != 1) {
                fprintf(stderr, "failed to parse argument: -p %s\n", optarg);
                return 111;
            }
            break;
        default:
            usage();
            return 0;
        }
#undef ASSERT_MODE_IS_UNSET
    }
    argc -= optind;
    argv += optind;

    switch (mode) {
    case MODE_ENCODE:
        return encode((const char **)argv, argc, P_log2);
    case MODE_DECODE:
        return decode();
    case MODE_PUSH:
        return push((const char **)argv, argc);
    default:
        fprintf(stderr, "one of --encode,--decode,--check MUST be specified\n");
        return 111;
    }
}
