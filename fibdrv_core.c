#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>

#include "bn.h"
#include "xs.h"

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("National Cheng Kung University, Taiwan");
MODULE_DESCRIPTION("Fibonacci engine driver");
MODULE_VERSION("0.1");

#define DEV_FIBONACCI_NAME "fibonacci"

/* MAX_LENGTH is set to 92 because
 * ssize_t can't fit the number > 92
 */
#define MAX_LENGTH 500

static dev_t fib_dev = 0;
static struct cdev *fib_cdev;
static struct class *fib_class;
static DEFINE_MUTEX(fib_mutex);
static ktime_t kt;

#define XOR_SWAP(a, b, type) \
    do {                     \
        type *__c = (a);     \
        type *__d = (b);     \
        *__c ^= *__d;        \
        *__d ^= *__c;        \
        *__c ^= *__d;        \
    } while (0)

static void __swap(void *a, void *b, size_t size)
{
    if (a == b)
        return;

    switch (size) {
    case 1:
        XOR_SWAP(a, b, char);
        break;
    case 2:
        XOR_SWAP(a, b, short);
        break;
    case 4:
        XOR_SWAP(a, b, unsigned int);
        break;
    case 8:
        XOR_SWAP(a, b, unsigned long);
        break;
    default:
        /* Do nothing */
        break;
    }
}
static void reverse_str(char *str, size_t n)
{
    for (int i = 0; i < (n >> 1); i++)
        __swap(&str[i], &str[n - i - 1], sizeof(char));
}
static void string_number_add(xs *a, xs *b, xs *out)
{
    char *data_a, *data_b;
    size_t size_a, size_b;
    int i, carry = 0;
    int sum;

    /*
     * Make sure the string length of 'a' is always greater than
     * the one of 'b'.
     */
    if (xs_size(a) < xs_size(b))
        __swap((void *) &a, (void *) &b, sizeof(void *));

    data_a = xs_data(a);
    data_b = xs_data(b);

    size_a = xs_size(a);
    size_b = xs_size(b);

    reverse_str(data_a, size_a);
    reverse_str(data_b, size_b);

    char buf[size_a + 2];

    for (i = 0; i < size_b; i++) {
        sum = (data_a[i] - '0') + (data_b[i] - '0') + carry;
        buf[i] = '0' + sum % 10;
        carry = sum / 10;
    }

    for (i = size_b; i < size_a; i++) {
        sum = (data_a[i] - '0') + carry;
        buf[i] = '0' + sum % 10;
        carry = sum / 10;
    }

    if (carry)
        buf[i++] = '0' + carry;

    buf[i] = '\0';

    reverse_str(buf, i);

    /* Restore the original string */
    reverse_str(data_a, size_a);
    reverse_str(data_b, size_b);

    if (out)
        *out = *xs_tmp(buf);
}
static long long fib_sequence(long long k, char __user *buf)
{
    /* FIXME: C99 variable-length array (VLA) is not allowed in Linux kernel. */
    xs f[k + 2];
    int i, n;

    f[0] = *xs_tmp("0");
    f[1] = *xs_tmp("1");

    for (i = 2; i <= k; i++)
        string_number_add(&f[i - 1], &f[i - 2], &f[i]);
    n = xs_size(&f[k]);
    if (copy_to_user(buf, xs_data(&f[k]), n))
        return -EFAULT;

    for (i = 0; i <= k; i++)
        xs_free(&f[i]);
    return n;
}
static unsigned long long fib_fast_double_iterative(long long k)
{  // F(2k+1) = F(k)^2 + F(k+1)^2
    /* F(2k+2) =  F(2k) +  F(2k+1) */
    if (k < 2) {
        return k;
    }
    long long f[2] = {0, 1}, ft[2];
    // int non_digital=sizeof(void *)*8-__builtin_clz(k);
    for (unsigned int i = 1U << 31; i; i >>= 1) {
        ft[0] = 2 * f[0] * f[1] - f[0] * f[0];
        ft[1] = f[0] * f[0] + f[1] * f[1];
        if (k & i) {
            f[0] = ft[1];
            f[1] = ft[0] + ft[1];
        } else {
            f[0] = ft[0];
            f[1] = ft[1];
        }
    }
    return f[0];
}
static unsigned long long fib_fast_double_bn(long long k, char __user *buf)
{
    bn *b1 = bn_alloc(1);
    bn *b2 = bn_alloc(1);
    b1->number[0] = 0;
    b2->number[0] = 1;
    bn *k1 = bn_alloc(1), *k2 = bn_alloc(1);
    for (unsigned int i = 1U << 32 - __builtin_clz(k); i; i >>= 1) {
        bn_cpy(k1, b2);
        bn_lshift(k1, 1, k1);
        bn_sub(k1, b1, k1);
        bn_mult(k1, b1, k1);
        /* F(2k+1) = F(k)^2 + F(k+1)^2 */
        bn_mult(b1, b1, b1);
        bn_mult(b2, b2, b2);
        bn_cpy(k2, b1);
        bn_add(k2, b2, k2);
        if (k & i) {
            bn_cpy(b1, k2);
            bn_cpy(b2, k1);
            bn_add(b2, k2, b2);
        } else {
            bn_cpy(b1, k1);
            bn_cpy(b2, k2);
        }
    }
    /*  char *h=result(b1);
      if (copy_to_user(buf,h,strlen(h)))
          return -EFAULT;*/
    bn_free(b2);
    bn_free(k1);
    bn_free(k2);
    bn_free(b1);
    /* int hlen=strlen(h);
     free(h);
     return hlen;*/
    return 0;
}
static unsigned long long fib_no_cahce_recursive(long long k)
{
    if (k <= 1)
        return k;
    return fib_no_cahce_recursive(k - 1) + fib_no_cahce_recursive(k - 2);
}
static unsigned long long fib_naive_iterative(long long k)
{
    int f0 = 0, f1 = 1;
    for (int i = 1; i <= k; i++) {
        int t = f1;
        f1 = f0 + f1;
        f0 = t;
    }
    return f0;
}

