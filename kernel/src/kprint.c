#include "kprint.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#define BACKENDS_SIZE 4

static size_t backend_count = 0;
static putc_fn backends[BACKENDS_SIZE] = { NULL };

int kprint_add_output(putc_fn output) {
    if (backend_count >= BACKENDS_SIZE) {
        return 1;
    }

    if (output == NULL) {
        return 2;
    }

    backends[backend_count++] = output;
    return 0;
}

static int kprint_char(char c) {
    for (size_t i = 0; i < backend_count; i++) {
        backends[i](c);
    }

    return 1;
}

static int kprint_binary(int number) {
    const int total_bits = sizeof(number) * 8;
    for (int i = total_bits - 1; i >= 0; i--) {
        int bit = (number >> i) & 1;
        if (bit) {
            kprint_char('1');
        } else {
            kprint_char('0');
        }
    }

    return total_bits;
}

static int kprint_decimal(int number) {
    // UINT64_MAX (18446744073709551615UL) has 20 decimal digits,
    // so we need space for at most 20 characters.
    char digits[20] = {0};
    int current_digit = 0;

    if (number == 0) {
        return kprint_char('0');
    }

    int bytes = 0;
    if (number < 0) {
        bytes += kprint_char('-');
        number *= -1;
    }

    // Append the digits from right to left.
    while (number > 0) {
        int digit = number % 10;
        digits[current_digit++] = '0' + digit;
        number /= 10;
    }

    // Print the digits in reverse order.
    while (current_digit--) {
        bytes += kprint_char(digits[current_digit]);
    }

    return bytes;
}

static int kprint_pointer(void* pointer) {
    int bytes = 0;
    bytes +=  kprint_char('0');
    bytes +=  kprint_char('x');

    const int total_bytes = sizeof(pointer);
    const int hex_digits = total_bytes * 2;
    const char* hex_table = "0123456789abcdef";
    uintptr_t number = (uintptr_t)pointer;
    for (int i = hex_digits - 1; i >= 0; i--) {
        int digit = (number >> (i * 4)) & 0xf;
        bytes += kprint_char(hex_table[digit]);
    }

    return bytes;
}

static int kprint_string(const char* string) {
    int bytes = 0;
    if (string == NULL) {
        return 0;
    }

    while (*string) {
        bytes += kprint_char(*string++);
    }

    return bytes;
}

static int kprint_hex(int number) {
    const int total_bytes = sizeof(number);
    const int hex_digits = total_bytes * 2;
    const char* hex_table = "0123456789abcdef";
    for (int i = hex_digits - 1; i >= 0; i--) {
        int digit = (number >> (i * 4)) & 0xf;
        kprint_char(hex_table[digit]);
    }

    return hex_digits;
}

__attribute__((format(printf, 1, 2)))
int kprintf(const char* format, ...) {
    int bytes = 0;
    va_list args;
    
    for (va_start(args, format); *format != '\0'; format++) {
        if (*format == '%') {
            // Move past the %
            format++;

            switch (*format) {
                case 'b': {
                    unsigned int number = va_arg(args, unsigned int);
                    bytes += kprint_binary(number);
                    break;
                }
                case 'c': {
                    char c = (char)va_arg(args, int);
                    bytes += kprint_char(c);
                    break;
                }
                case 'd':
                case 'i': {
                    int number = va_arg(args, int);
                    bytes += kprint_decimal(number);
                    break;
                }
                case 'p': {
                    void* pointer = va_arg(args, void*);
                    bytes += kprint_pointer(pointer);
                    break;
                }
                case 's': {
                    char* string = va_arg(args, char*);
                    bytes += kprint_string(string);
                    break;
                }
                case 'x': {
                    unsigned int number = va_arg(args, unsigned int);
                    bytes += kprint_hex(number);
                    break;
                }
                case '%': {
                    // Literal %
                    bytes += kprint_char('%');
                    break;
                }
                default: {
                    // Unknown specifier, print literally
                    bytes += kprint_char('%');
                    bytes += kprint_char(*format);
                    break;
                }
            }
        } else {
            // Normal characters
            bytes += kprint_char(*format);
        }
    }

    va_end(args);
    return bytes;
}
