//  Copyright 2021 Google LLC.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef EXPERIMENTAL_WAYWARDGEEK_RUNE_RUNTIME_RUNE_RUNTIME_H_
#define EXPERIMENTAL_WAYWARDGEEK_RUNE_RUNTIME_RUNE_RUNTIME_H_

// This is the runtime for Rune.  It gets linked with every Rune application.
// It needs to stay small!  It provides basic I/O and big integer support, as
// well as dynamic arrays.  Functions starting with runtime_ are directly
// callable from Rune, since they are declared as extern "C" in package.rn.

#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "cttk.h"  // For cttk_bool.

// Integers larger than this are represented as CTTK bigints.  Otherwise, they
// are passed by value on the stack.
#define runtime_maxNativeIntWidth (sizeof(uint64_t)*8)

#define RN_SIZET_MASK                   \
  (sizeof(size_t) == 4 ? (uint32_t)0x3u \
                       : (sizeof(size_t) == 8 ? (uint32_t)0x7 : UINT32_MAX))
#define RN_SIZET_SHIFT                \
  (sizeof(size_t) == 4 ? (uint32_t)2u \
                       : (sizeof(size_t) == 8 ? (uint32_t)0x3u : UINT32_MAX))
// The max length message generated by the runtime in error messages.
// TODO: Store the list in an runtime_array and allow it to grow dynamically.,
#define RN_MAX_CSTRING 1024u

// These are primitive element types.  They are needed for array comparison.
typedef enum {
  RN_UINT,
  RN_INT,
  RN_FLOAT,
  RN_DOUBLE,
} runtime_type;

// Comparison operator types.
// NOTE: If this changes, be sure to change runtime/package.rn as well.
// LINT.IfChange
typedef enum {
  RN_LT = 0,  // a < b
  RN_LE = 1,  // a <= b
  RN_GT = 2,  // a > b
  RN_GE = 3,  // a >= b
  RN_EQUAL = 4,  // a == b
  RN_NOTEQUAL = 5,  // a != b
} runtime_comparisonType;
// LINT.ThenChange(
//   package.rn)

// These live on the stack or in globals.  It must be the unique reference to
// the array's data on the heap, so it can be updated during heap compaction.
typedef struct {
  size_t *data;
  size_t numElements;
} runtime_array;

// A "word" in this runtime means a size_t.  This 2-word structure is the header
// on the heap preceding an array's data.
typedef struct {
#ifdef RN_DEBUG
  size_t counter;  // Set the value of runtime_arrayCounter when the header is
                   // initialized.
#endif
  bool hasSubArrays: 1;
  size_t allocatedWords : sizeof(size_t) * 8 - 1;
  runtime_array *backPointer;
} runtime_heapHeader;

static inline runtime_array runtime_makeEmptyArray(void) {
  runtime_array array = {NULL, 0};
  return array;
}

// The compiler knows the element size and passes this to array manipulation
// functions.  It can never be > uint64_t, which means either 32 or 64 bits,
// depending on whether we're compiling 32 or 64 bit code.
void runtime_arrayStart(void);
void runtime_arrayStop(void);
void runtime_initArrayOfStringsFromC(runtime_array *array, const uint8_t **vector, size_t len);
void runtime_initArrayOfStringsFromCUTF8(runtime_array *array, const uint8_t **vector, size_t len);
void runtime_allocArray(runtime_array *array, size_t numElements, size_t elementSize,
    bool hasSubArrays);
void runtime_arrayInitCstr(runtime_array *array, const char *text);
void runtime_resizeArray(runtime_array *array, uint64_t numElements, size_t elementSize,
    bool hasSubArrays);
void runtime_copyArray(runtime_array *dest, runtime_array *source, size_t elementSize,
    bool hasSubArrays);
void runtime_moveArray(runtime_array *dest, runtime_array *source);
void runtime_sliceArray(runtime_array *dest, runtime_array *source, uint64_t lower,
    uint64_t upper, size_t elementSize, bool hasSubArrays);
void runtime_freeArray(runtime_array *array);
void runtime_foreachArrayObject(runtime_array *array, void *callback, uint32_t refWidth,
    uint32_t depth);
