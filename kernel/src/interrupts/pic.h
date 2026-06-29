#ifndef PIC_H
#define PIC_H

#define PIC1 0x20  // IO base address for master PIC
#define PIC2 0xA0  // IO base address for slave PIC
#define PIC1_COMMAND PIC1
#define PIC2_COMMAND PIC2
#define PIC1_DATA (PIC1 + 1)
#define PIC2_DATA (PIC2 + 1)

// Offsets to remap the PICs
#define PIC1_OFFSET 0x20
#define PIC2_OFFSET 0x28

#define ICW1_INIT 0x10  // Initialisation
#define ICW1_ICW4 0x01  // Indicates that ICW4 will be present
#define CASCADE_IRQ 2
#define ICW4_8086 0x01
#define PIC_EOI 0x20  // End-of-interrupt

#endif  // PIC_H
