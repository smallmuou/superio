/**
 *file superio.h
 *create by xuwf
 *copyright(C)2012
 */
#ifndef _SIO_SUPERIO_H_
#define _SIO_SUPERIO_H_
#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>

/**
 * @name SIO open modes
 * The flags argument to sio_open and cosins must be one of the following
 * constants, optionally ORed with other flags.
 * @{
 */
#define SIO_RDONLY 1  /**< read-only */
#define SIO_WRONLY 2  /**< write-only */
#define SIO_RDWR   (SIO_RDONLY|SIO_WRONLY)  /**< read-write */

typedef struct SIOContext SIOContext;
/**
 *group of sio operation same as std io
 */
int sio_open(SIOContext **h, const char *url, int flags);
int sio_close(SIOContext *h);
int sio_read(SIOContext *h, unsigned char *buf, int size);
int sio_write(SIOContext *h, const unsigned char *buf, int size);
int64_t sio_seek(SIOContext *h, int64_t pos, int whence);
int64_t sio_filesize(SIOContext *h);


#ifdef __cplusplus
}
#endif
#endif