void runtime_updateArrayBackPointer(runtime_array *array);
void runtime_compactArrayHeap(void);
void runtime_appendArrayElement(runtime_array *array, uint8_t *data, size_t elementSize,
    bool isArray, bool hasSubArrays);
void runtime_concatArrays(runtime_array *dest, runtime_array *source, size_t elementSize,
    bool hasSubArrays);
void runtime_xorStrings(runtime_array *dest, runtime_array *a, runtime_array *b);
void runtime_reverseArray(runtime_array *array, size_t elementSize, bool hasSubArrays);
bool runtime_compareArrays(runtime_comparisonType compareType, runtime_type elementType,
    const runtime_array *a, const runtime_array *b, size_t elementSize,
    bool hasSubArrays, bool secret);
void runtime_memcopy(void *dest, const void *source, size_t len);
// For debugging.
void runtime_printBigint(runtime_array *val);
void runtime_printHexBigint(runtime_array *val);
void runtime_verifyHeap(void);

void runtime_panicCstr(const char *format, ...);

static inline runtime_heapHeader *runtime_getArrayHeader(const runtime_array *array) {
  return ((runtime_heapHeader*)(array->data)) - 1;
}

// I/O: logging and error handling.  For now, all I/O is to stdout and from
// stdin, which matches the communication model of a sealed enclave.
uint8_t readByte(void);
void writeByte(uint8_t c);
void readBytes(runtime_array *array, uint64_t numBytes);
void writeBytes(const runtime_array *array, uint64_t numBytes, uint64_t offset);
void readln(runtime_array *array, uint64_t maxBytes);
void io_getcwd(runtime_array *array);
uint64_t io_file_fopenInternal(runtime_array *fileName, runtime_array *mode);
bool io_file_fcloseInternal(uint64_t ptr);
uint64_t io_file_freadInternal(uint64_t ptr, runtime_array *buf);
bool io_file_fwriteInternal(uint64_t ptr, runtime_array *buf);
void runtime_puts(const runtime_array *string);
void runtime_putsCstr(const char *string);
void runtime_sprintf(runtime_array *array, const runtime_array *format, ...);
void runtime_printf(const char *format, ...);
void runtime_vsprintf(runtime_array *array, const runtime_array *format, va_list ap);
void runtime_throwException(const runtime_array *format, ...);
void runtime_throwExceptionCstr(const char *format, ...);
void runtime_throwOverflow();
void runtime_panic(const runtime_array *format, ...);
void runtime_nativeIntToString(runtime_array *string, uint64_t value, uint32_t base, bool isSigned);
void runtime_bigintToString(runtime_array *string, runtime_array *bigint, uint32_t base);
void runtime_stringToHex(runtime_array *destHexString, const runtime_array *sourceBinString);
void runtime_hexToString(runtime_array *destBinString, const runtime_array *sourceHexString);
uint64_t runtime_stringFind(const runtime_array *haystack, const runtime_array *needle, uint64_t offset);
uint64_t runtime_stringRfind(const runtime_array *haystack, const runtime_array *needle, uint64_t offset);

// Interface to TRNG.  On Linux, this should be a CPRNG initialized from
// /dev/urandom, or even better, using the getrandom syscall.
uint64_t runtime_generateTrueRandomValue(uint32_t width);
void runtime_generateTrueRandomBytes(uint8_t *dest, uint64_t numBytes);

// Small integer exponentiation, with overflow checking.

// Zero memory securely.
static inline void runtime_zeroMemory(uint64_t *p, uint64_t numWords) {
  volatile uint64_t *q = p;
  while (numWords--) {
    *q++ = 0;
  }
}

// Copy memory by uint64_t sized words. |src| and |dest| may overlap, as long as |src| < |dest|.
// src and dest must be uint64_t aligned.
static inline void runtime_copyWords(size_t *dest, const size_t *src,
                               size_t numWords) {
  while (numWords--) {
    *dest++ = *src++;
  }
}

// Convert bytes to words of size size_t, rounding up.
static inline uint64_t runtime_bytesToWords(size_t numBytes) {
  return (numBytes + sizeof(size_t) - 1) >> RN_SIZET_SHIFT;
}

