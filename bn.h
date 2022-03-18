#include <linux/device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#ifndef DIV_ROUNDUP
#define DIV_ROUNDUP(x, len) (((x) + (len) -1) / (len))
#endif
#ifndef SWAP
#define SWAP(x, y)           \
    do {                     \
        typeof(x) __tmp = x; \
        x = y;               \
        y = __tmp;           \
    } while (0)
#endif

typedef struct _bn {
    unsigned int *number;
    unsigned int size;
    int sign;
} bn;
bn *bn_alloc(size_t size);
char *result(bn *bnfib);
char *bn_to_string(bn src);
int bn_free(bn *src);
static int bn_resize(bn *src, size_t size);
void bn_swap(bn *a, bn *b);
static int bn_msb(const bn *src);
int bn_cmp(const bn *a, const bn *b);
static void bn_do_add(const bn *a, const bn *b, bn *c);
static void bn_do_sub(const bn *a, const bn *b, bn *c);
void bn_add(const bn *a, const bn *b, bn *c);
void bn_sub(const bn *a, const bn *b, bn *c);
void bn_mult(const bn *a, const bn *b, bn *c);
void bn_lshift(const bn *src, size_t shift, bn *dest);
int bn_cpy(bn *dest, bn *src);
