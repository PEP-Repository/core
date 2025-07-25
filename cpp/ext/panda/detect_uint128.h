#pragma once

#if defined(__clang__)
	#define COMPILER_CLANG ((__clang_major__ * 10000) + (__clang_minor__ * 100) + (__clang_patchlevel__))
#endif
#if defined(__GNUC__)
	#if (__GNUC__ >= 3)
		#define COMPILER_GCC ((__GNUC__ * 10000) + (__GNUC_MINOR__ * 100) + (__GNUC_PATCHLEVEL__))
	#else
		#define COMPILER_GCC ((__GNUC__ * 10000) + (__GNUC_MINOR__ * 100)                        )
	#endif
#endif

#if defined(__amd64__) || defined(__amd64) || defined(__x86_64__ ) || defined(_M_X64)
	#define CPU_64BITS
#endif

#if defined(CPU_64BITS)
	#if defined(COMPILER_CLANG) && (COMPILER_CLANG >= 30100)
		#define HAVE_NATIVE_UINT128
		typedef unsigned __int128 uint128_t;
	#elif defined(COMPILER_GCC) && !defined(HAVE_NATIVE_UINT128)
		#if defined(__SIZEOF_INT128__)
			#define HAVE_NATIVE_UINT128
			typedef unsigned __int128 uint128_t;
		#elif (COMPILER_GCC >= 40400)
			#define HAVE_NATIVE_UINT128
			typedef unsigned uint128_t __attribute__((mode(TI)));
    #endif
  #endif
#endif