// Multiply the two numbers, and check that the result fits into uint64_t.
// WARNING: NOT constant time!
static inline uint64_t runtime_multCheckForOverflow(size_t a, size_t b) {
  // Check common case where we don't have to do the division.
  size_t res = a * b;
  if (a == 0 ||
      (a >> (sizeof(size_t) << 2) == 0 && b >> (sizeof(size_t) << 2) == 0)) {
    return res;
  }
  if (res / a != b) {
    runtime_throwExceptionCstr("Integer overflow");
  }
  return res;
}

// Add the two numbers, and check that the result fits into uint64_t.
// WARNING: NOT constant time!
static inline size_t runtime_addCheckForOverflow(size_t a, size_t b) {
  size_t sum = a + b;
  if (sum < a || sum < b) {
    runtime_throwExceptionCstr("Integer overflow");
  }
  return sum;
}

// Constant-time Boolean type based on cttk_book.
typedef cttk_bool runtime_bool;

// Constant time bigints, currently based on CTTK.  When Rune is rewritten in
// Rune, we should build a constant time CTTK-like constant-time bigint library
// in Rune.  It should use 32/64 bit arithmetic, and get access to the carry bit
// through LLVM overflow detection intrinsics.
extern const runtime_bool runtime_false;
extern const runtime_bool runtime_true;

// These bits are set in the first word of the bigint.  When we integrate CTTK,
// we should merge this word with the first CTTK word.
#define RN_SECRET_BIT 0x80000000
#define RN_SIGNED_BIT 0x40000000

uint32_t runtime_bigintWidth(const runtime_array *bigint);
bool runtime_bigintSigned(const runtime_array *bigint);
bool runtime_bigintSecret(const runtime_array *bigint);
void runtime_bigintSetSecret(runtime_array *bigint, bool value);
runtime_bool runtime_bigintZero(const runtime_array *a);
runtime_bool runtime_bigintNegative(const runtime_array *a);
void runtime_bigintCast(runtime_array *dest, runtime_array *source, uint32_t newWidth,
    bool isSigned, bool isSecret, bool truncate);
void runtime_bigintSet(runtime_array *dest, runtime_array *source);
void runtime_integerToBigint(runtime_array *dest, uint64_t value, uint32_t width, bool isSigned, bool secret);
uint64_t runtime_bigintToInteger(const runtime_array *source);
uint64_t runtime_bigintToIntegerTrunc(const runtime_array *source);
void runtime_bigintDecodeLittleEndian(runtime_array *dest, runtime_array *byteArray,
    uint32_t width, bool isSigned, bool secret);
void runtime_bigintDecodeBigEndian(runtime_array *dest, runtime_array *byteArray,
    uint32_t width, bool isSigned, bool secret);
