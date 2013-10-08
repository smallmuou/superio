#ifndef _SIO_PRIV_H_





	int (*open)(SIOContext *h, const char *url, int flags);
	int (*close)(SIOContext *h);

	int (*read)(SIOContext *h, unsigned char *buf, int size);
	int (*write)(SIOContext *h, const unsigned char *buf, int size);
	int64_t (*seek)(SIOContext *h, int64_t pos, int whence);
	int64_t (*filesize)(SIOContext *h);

	struct SIOProtocol *next;
}SIOProtocol;

 *SIOContext
	SIOProtocol *prot;
	void *priv_data;
	char url[URL_MAX_SIZE];
};

#ifdef __cplusplus
}
#endif

#endif
