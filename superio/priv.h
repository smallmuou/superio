#ifndef _SIO_PRIV_H_#define _SIO_PRIV_H_
#ifdef __cplusplusextern C {#endif
#include <stdio.h>#include <fcntl.h>#include <stdlib.h>#include <sys/types.h>#include <fcntl.h>#include <string.h>#include <assert.h>#include <unistd.h>
#include "superio.h"#include "util.h"#include "log.h"
#define SIOSEEK_SIZE 	(0x100)#define URL_MAX_SIZE	(512)
/** *SIOProtocol */typedef struct SIOProtocol {	const char *name;
	int (*open)(SIOContext *h, const char *url, int flags);
	int (*close)(SIOContext *h);

	int (*read)(SIOContext *h, unsigned char *buf, int size);
	int (*write)(SIOContext *h, const unsigned char *buf, int size);
	int64_t (*seek)(SIOContext *h, int64_t pos, int whence);
	int64_t (*filesize)(SIOContext *h);

	struct SIOProtocol *next;
}SIOProtocol;
/**
 *SIOContext */struct SIOContext {
	SIOProtocol *prot;
	void *priv_data;
	char url[URL_MAX_SIZE];	int flags;	int is_streamed; //if no, can't seek
};

#ifdef __cplusplus
}
#endif

#endif

