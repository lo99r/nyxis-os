#ifndef PANIC_C
#define PANIC_C

static inline void kernel_panic_simple(
    const char *Message,
    Nstatus error
);

#endif // PANIC_C