void runtime_bigintEncodeLittleEndian(runtime_array *byteArray, runtime_array *source);
void runtime_bigintEncodeBigEndian(runtime_array *byteArray, runtime_array *source);
uint32_t runtime_bigintToU32(const runtime_array *a);
void runtime_generateTrueRandomBigint(runtime_array *dest, uint32_t width);
bool runtime_compareBigints(runtime_comparisonType compareType, const runtime_array *a, const runtime_array *b);
void runtime_bigintAdd(runtime_array *dest, runtime_array *a, runtime_array *b);
void runtime_bigintAddTrunc(runtime_array *dest, runtime_array *a, runtime_array *b);
void runtime_bigintSub(runtime_array *dest, runtime_array *a, runtime_array *b);
void runtime_bigintSubTrunc(runtime_array *dest, runtime_array *a, runtime_array *b);
void runtime_bigintMul(runtime_array *dest, runtime_array *a, runtime_array *b);
void runtime_bigintMulTrunc(runtime_array *dest, runtime_array *a, runtime_array *b);
void runtime_bigintDiv(runtime_array *dest, runtime_array *a, runtime_array *b);
void runtime_bigintMod(runtime_array *dest, runtime_array *a, runtime_array *b);
void runtime_bigintExp(runtime_array *dest, runtime_array *base, uint32_t exponent);
void runtime_bigintNegate(runtime_array *dest, runtime_array *a);
void runtime_bigintNegateTrunc(runtime_array *dest, runtime_array *a);
void runtime_bigintComplement(runtime_array *dest, runtime_array *a);
void runtime_bigintShl(runtime_array *dest, runtime_array *source, uint32_t dist);
void runtime_bigintShr(runtime_array *dest, runtime_array *source, uint32_t dist);
void runtime_bigintRotl(runtime_array *dest, runtime_array *source, uint32_t dist);
void runtime_bigintRotr(runtime_array *dest, runtime_array *source, uint32_t dist);
void runtime_bigintBitwiseAnd(runtime_array *dest, runtime_array *a, runtime_array *b);
void runtime_bigintBitwiseOr(runtime_array *dest, runtime_array *a, runtime_array *b);
void runtime_bigintBitwiseXor(runtime_array *dest, runtime_array *a, runtime_array *b);
void runtime_bigintDivRem(runtime_array *q, runtime_array *r, runtime_array *a, runtime_array *b);
// Modular operations on bigints.
void runtime_bigintModularAdd(runtime_array *dest, runtime_array *a, runtime_array *b, runtime_array *modulus);
void runtime_bigintModularSub(runtime_array *dest, runtime_array *a, runtime_array *b, runtime_array *modulus);
void runtime_bigintModularMul(runtime_array *dest, runtime_array *a, runtime_array *b, runtime_array *modulus);
void runtime_bigintModularDiv(runtime_array *dest, runtime_array *a, runtime_array *b, runtime_array *modulus);
void runtime_bigintModularExp(runtime_array *dest, runtime_array *base, runtime_array *exponent, runtime_array *modulus);
void runtime_bigintModularNegate(runtime_array *dest, runtime_array *a, runtime_array *modulus);
bool runtime_bigintModularInverse(runtime_array *dest, runtime_array *source, runtime_array *modulus);
static inline void runtime_copyBigint(runtime_array *dest, runtime_array *source) {
  runtime_copyArray(dest, source, sizeof(uint32_t), false);
}

// Small secret integer APIs.  Are any other operations non-constant anywhere?
uint64_t runtime_smallnumMul(uint64_t a, uint64_t b, bool isSigned, bool secret);
uint64_t runtime_smallnumDiv(uint64_t a, uint64_t b, bool isSigned, bool secret);
uint64_t runtime_smallnumMod(uint64_t a, uint64_t b, bool isSigned, bool secret);
uint64_t runtime_smallnumModReduce(uint64_t value, uint64_t modulus, bool isSigned, bool secret);
uint64_t runtime_smallnumExp(uint64_t base, uint32_t exponent, bool isSigned, bool secret);

// Small integer modular operations.
uint64_t runtime_smallnumModularAdd(uint64_t a, uint64_t b, uint64_t modulus, bool secret);
uint64_t runtime_smallnumModularSub(uint64_t a, uint64_t b, uint64_t modulus, bool secret);
uint64_t runtime_smallnumModularMul(uint64_t a, uint64_t b, uint64_t modulus, bool secret);
uint64_t runtime_smallnumModularDiv(uint64_t a, uint64_t b, uint64_t modulus, bool secret);
uint64_t runtime_smallnumModularExp(uint64_t base, uint64_t exponent, uint64_t modulus, bool secret);
uint64_t runtime_smallnumModularNegate(uint64_t a, uint64_t modulus, bool secret);

// runtime_bool API, for secret Boolean values.
runtime_bool runtime_boolToRnBool(bool a);
bool runtime_rnBoolToBool(runtime_bool a);
runtime_bool runtime_boolAnd(runtime_bool a, runtime_bool b);
runtime_bool runtime_boolOr(runtime_bool a, runtime_bool b);
void runtime_bigintXor(runtime_array *dest, runtime_array *a, runtime_array *b);
runtime_bool runtime_boolNot(runtime_bool a);
uint64_t runtime_selectUint32(runtime_bool select, uint64_t data1, uint64_t data0);
void runtime_bigintCondCopy(runtime_bool doCopy, runtime_array *dest, const runtime_array *source);

// Used in Linux for testing purposes.
#define runtime_setJmp() (runtime_jmpBufSet = true, setjmp(runtime_jmpBuf))
extern jmp_buf runtime_jmpBuf;
extern bool runtime_jmpBufSet;

#endif  // EXPERIMENTAL_WAYWARDGEEK_RUNE_RUNTIME_RUNE_RUNTIME_H_
