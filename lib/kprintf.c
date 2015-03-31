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

#define BASE_PREFIX_FLAG (1 << 0)

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

static int pr_unsigned(int precision, unsigned num, unsigned base,
                       int recursion)
{
    int size = 1;
    int retval;

    if (base == 0)
        return -EINVAL;

    if (num >= base) {
        retval = pr_unsigned(precision, num / base, base, recursion + 1);
        if (retval < 0)
            return retval;
        size += retval;
    } else {
        for (int i = recursion; i < precision; i++)
            kputc('0');
    }

    char offset = (char) (num % base);
    if (offset >= 10)
        kputc((char) ('A' + offset - 10));
    else
        kputc((char) ('0' + offset));

    return size;
}

static int print_unsigned_number(int precision, unsigned num, unsigned base)
{
    return pr_unsigned(precision, num, base, 1);
}

static int print_signed_number(int precision, int num)
{
    int size = 1;

    if (num < 0) {
        kputc('-');
        num = -num;
        size++;
    }

    int result = print_unsigned_number(precision, (unsigned) num, 10);
    if (result < 0)
        return result;

    return size + result;
}

static int print_hexa(int flags, int precision, unsigned nbr)
{
    size_t nwrote = 0;
    int retval;

    if (precision == 0 && nbr == 0)
        return 0;

    if (flags & BASE_PREFIX_FLAG) {
        retval = kputs("0x");
        if (retval < 0)
            return retval;
        nwrote += retval;
    }

    return print_unsigned_number(precision, nbr, 16) + nwrote;
}

static int print_octal(int flags, int precision, unsigned nbr)
{
    size_t nwrote = 0;

    if (precision == 0 && nbr == 0)
        return 0;

    if (flags & BASE_PREFIX_FLAG) {
        nwrote++;
        kputc('o');
    }

    return print_unsigned_number(precision, nbr, 8) + nwrote;
}

static int print_binary(int flags, int precision, unsigned nbr)
{
    size_t nwrote = 0;

    if (precision == 0 && nbr == 0)
        return 0;

    if (flags & BASE_PREFIX_FLAG) {
        nwrote++;
        kputc('b');
    }

    return print_unsigned_number(precision, nbr, 2) + nwrote;
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

static ssize_t pr_string(int precision, const char *str)
{
    size_t nwritten = 0;

    if (precision < 0)
        return kputs(str);

    while (*str != '\0' && precision--) {
        kputc(*str++);
        nwritten += 1;
    }

    return nwritten;
}

static int print_from_specifier(const char** specifier, va_list* arg)
{
    int flags = 0;
    int precision = -1;
    ssize_t nread;

    if (!specifier || !*specifier || **specifier != '%')
        return -EINVAL;

    do {
        *specifier += 1;

        switch (**specifier) {
            case '#':
                flags |= BASE_PREFIX_FLAG;
                break;

            case '.':
                *specifier += 1;
                nread = katoi(specifier, &precision);
                if (nread < 0)
                    return nread;
                break;

            case '%':
                kputc('%');
                return 1;

            case 's':
                return pr_string(precision, va_arg(*arg, const char*));

            case 'c':
                kputc((char) va_arg(*arg, int));
                return 1;

            case 'd':
                return print_signed_number(precision, va_arg(*arg, int));

            case 'u':
                return print_unsigned_number(precision, va_arg(*arg, unsigned),
                                             10);

            case 'o':
                return print_octal(flags, precision, va_arg(*arg, unsigned));

            case 'x':
            case 'X':
                return print_hexa(flags, precision, va_arg(*arg, unsigned));

            case 'p':
                return print_hexa(BASE_PREFIX_FLAG, sizeof(void*) * 2,
                                  va_arg(*arg, unsigned));

            case 'b':
                return print_binary(flags, precision, va_arg(*arg, unsigned));

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
