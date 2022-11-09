#ifndef __ARM_ARCH_H__
#define __ARM_ARCH_H__

#if !defined(__ARM_ARCH__)
# if defined(__CC_ARM)
#  define __ARM_ARCH__ __TARGET_ARCH_ARM
#  if defined(__BIG_ENDIAN)
#   define __ARMEB__
#  else
#   define __ARMEL__
#  endif
# elif defined(__GNUC__)
#  if defined(__aarch64__)
#    define __ARM_ARCH__ 8
#    if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#      define __ARMEB__
#    else
#      define __ARMEL__
#    endif
  /* Why doesn't gcc define __ARM_ARCH__? Instead it defines
   * bunch of below macros. See all_architectires[] table in
   * gcc/config/arm/arm.c. On a side note it defines
   * __ARMEL__/__ARMEB__ for little-/big-endian. */
#  elif	defined(__ARM_ARCH)
#    define __ARM_ARCH__ __ARM_ARCH
#  elif	defined(__ARM_ARCH_8A__)
#    define __ARM_ARCH__ 8
#  elif	defined(__ARM_ARCH_7__)	|| defined(__ARM_ARCH_7A__)	|| \
	defined(__ARM_ARCH_7R__)|| defined(__ARM_ARCH_7M__)	|| \
	defined(__ARM_ARCH_7EM__)
#   define __ARM_ARCH__ 7
#  elif	defined(__ARM_ARCH_6__)	|| defined(__ARM_ARCH_6J__)	|| \
	defined(__ARM_ARCH_6K__)|| defined(__ARM_ARCH_6M__)	|| \
	defined(__ARM_ARCH_6Z__)|| defined(__ARM_ARCH_6ZK__)	|| \
	defined(__ARM_ARCH_6T2__)
#   define __ARM_ARCH__ 6
#  elif	defined(__ARM_ARCH_5__)	|| defined(__ARM_ARCH_5T__)	|| \
	defined(__ARM_ARCH_5E__)|| defined(__ARM_ARCH_5TE__)	|| \
	defined(__ARM_ARCH_5TEJ__)
#   define __ARM_ARCH__ 5
#  elif	defined(__ARM_ARCH_4__)	|| defined(__ARM_ARCH_4T__)
#   define __ARM_ARCH__ 4
#  else
#   error "unsupported ARM architecture"
#  endif
# endif
#endif

#ifdef OPENSSL_FIPSCANISTER
#include <openssl/fipssyms.h>
#endif

#if !__ASSEMBLER__
extern unsigned int OPENSSL_armcap_P;
#endif

/* Even when building for 32-bit ARM, support for aarch64 crypto instructions
 * will be included. */
#define __ARM_MAX_ARCH__ 8

/* ARMV7_NEON is true when a NEON unit is present in the current CPU. */
#define ARMV7_NEON      (1<<0)
/* ARMV7_NEON_FUNCTIONAL is true when the NEON unit doesn't contain subtle bugs.
 * The Poly1305 NEON code is known to trigger bugs in the NEON units of some
 * phones. If this bit isn't set then the Poly1305 NEON code won't be used.
 * See https://code.google.com/p/chromium/issues/detail?id=341598. */
#define ARMV7_NEON_FUNCTIONAL (1 << 10)

#define ARMV7_TICK      (1<<1)
/* ARMV8_AES indicates support for hardware AES instructions. */
#define ARMV8_AES (1 << 2)

/* ARMV8_SHA1 indicates support for hardware SHA-1 instructions. */
#define ARMV8_SHA1 (1 << 3)

/* ARMV8_SHA256 indicates support for hardware SHA-256 instructions. */
#define ARMV8_SHA256 (1 << 4)

/* ARMV8_PMULL indicates support for carryless multiplication. */
#define ARMV8_PMULL (1 << 5)

#endif
