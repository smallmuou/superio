
#include <sys/stat.h>
#include "priv.h"

typedef struct FILEContext {
	int fd;
}FILEContext;


static int file_close(SIOContext *h);


/* standard file protocol */
static int file_open(SIOContext *h, const char *url, int flags)
{
    int access;
    int fd = -1;
	const char *filename = NULL;

	filename = have_prefix(url, "file://");
	if (!filename) {
		ERR("%s is invalid", url);
		return -1;
	}

    if (flags & SIO_RDONLY && flags & SIO_WRONLY) {
        access = O_CREAT | O_TRUNC | O_RDWR;
    } else if (flags & SIO_WRONLY) {
        access = O_CREAT | O_TRUNC | O_WRONLY;
    } else {
        access = O_RDONLY;
    }

    fd = open(filename, access, 0666);
	if (fd < 0) {
		ERR("open %s error", filename);
		return -1;
	}

	FILEContext *cxt = (FILEContext *)calloc(1, sizeof(FILEContext));
	if (!cxt) {
		ERR("malloc error");
		goto _err;
	}

	cxt->fd = fd;
    h->priv_data = (void *)cxt;
    return 0;

_err:
	if (fd >=0 ) close(fd);
	return -1;
}

static int file_close(SIOContext *h)
{
	assert(h);
	FILEContext *cxt = (FILEContext *)h->priv_data;
	if (cxt) {
		if (cxt->fd >= 0) close(cxt->fd);
		free(cxt);
		h->priv_data = NULL;
		return 0;
	}

	return -1;
}

static int file_read(SIOContext *h, unsigned char *buf, int size)
{
	assert(h && buf);

	FILEContext *cxt = (FILEContext *)h->priv_data;
	int ret = -1;
	if (cxt && cxt->fd >= 0) {
		ret = read(cxt->fd, buf, size);
	}

    return ret;
}

static int file_write(SIOContext *h, const unsigned char *buf, int size)
{
	assert(h && buf);
	FILEContext *cxt = (FILEContext *)h->priv_data;
	int ret = -1;
	if (cxt && cxt->fd >= 0) {
		ret = write(cxt->fd, buf, size);
	}
	return ret;
}

static int64_t file_seek(SIOContext *h, int64_t pos, int whence)
{
	assert(h);
	FILEContext *cxt = (FILEContext *)h->priv_data;
	int64_t ret = -1;
	if (cxt && cxt->fd >= 0) {
		if (whence == SIOSEEK_SIZE) {
			struct stat st;
			if (fstat(cxt->fd, &st) == 0) {
				ret = st.st_size;
			}
		} else {
			ret = lseek(cxt->fd, pos, whence);
		}
	}
	return ret;
}


SIOProtocol sio_file_protocol = {
	.name 			= "file",
	.open 			= file_open,
	.close 			= file_close,
	.read			= file_read,
	.write			= file_write,
	.seek			= file_seek,
};


