#include "xs.h"

static void xs_allocate_data(xs *x, size_t len, bool reallocate)
{  //        xs_allocate_data(x, x->size, 0);

    size_t n = 1 << x->capacity;  // 1<<7 n=64
    /* Medium string */
    if (len < LARGE_STRING_LEN) {
        x->ptr = reallocate ? krealloc(x->ptr, n, GFP_KERNEL)
                            : kmalloc(n, GFP_KERNEL);
        return;
    }

    /*
     * Large string
     */
    x->is_large_string = 1;

    /* The extra 4 bytes are used to store the reference count */
    x->ptr = reallocate ? krealloc(x->ptr, n + 4, GFP_KERNEL)
                        : kmalloc(n + 4, GFP_KERNEL);

    xs_set_ref_count(x, 1);
}

xs *xs_new(xs *x, const void *p)
{
    // xs_tmp("0");
    //#define xs_tmp(x) xs_new((xs) {{ .space_left = 15 }, "0")

    *x = xs_literal_empty();
    // printk("x->ptr=%p x->size=%p x->capacity=%p x->ptr+4=%p\r\n
    // ",&(x->ptr),(x->size),(x->capacity),x->ptr+4);
    size_t len = strlen(p) + 1;
    if (len > 16) {
        // printk("len>16\r\n");
        x->capacity = ilog2(len) + 1;
        // printk("ilog(len)=%d x->capacity=%d\r\n ",ilog2(len)+1,x->capacity);
        x->size = len - 1;
        x->is_ptr = true;
        xs_allocate_data(x, x->size, 0);
        memcpy(xs_data(x), p, len);
        // printk("x string %d\r\n",x->size);
    } else {
        // printk("len less than 16\r\n");
        memcpy(x->data, p, len);
        x->space_left = 15 - (len - 1);
    }
    return x;
}
/*
void xs_trivia_test(void)
{
    xs string = *xs_tmp("\n foobarbar \n\n\n");
    xs backup_string;

    xs_copy(&backup_string, &string);

    xs_trim(&string, "\n ");

    xs prefix = *xs_tmp("((("), suffix = *xs_tmp(")))");

    xs_concat(&string, &prefix, &suffix);
}*/