static unsigned long long fib_cahce_recursive(long long k,
                                              unsigned long long *cache)
{
    if (k <= 1)
        return k;
    if (cache[k]) {
        return cache[k];
    }
    cache[k] =
        fib_cahce_recursive(k - 1, cache) + fib_cahce_recursive(k - 2, cache);
    return cache[k];
}
static unsigned long long fib_fast_double_recursive(long long k,
                                                    unsigned long long *cache)
{  // F(2k+1)=F(k+1)^2+F(k)^2
    // f(2n)=2×f(n+1)×f(n)−(f(n))^2
    if (k <= 1)
        return k;
    if (k == 2)
        return 1;
    if (cache[k]) {
        return cache[k];
    }
    int fn1 = (k >> 1) + 1, fn = (k >> 1);
    cache[fn1] = fib_fast_double_recursive(fn1, cache);
    cache[fn] = fib_fast_double_recursive(fn, cache);
    if (k & 1) {
        cache[k] = (cache[fn1] * cache[fn1]) + (cache[fn] * cache[fn]);
    } else {
        cache[k] = (2 * cache[fn1] * cache[fn]) - (cache[fn] * cache[fn]);
    }
    return cache[k];
}
static int fib_open(struct inode *inode, struct file *file)
{
    if (!mutex_trylock(&fib_mutex)) {
        printk(KERN_ALERT "fibdrv is in use");
        return -EBUSY;
    }
    return 0;
}

static int fib_release(struct inode *inode, struct file *file)
{
    mutex_unlock(&fib_mutex);
    return 0;
}
/* calculate the fibonacci number at given offset */
static ssize_t fib_read(struct file *file,
                        char __user *user_buf,
                        size_t size,
                        loff_t *offset)
{
    unsigned long long cache[(*offset) + 1];
    memset(cache, 0, sizeof(long long) * ((*offset) + 1));
    kt = ktime_get();
    (ssize_t) fib_fast_double_bn(*offset, user_buf);
    kt = ktime_sub(ktime_get(), kt);
    return (ssize_t) ktime_to_ns(kt);
    ;
}


/* write operation is skipped */
static ssize_t fib_write(struct file *file,
                         const char *buf,
                         size_t size,
                         loff_t *offset)
{
    // return (ssize_t)ktime_to_ns(kt);
    if (size == 0) {
        fib_fast_double_recursive(*offset, buf);
        fib_cahce_recursive(*offset, buf);
        fib_fast_double_iterative(*offset);
        fib_sequence(*offset, buf);
        fib_naive_iterative(*offset);
    }
    return 1;
}

static loff_t fib_device_lseek(struct file *file, loff_t offset, int orig)
{
    loff_t new_pos = 0;
    switch (orig) {
    case 0: /* SEEK_SET: */
        new_pos = offset;
        break;
    case 1: /* SEEK_CUR: */
        new_pos = file->f_pos + offset;
        break;
    case 2: /* SEEK_END: */
        new_pos = MAX_LENGTH - offset;
        break;
    }

    if (new_pos > MAX_LENGTH)
        new_pos = MAX_LENGTH;  // max case
    if (new_pos < 0)
        new_pos = 0;        // min case
    file->f_pos = new_pos;  // This is what we'll use now
    return new_pos;
}

const struct file_operations fib_fops = {
    .owner = THIS_MODULE,
    .read = fib_read,
    .write = fib_write,
    .open = fib_open,
    .release = fib_release,
    .llseek = fib_device_lseek,
};

static int __init init_fib_dev(void)
{
    int rc = 0;

    mutex_init(&fib_mutex);

    // Let's register the device
    // This will dynamically allocate the major number
    rc = alloc_chrdev_region(&fib_dev, 0, 1, DEV_FIBONACCI_NAME);

    if (rc < 0) {
        printk(KERN_ALERT
               "Failed to register the fibonacci char device. rc = %i",
               rc);
        return rc;
    }

    fib_cdev = cdev_alloc();
    if (fib_cdev == NULL) {
        printk(KERN_ALERT "Failed to alloc cdev");
        rc = -1;
        goto failed_cdev;
    }
    fib_cdev->ops = &fib_fops;
    rc = cdev_add(fib_cdev, fib_dev, 1);

    if (rc < 0) {
        printk(KERN_ALERT "Failed to add cdev");
        rc = -2;
        goto failed_cdev;
    }

    fib_class = class_create(THIS_MODULE, DEV_FIBONACCI_NAME);

    if (!fib_class) {
        printk(KERN_ALERT "Failed to create device class");
        rc = -3;
        goto failed_class_create;
    }

    if (!device_create(fib_class, NULL, fib_dev, NULL, DEV_FIBONACCI_NAME)) {
        printk(KERN_ALERT "Failed to create device");
        rc = -4;
        goto failed_device_create;
    }
    return rc;
failed_device_create:
    class_destroy(fib_class);
failed_class_create:
    cdev_del(fib_cdev);
failed_cdev:
    unregister_chrdev_region(fib_dev, 1);
    return rc;
}

static void __exit exit_fib_dev(void)
{
    mutex_destroy(&fib_mutex);
    device_destroy(fib_class, fib_dev);
    class_destroy(fib_class);
    cdev_del(fib_cdev);
    unregister_chrdev_region(fib_dev, 1);
}

module_init(init_fib_dev);
module_exit(exit_fib_dev);
