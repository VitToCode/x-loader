/*
 *  Copyright (C) 2016 Ingenic Semiconductor Co.,Ltd
 *
 *  X1000 series bootloader for u-boot/rtos/linux
 *
 *  Zhang YanMing <yanming.zhang@ingenic.com, jamincheung@126.com>
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under  the terms of the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the License, or (at your
 *  option) any later version.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <common.h>

#define cache_op(op, addr)      \
    __asm__ __volatile__(       \
        ".set   push\n"     \
        ".set   noreorder\n"    \
        ".set   mips3\n"    \
        "cache  %0, %1\n"   \
        ".set   pop\n"      \
        :           \
        : "i" (op), "R" (*(unsigned char *)(addr)))

#define __sync()                \
    __asm__ __volatile__(           \
        ".set   push\n\t"       \
        ".set   noreorder\n\t"      \
        ".set   mips2\n\t"      \
        "sync\n\t"          \
        ".set   pop"            \
        : /* no output */       \
        : /* no input */        \
        : "memory")

#define __fast_iob()                \
    __asm__ __volatile__(           \
        ".set   push\n\t"       \
        ".set   noreorder\n\t"      \
        "lw $0,%0\n\t"      \
        "nop\n\t"           \
        ".set   pop"            \
        : /* no output */       \
        : "m" (*(int *)0xa0000000)  \
        : "memory")

#define fast_iob()              \
    do {                    \
        __sync();           \
        __fast_iob();           \
    } while (0)

uint32_t __div64_32(uint64_t *n, uint32_t base) {
    uint64_t rem = *n;
    uint64_t b = base;
    uint64_t res, d = 1;
    uint32_t high = rem >> 32;

    /* Reduce the thing a bit first */
    res = 0;
    if (high >= base) {
        high /= base;
        res = (uint64_t) high << 32;
        rem -= (uint64_t) (high*base) << 32;
    }
    while ((int64_t)b > 0 && b < rem) {
        b = b+b;
        d = d+d;
    }

    do {
        if (rem >= b) {
            rem -= b;
            res += d;
        }
        b >>= 1;
        d >>= 1;
    } while (d);

    *n = res;

    return rem;
}

void set_bit(int nr, volatile void * addr)
{
    int mask;
    volatile int    *a = addr;

    a += nr >> 5;
    mask = 1 << (nr & 0x1f);
    *a |= mask;
}

void clear_bit(int nr, volatile void * addr)
{
    int mask;
    volatile int    *a = addr;

    a += nr >> 5;
    mask = 1 << (nr & 0x1f);
    *a &= ~mask;
}

int test_bit(int nr, const volatile void *addr)
{
    return ((1UL << (nr & 31)) & (((const unsigned int *) addr)[nr >> 5])) != 0;
}

void udelay(unsigned long usec) {
    unsigned long loops = usec * (CONFIG_APLL_FREQ / 2);

    __asm__ __volatile__ (
            ".set noreorder \n"
            ".align 3 \n"
            "1:bnez %0, 1b \n"
            "subu %0, 1\n"
            ".set reorder \n"
            : "=r" (loops)
            : "0" (loops)
    );
}

void mdelay(unsigned long msec) {
    while (msec--)
        udelay(1000);
}

__attribute__((noreturn)) void hang() {
    uart_puts("\n### Hang-up - Please reset board ###\n");
    while (1)
        ;
}

__attribute__((noreturn)) void hang_reason(const char* reason) {
    uart_puts(reason);
    hang();
}

void dump_mem_content(uint32_t *src, uint32_t len)
{
    debug("====================\n");
    for(int i = 0; i < len / 4; i++) {
        debug("%x:%x\n", src, *(unsigned int *)src);
        src++;
    }
    debug("====================\n");
}

#ifndef __HOST__
void *memcpy(void *dst, const void *src, unsigned int len) {
    char *ret = dst;
    while (len-- > 0) {
        *ret++ = *((char *)src);
        src++;
    }
    return (void *)ret;
}
#endif

void flush_icache_all(void) {
    uint32_t addr, t = 0;

    __asm__ __volatile__("mtc0 $0, $28"); /* Clear Taglo */
    __asm__ __volatile__("mtc0 $0, $29"); /* Clear TagHi */

    for (addr = CKSEG0; addr < CKSEG0 + CONFIG_SYS_ICACHE_SIZE;
         addr += CONFIG_SYS_CACHELINE_SIZE) {
        cache_op(INDEX_STORE_TAG_I, addr);
    }

    /* invalidate btb */
    __asm__ __volatile__(
        ".set mips32\n\t"
        "mfc0 %0, $16, 7\n\t"
        "nop\n\t"
        "ori %0,2\n\t"
        "mtc0 %0, $16, 7\n\t"
        ".set mips2\n\t"
        :
        : "r" (t));
}

void flush_dcache_all(void) {
    uint32_t addr;

    for (addr = CKSEG0; addr < CKSEG0 + CONFIG_SYS_DCACHE_SIZE; addr +=
            CONFIG_SYS_CACHELINE_SIZE) {
        cache_op(INDEX_WRITEBACK_INV_D, addr);
    }

    fast_iob();
}

void flush_cache_all(void) {
    flush_dcache_all();
    flush_icache_all();
}
