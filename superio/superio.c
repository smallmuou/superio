#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "superio.h"
#include "priv.h"
#include "util.h"
#include "log.h"

#define REGISTER_PROTOCOL(x) { \
    extern SIOProtocol sio_##x##_protocol; \
    sio_register_protocol(&sio_##x##_protocol); }

static SIOProtocol *first_protocol = NULL;
void sio_register_all()
{
	static int initialized = 0;
	if (initialized) return;

	initialized = 1;

	REGISTER_PROTOCOL(file);
	REGISTER_PROTOCOL(http);
	REGISTER_PROTOCOL(tcp);
}

int sio_register_protocol(SIOProtocol *protocol)
{
	SIOProtocol **p = &first_protocol;

    while (*p != NULL) p = &(*p)->next;
    *p = protocol;
    protocol->next = NULL;
    return 0;
}

SIOProtocol *sio_find_next_protocol(SIOProtocol *protocol)
{
	if (!protocol) return first_protocol;
	if (protocol) return protocol->next;
	return NULL;
}

/**
 *sio operation
 */
#define URL_SCHEME_CHARS                        \
    "abcdefghijklmnopqrstuvwxyz"                \
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"                \
    "0123456789+-."

int SIOContent_dealloc(SIOContext *h);
int SIOContent_alloc(SIOContext **h, const char *url, int flags);


int SIOContent_alloc(SIOContext **h, const char *url, int flags)
{
	char proto_str[128] = {0};
	SIOProtocol *p = NULL;
	size_t proto_len = strspn(url, URL_SCHEME_CHARS);

	if ((url[proto_len] != ':' &&  url[proto_len] != ',') || is_dos_path(url)) {
		strcpy(proto_str, "file");
	} else {
		strncpy(proto_str, url, proto_len);
		proto_str[proto_len+1] = '\0';
	}

	//find protocol
	while(NULL != (p = sio_find_next_protocol(p))) {
		if (!strcmp(proto_str, p->name)) {
			*h = (SIOContext *)calloc(1, sizeof(SIOContext));
			if (!*h) {
				ERR("malloc error");
				return -1;
			}
			(*h)->prot = p;
			(*h)->priv_data = NULL;

			if (!strcmp(proto_str, "file")) {
				snprintf((*h)->url, sizeof((*h)->url), "%s://%s", proto_str, url);
			} else {
				snprintf((*h)->url, sizeof((*h)->url), "%s", url);
			}
			return 0;
		}
	}

	WARM("can't find the '%s' protocol", proto_str);
	return -1;
}

int SIOContent_dealloc(SIOContext *h)
{
	if (h) free(h);
    return 0;
}

int sio_open(SIOContext **h, const char *url, int flags)
{
	int ret = -1;
	sio_register_all();

	ret = SIOContent_alloc(h, url, flags);
	if (ret < 0) return ret;
	(*h)->flags = flags;
	return (*h)->prot->open(*h, (*h)->url, flags);
}

int sio_close(SIOContext *h)
{
	if (!h) return -1;

	h->prot->close(h);
	SIOContent_dealloc(h);
	return 0;
}

int sio_read(SIOContext *h, unsigned char *buf, int size)
{
	if (!h || !buf) return -1;
	return h->prot->read(h, buf, size);
}

int sio_write(SIOContext *h, const unsigned char *buf, int size)
{
	if (!h || !buf) return -1;
	return h->prot->write(h, buf, size);
}

int64_t sio_seek(SIOContext *h, int64_t pos, int whence)
{
	if (!h) return -1;
	return h->prot->seek(h, pos, whence);
}

int64_t sio_filesize(SIOContext *h)
{
	int64_t pos, size;
	if (!h) return -1;

    size= h->prot->seek(h, 0, SIOSEEK_SIZE);
	if(size < 0){
        pos = h->prot->seek(h, 0, SEEK_CUR);
        if ((size = h->prot->seek(h, -1, SEEK_END)) < 0)
            return size;
        size++;
        h->prot->seek(h, pos, SEEK_SET);
    }
    return size;
}


