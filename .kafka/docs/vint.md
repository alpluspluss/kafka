# Kafka Kernel Architecture-Independent Interrupt System

The kafka kernel provides a unified, architecture-independent interrupt handling that 
abstracts away the differences between x86_64, Aarch64, RISC-V etc. 
This document describes the implementation and usage of this system.

### Virtual Interrupt Numbers (Vint)

The system uses a 16-bit virtual interrupt numbering scheme that maps conceptual interrupt types to
architecture-specific mechanisms. These are organized into logical categories.

| Category Range | Description            | Examples                          |
| -------------- | ---------------------- | --------------------------------- |
| 0x0000-0x00FF  | Synchronous Exceptions | Page faults, divide-by-zero       |
| 0x0100-0x01FF  | Hardware Interrupts    | Timer, keyboard, storage          | 
| 0x0200-0x02FF  | Software Interrupts    | System calls                      |
| 0x1000-0x1FFF  | Platform Specific      | Architecture-specific extensions  |
| 0x8000-0xFFFF  | User Defined           | Application-specific interrupts   |

The system uses a traits-based approach to implement architecture-specific behaviors to 
maintain consistent APIs within the kernel.

```cpp
template<typename Arch>
struct interrupt_traits
{
    static void init() noexcept;
    static void enable(uint16_t n) noexcept;
    static void disable(uint16_t n) noexcept;
    static void enable() noexcept;
    static void disable() noexcept;
    static void register_handler(vint id, interrupt_handler handler, 
                         void* context, uint8_t priority,
                         int_flags flags) noexcept;
};
```

## Interrupt Mapping Tables

### x86_64

| Virtual                 | Interrupt x86_64 | Vector Description      |
| ----------------------- | ---------------- | ----------------------- |
| EXCEPTION_DIVIDE_ERROR  | 0                | Division by zero        |
| EXCEPTION_DEBUG         | 1                | Debug exception         |
| EXCEPTION_NMI           | 2                | Non-maskable interrupt  | 
| EXCEPTION_BREAKPOINT    | 3                | Breakpoint              |
| EXCEPTION_OVERFLOW      | 4                | Overflow                |
| ...                     | ...              | ...                     |
| IRQ_TIMER               | 32               | PIT/APIC Timer          |
| IRQ_KEYBOARD            | 33               | Keyboard                |
| ...                     | ...              | ...                     |
| SYSCALL                 | 128              | System call (int 0x80)  |


## Common Virtual Interrupts

| Virtual ID     | Description                | x86_64 Vector  |
| -------------- | -------------------------- | -------------- |
| 0x0000         | EXCEPTION_DIVIDE_ERROR     | 0              |
| 0x0001         | EXCEPTION_DEBUG            | 1              | 
| 0x0002         | EXCEPTION_NMI              | 2              |
| 0x0003         | EXCEPTION_BREAKPOINT       | 3              |
| 0x0004         | EXCEPTION_BOUND_RANGE      | 4              |
| 0x0005         | EXCEPTION_INVALID_OPCODE   | 5              |
| 0x0006         | EXCEPTION_PAGE_FAULT       | 6              |

## Hardware Interrupts

| Virtual ID     | Description       | x86_64 Vector  |
| -------------- | ----------------- | -------------- |
| 0x0100         | IRQ_TIMER         | 32             |
| 0x0101         | IRQ_KEYBOARD      | 33             | 
| 0x0104         | IRQ_COM1          | 36             |
| 0x0108         | IRQ_RTC           | 40             |
| 0x010C         | IRQ_MOUSE         | 44             |
| 0x010E         | IRQ_PRIMARY_ATA   | 46             |

## Software Interrupts

| Virtual ID     | Description     | x86_64 Vector   |
| -------------- | --------------- | --------------- |
| 0x0200         | SYSCALL         | 128             |

## Some examples

```cpp
#include <kafka/hal/interrupt.hpp>

/* IDT initialization */
extern "C" void kernel_main()
{
    kfk::interrupt::init();
    kfk::interrupt::enable();
}

/* timer driver */
void timer_handler(void* context) noexcept
{
    auto* counter = static_cast<uint64_t*>(context);
    (*counter)++;
}

void setup_timer() noexcept
{
    static uint64_t tick = 0;
    
    kfk::interrupt::register_handler(
        kfk::vint::IRQ_TIMER,
        timer_handler,
        &tick,
        128,
        kfk::int_flags::EDGE_TRIGGER
    );
    
    kfk::interrupt::enable(kfk::vint::IRQ_TIMER);
}

/* syscall handler */
void syscall_handler(void* context) noexcept
{
    auto* regs = static_cast<RegisterState*>(context);

    switch (regs->rax) /* x86_64 */
    {
        case 1: /* write */
            /* ... */
            break;
        case 2: /* read */
            /* ... */
            break;
        /* ... */
    }
}

void setup_syscalls() noexcept
{
    kfk::interrupt::register_handler(
        kfk::vint::SYSCALL,
        syscall_handler,
        nullptr,
        64, /* high priority */
        kfk::int_flags::NONE
    );
}
```

## Notes

- Higher value means higher priority
- On x86_64, mapped to TPR in APIC
- Higher priority interrupts can preempt lower ones
- Architecture-specific implementations handle the details


For more details, see implementation details.