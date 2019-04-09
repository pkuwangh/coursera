#ifndef SYSTEM_SPECIFIC_H
#define SYSTEM_SPECIFIC_H

#if (defined (__i386__) || defined (__x86_64__))
#define PAUSE __asm__ __volatile__ ("pause")
#else
#define PAUSE
#endif

#endif // SYSTEM_SPECIFIC_H
