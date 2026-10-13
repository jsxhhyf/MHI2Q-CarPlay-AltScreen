#ifndef QSHIM_ZLIB_H
#define QSHIM_ZLIB_H
/* Public zlib 1.2 API subset, resolved from the vehicle libz.so.2. */
typedef unsigned char Bytef;
typedef unsigned int uInt;
typedef unsigned long uLong;
typedef uLong uLongf;
#define Z_NULL 0
#define Z_OK 0
#define Z_BEST_SPEED 1
extern int compress2(Bytef *dest, uLongf *destLen, const Bytef *src, uLong srcLen, int level);
extern uLong compressBound(uLong srcLen);
extern uLong crc32(uLong crc, const Bytef *buf, uInt len);
#endif
