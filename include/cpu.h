#ifndef __CPU_DOT_H__
#define __CPU_DOT_H__

/* in startup.s */
void cpu_cache_disable(void);
void cpu_cache_flush(void);
void cpu_cache_invalidate(void);
void cpu_interrupts_on(void);
void cpu_interrupts_off(void);
void machine_execute(void *entry_vector);

#if defined(__mc68020__) || defined(__mc68030__) || defined(__mc68040__) || defined(__mc68060__)
#define CPU_68020_OR_LATER
#else
#define CPU_68010_OR_EARLIER
#endif

#endif
