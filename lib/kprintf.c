/*
 * Copyright (C) 2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#include <phabos/kprintf.h>

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#define BASE_PREFIX_FLAG (1 << 0)
#define LEFT_PAD_0_FLAG  (1 << 1)

struct format_specifier {
    int flags;
    int width;
    int precision;
    int length;
};

void low_putchar(char c);

void kputc(char c)
{
    low_putchar(c);
}

int kputs(const char* str)
{
    int nbyte = 0;

    if (!str)
        return EOF;

    for (; *str != '\0'; str++, nbyte++)
        kputc(*str);

    return nbyte;
}

static int pr_unsigned(struct format_specifier *fs, unsigned num, unsigned base,
                       int recursion)
{
    int size = 1;
    int retval;

    if (base == 0)
        return -EINVAL;

    if (num >= base) {
        retval = pr_unsigned(fs, num / base, base, recursion + 1);
        if (retval < 0)
            return retval;
        size += retval;
    } else {
        if (fs->precision < 0)
            fs->precision = 1;
        for (int i = recursion + fs->precision - 1; i < fs->width; i++)
            kputc(fs->flags & LEFT_PAD_0_FLAG ? '0' : ' ');

        for (int i = recursion; i < fs->precision; i++)
            kputc('0');
    }

    char offset = (char) (num % base);
    if (offset >= 10)
        kputc((char) ('A' + offset - 10));
    else
        kputc((char) ('0' + offset));

    return size;
}

static int print_unsigned_number(struct format_specifier *fs, unsigned num,
                                 unsigned base)
{
    return pr_unsigned(fs, num, base, 1);
}

static int print_signed_number(struct format_specifier *fs, int num)
{
    int size = 1;

    if (num < 0) {
        kputc('-');
        num = -num;
        size++;
    }

    int result = print_unsigned_number(fs, (unsigned) num, 10);
    if (result < 0)
        return result;

    return size + result;
}

static int print_hexa(struct format_specifier *fs, unsigned nbr)
{
    size_t nwrote = 0;
    int retval;

    if (fs->precision == 0 && nbr == 0)
        return 0;

    if (fs->flags & BASE_PREFIX_FLAG) {
        retval = kputs("0x");
        if (retval < 0)
            return retval;
        nwrote += retval;
    }

    return print_unsigned_number(fs, nbr, 16) + nwrote;
}

static int print_octal(struct format_specifier *fs, unsigned nbr)
{
    size_t nwrote = 0;

    if (fs->precision == 0 && nbr == 0)
        return 0;

    if (fs->flags & BASE_PREFIX_FLAG) {
        nwrote++;
        kputc('o');
    }

    return print_unsigned_number(fs, nbr, 8) + nwrote;
}

static int print_binary(struct format_specifier *fs, unsigned nbr)
{
    size_t nwrote = 0;

    if (fs->precision == 0 && nbr == 0)
        return 0;

    if (fs->flags & BASE_PREFIX_FLAG) {
        nwrote++;
        kputc('b');
    }

    return print_unsigned_number(fs, nbr, 2) + nwrote;
}

static ssize_t katoi(const char** specifier, int *x)
{
    size_t nread = 0;

    if (!specifier || !*specifier || **specifier < '0' || **specifier > '9')
        return -EINVAL;

    *x = 0;

    do {
        *x *= 10;
        *x += **specifier - '0';
        *specifier += 1;
        nread++;
    } while (**specifier >= '0' && **specifier <= '9');

    *specifier -= 1;
    return nread;
}

static ssize_t pr_string(struct format_specifier *fs, const char *str)
{
    size_t nwritten = 0;
    size_t len = strlen(str);

    for (int i = len; i < fs->width; i++)
        kputc(' ');

    if (fs->precision < 0)
        return kputs(str);

    while (*str != '\0' && fs->precision--) {
        kputc(*str++);
        nwritten += 1;
    }

    return nwritten;
}

static int print_from_specifier(const char** specifier, va_list* arg)
{
    ssize_t nread;
    ssize_t long_count = 0;
    struct format_specifier fs = {
        .flags = 0,
        .precision = -1,
        .width = -1,
        .length = sizeof(int),
    };

    if (!specifier || !*specifier || **specifier != '%')
        return -EINVAL;

    do {
        *specifier += 1;

        switch (**specifier) {
            case '0':
                fs.flags |= LEFT_PAD_0_FLAG;

            case '1' ... '9':
                nread = katoi(specifier, &fs.width);
                if (nread < 0)
                    return nread;
                break;

            case '#':
                fs.flags |= BASE_PREFIX_FLAG;
                break;

            case 'l':
                if (long_count++ == 1)
                    fs.length *= 2; // let's not support 8/16 bits machines.
                break;

            case 'z':
                break;

            case 'h':
                fs.length /= 2;
                if (fs.length == 0)
                    return -EINVAL;
                break;

            case '.':
                *specifier += 1;
                nread = katoi(specifier, &fs.precision);
                if (nread < 0)
                    return nread;
                break;

            case '%':
                kputc('%');
                return 1;

            case 's':
                return pr_string(&fs, va_arg(*arg, const char*));

            case 'c':
                kputc((char) va_arg(*arg, int));
                return 1;

            case 'd':
                return print_signed_number(&fs, va_arg(*arg, int));

            case 'u':
                return print_unsigned_number(&fs, va_arg(*arg, unsigned), 10);

            case 'o':
                return print_octal(&fs, va_arg(*arg, unsigned));

            case 'x':
            case 'X':
                return print_hexa(&fs, va_arg(*arg, unsigned));

            case 'p':
                fs.flags = BASE_PREFIX_FLAG;
                fs.precision = sizeof(void*) * 2;
                return print_hexa(&fs, va_arg(*arg, unsigned));

            case 'b':
                return print_binary(&fs, va_arg(*arg, unsigned));

            default:
                return -EINVAL;
        }
    } while (*specifier);

    return -EINVAL;
}

int kprintf(const char* format, ...)
{
    int num_byte_printed = 0;
    int result = 0;
    va_list vl;

    if (!format)
        return -EINVAL;

    va_start(vl, format);

    for (; *format != '\0'; format++) {
        if (*format == '%') {
            result = print_from_specifier(&format, &vl);
            if (result < 0)
                return result;
            num_byte_printed += result;
        } else {
            kputc(*format);
            num_byte_printed++;
        }
    }

    va_end(vl);

    return num_byte_printed;
}

int kvprintf(const char *format, va_list ap)
{
    int num_byte_printed = 0;
    int result = 0;

    if (!format)
        return -EINVAL;

    for (; *format != '\0'; format++) {
        if (*format == '%') {
            result = print_from_specifier(&format, &ap);
            if (result < 0)
                return result;
            num_byte_printed += result;
        } else {
            kputc(*format);
            num_byte_printed++;
        }
    }

    return num_byte_printed;
}
