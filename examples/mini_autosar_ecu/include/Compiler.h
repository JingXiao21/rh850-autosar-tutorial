/*
 * Compiler.h
 *
 * [Educational Implementation]
 * Real AUTOSAR counterpart: Compiler.h (AUTOSAR_SWS_CompilerAbstraction). Real generated code uses
 * FUNC()/P2VAR()/... everywhere; this project keeps plain C for readability. The macros below exist
 * only so that code ported from real stacks still compiles. PROJECT RULE: do NOT use FUNC/P2VAR in
 * new code; use the MINI_* macros for GCC attributes.
 */
#ifndef COMPILER_H
#define COMPILER_H

#define _GNU_C_COMPILER 1

#define AUTOMATIC
#define TYPEDEF
#define STATIC           static
#define NULL_PTR         ((void *)0)
#define INLINE           inline
#define LOCAL_INLINE     static inline

#define FUNC(rettype, memclass)                   rettype
#define P2VAR(ptrtype, memclass, ptrclass)        ptrtype *
#define P2CONST(ptrtype, memclass, ptrclass)      const ptrtype *
#define CONSTP2VAR(ptrtype, memclass, ptrclass)   ptrtype * const
#define CONSTP2CONST(ptrtype, memclass, ptrclass) const ptrtype * const
#define CONST(type, memclass)                     const type
#define VAR(type, memclass)                       type

/* ---- project specific GCC attribute wrappers ---- */
#define MINI_WEAK        __attribute__((weak))
#define MINI_USED        __attribute__((used))
#define MINI_NORETURN    __attribute__((noreturn))
#define MINI_NOINLINE    __attribute__((noinline))
#define MINI_ALIGNED(n)  __attribute__((aligned(n)))
#define MINI_SECTION(s)  __attribute__((section(s)))
#define MINI_UNUSED(x)   ((void)(x))

#endif /* COMPILER_H */
