#include "io.h"

inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %[src_port], %[dest_reg]" 
        : [dest_reg] "=a"(ret) 
        : [src_port] "Nd"(port) 
    );
    return ret;
}

inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %[data], %[dest_port]" 
        : // No outputs
        : [data] "a"(value), [dest_port] "Nd"(port) 
    );
}

inline void io_wait(void) {
    // We can do an IO operation on any unused port
    outb(0x80, 0);
}
