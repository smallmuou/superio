#include "priv.h"

#define BUF_MAX_SIZE (1024)
#define MAX_REDIRECTS 8
typedef unsigned char uint8_t;


/**
 * Authentication types, ordered from weakest to strongest.
 */
typedef enum HTTPAuthType {
    HTTP_AUTH_NONE = 0,    /**< No authentication specified */
    HTTP_AUTH_BASIC,       /**< HTTP 1.0 Basic auth from RFC 1945
                             *  (also in RFC 2617) */
    HTTP_AUTH_DIGEST,      /**< HTTP 1.1 Digest auth from RFC 2617 */
} HTTPAuthType;

typedef struct {
    char nonce[300];       /**< Server specified nonce */
    char algorithm[10];    /**< Server specified digest algorithm */
    char qop[30];          /**< Quality of protection, containing the one
                             *  that we've chosen to use, from the
                             *  alternatives that the server offered. */
    char opaque[300];      /**< A server-specified string that should be
                             *  included in authentication responses, not
                             *  included in the actual digest calculation. */
    int nc;                /**< Nonce count, the number of earlier replies
                             *  where this particular nonce has been used. */
} DigestParams;


/**
 * HTTP Authentication state structure. Must be zero-initialized
 * before used with the functions below.
 */
typedef struct {
    /**
     * The currently chosen auth type.
     */
    HTTPAuthType auth_type;
    /**
     * Authentication realm
     */
    char realm[200];
    /**
     * The parameters specifiec to digest authentication.
     */
    DigestParams digest_params;
} HTTPAuthState;


typedef struct HTTPContext{
    SIOContext *hd;
    unsigned char buffer[BUF_MAX_SIZE], *buf_ptr, *buf_end;
    int line_count;
    int http_code;
    int64_t chunksize;      /**< Used if "Transfer-Encoding: chunked" otherwise -1. */
    char *user_agent;
    int64_t off, filesize;
    char location[4096];
    HTTPAuthState auth_state;
    HTTPAuthState proxy_auth_state;
    char *headers;
    int willclose;          /**< Set if the server correctly handles Connection: close and will close the connection after feeding us the content. */
    int chunked_post;

}HTTPContext;

//if read end, read from socket
static int http_getc(HTTPContext *cxt)
{
    int len;
    if (cxt->buf_ptr >= cxt->buf_end) {
        len = sio_read(cxt->hd, cxt->buffer, BUF_MAX_SIZE);
        if (len <= 0) {
            return -1;
        } else {
            cxt->buf_ptr = cxt->buffer;
            cxt->buf_end = cxt->buffer + len;
        }
    }
    return *cxt->buf_ptr++;
}

static int http_get_line(HTTPContext *s, char *line, int line_size)
{
    int ch;
    char *q;

    q = line;
    for(;;) {
        ch = http_getc(s);
        if (ch < 0)
            return -1;
        if (ch == '\n') {
            /* process line */
            if (q > line && q[-1] == '\r')
                q--;
            *q = '\0';

            return 0;
        } else {
            if ((q - line) < line_size - 1)
                *q++ = ch;
        }
    }
}

static int process_line(SIOContext *h, char *line, int line_count, int *new_location)
{
    HTTPContext *s = h->priv_data;
    char *tag, *p, *end;

    /* end of header */
    if (line[0] == '\0')
        return 0;

    p = line;
    if (line_count == 0) {
        while (!isspace(*p) && *p != '\0')
            p++;
        while (isspace(*p))
            p++;
        s->http_code = strtol(p, &end, 10);

        /* error codes are 4xx and 5xx, but regard 401 as a success, so we
         * don't abort until all headers have been parsed. */
        if (s->http_code >= 400 && s->http_code < 600 && (s->http_code != 401
            || s->auth_state.auth_type != HTTP_AUTH_NONE) &&
            (s->http_code != 407 || s->proxy_auth_state.auth_type != HTTP_AUTH_NONE)) {
            end += strspn(end, SPACE_CHARS);
            return -1;
        }
    } else {
        while (*p != '\0' && *p != ':')
            p++;
        if (*p != ':')
            return 1;

        *p = '\0';
        tag = line;
        p++;
        while (isspace(*p))
            p++;
        if (!strcasecmp(tag, "Location")) {
            strcpy(s->location, p);
            *new_location = 1;
        } else if (!strcasecmp (tag, "Content-Length") && s->filesize == -1) {
            s->filesize = atoll(p);
        } else if (!strcasecmp (tag, "Content-Range")) {
            /* "bytes $from-$to/$document_size" */
            const char *slash;
            if (!strncmp (p, "bytes ", 6)) {
                p += 6;
                s->off = atoll(p);
                if ((slash = strchr(p, '/')) && strlen(slash) > 0)
                    s->filesize = atoll(slash+1);
            }
            h->is_streamed = 0; /* we _can_ in fact seek */
        } else if (!strcasecmp(tag, "Accept-Ranges") && !strncmp(p, "bytes", 5)) {
            h->is_streamed = 0;
        } else if (!strcasecmp (tag, "Transfer-Encoding") && !strncasecmp(p, "chunked", 7)) {
            s->filesize = -1;
            s->chunksize = 0;
        } else if (!strcasecmp (tag, "Connection")) {
            if (!strcmp(p, "close"))
                s->willclose = 1;
        }
    }
    return 1;

}

static inline int has_header(const char *str, const char *header)
{
    /* header + 2 to skip over CRLF prefix. (make sure you have one!) */
    if (!str)
        return 0;
    return sio_stristart(str, header + 2, NULL) || sio_stristr(str, header);
}


static int http_connect(SIOContext *h, const char *path, const char *local_path,
                        const char *hoststr, const char *auth,
                        const char *proxyauth, int *new_location)
{
    HTTPContext *s = h->priv_data;
    int post, err;
    char line[1024];
    char headers[1024] = "";
    char *authstr = NULL, *proxyauthstr = NULL;
    int64_t off = s->off;
    int len = 0;
    const char *method;


    /* send http header */
    post = h->flags & SIO_WRONLY;
    method = post ? "POST" : "GET";

    /* set default headers if needed */
    if (!has_header(s->headers, "\r\nUser-Agent: "))
        len += sio_strlcatf(headers + len, sizeof(headers) - len,
                           "User-Agent: %s\r\n",
                           s->user_agent ? s->user_agent : "SIO");
    if (!has_header(s->headers, "\r\nAccept: "))
        len += sio_strlcpy(headers + len, "Accept: */*\r\n",
                          sizeof(headers) - len);
    if (!has_header(s->headers, "\r\nRange: ") && !post)
        len += sio_strlcatf(headers + len, sizeof(headers) - len,
                           "Range: bytes=%lld-\r\n", s->off);
    if (!has_header(s->headers, "\r\nConnection: "))
        len += sio_strlcpy(headers + len, "Connection: close\r\n",
                          sizeof(headers)-len);
    if (!has_header(s->headers, "\r\nHost: "))
        len += sio_strlcatf(headers + len, sizeof(headers) - len,
                           "Host: %s\r\n", hoststr);

    /* now add in custom headers */
    if (s->headers)
        sio_strlcpy(headers + len, s->headers, sizeof(headers) - len);

    snprintf(s->buffer, sizeof(s->buffer),
             "%s %s HTTP/1.1\r\n"
             "%s"
             "%s"
             "%s"
             "%s%s"
             "\r\n",
             method,
             path,
             post && s->chunked_post ? "Transfer-Encoding: chunked\r\n" : "",
             headers,
             authstr ? authstr : "",
             proxyauthstr ? "Proxy-" : "", proxyauthstr ? proxyauthstr : "");
    
    if (sio_write(s->hd, s->buffer, strlen(s->buffer)) < 0) {
        return -1;
    }

    /* init input buffer */
    s->buf_ptr = s->buffer;
    s->buf_end = s->buffer;
    s->line_count = 0;
    s->off = 0;
    s->filesize = -1;
    s->willclose = 0;
    if (post) {
        /* Pretend that it did work. We didn't read any header yet, since
         * we've still to send the POST data, but the code calling this
         * function will check http_code after we return. */
        s->http_code = 200;
        return 0;
    }
    s->chunksize = -1;
    /* wait for header */
    for(;;) {
        if (http_get_line(s, line, sizeof(line)) < 0)
            return -1;
        
        err = process_line(h, line, s->line_count, new_location);
        if (err < 0)
            return err;
        if (err == 0)
            break;
        s->line_count++;
    }

    return (off == s->off) ? 0 : -1;
}

/* return non zero if error */
static int http_open_cnx(SIOContext *h)
{
    const char *path, *proxy_path, *lower_proto = "tcp", *local_path;
    char hostname[1024], hoststr[1024], proto[10];
    char auth[1024], proxyauth[1024] = "";
    char path1[1024];
    char buf[1024], urlbuf[1024];
    int port, use_proxy, err, location_changed = 0, redirects = 0;
    HTTPAuthType cur_auth_type, cur_proxy_auth_type;
    HTTPContext *s = h->priv_data;
    SIOContext *hd = NULL;

    proxy_path = NULL;

    /* fill the dest addr */
 redo:
    /* needed in any case to build the host string */
    sio_url_split(proto, sizeof(proto), auth, sizeof(auth),
                 hostname, sizeof(hostname), &port,
                 path1, sizeof(path1), s->location);
    sio_url_join(hoststr, sizeof(hoststr), NULL, NULL, hostname, port, NULL);

    if (port < 0)
        port = 80;

    if (path1[0] == '\0')
        path = "/";
    else
        path = path1;
    local_path = path;

    sio_url_join(buf, sizeof(buf), lower_proto, NULL, hostname, port, NULL);
    err = sio_open(&hd, buf, SIO_RDONLY);
    if (err < 0)
        goto fail;

    s->hd = hd;
    cur_auth_type = s->auth_state.auth_type;
    cur_proxy_auth_type = s->auth_state.auth_type;
    if (http_connect(h, path, local_path, hoststr, auth, proxyauth, &location_changed) < 0)
        goto fail;
    if (s->http_code == 401) {
        if (cur_auth_type == HTTP_AUTH_NONE && s->auth_state.auth_type != HTTP_AUTH_NONE) {
            sio_close(hd);
            goto redo;
        } else
            goto fail;
    }
    if (s->http_code == 407) {
        if (cur_proxy_auth_type == HTTP_AUTH_NONE &&
            s->proxy_auth_state.auth_type != HTTP_AUTH_NONE) {
            sio_close(hd);
            goto redo;
        } else
            goto fail;
    }
    if ((s->http_code == 301 || s->http_code == 302 || s->http_code == 303 || s->http_code == 307)
        && location_changed == 1) {
        /* url moved, get next */
        sio_close(hd);
        if (redirects++ >= MAX_REDIRECTS)
            return -1;
        location_changed = 0;
        goto redo;
    }
    return 0;
 fail:
    if (hd)
        sio_close(hd);
    s->hd = NULL;
    return -1;
}


static int http_open(SIOContext *h, const char *url, int flags)
{
    HTTPContext *s;

    h->is_streamed = 1;

    
	DBG("");
	s = (HTTPContext *)calloc(1, sizeof(HTTPContext));
	s->filesize = -1;
    sio_strlcpy(s->location, url, sizeof(s->location));
	DBG("");
	h->priv_data = s;
    return http_open_cnx(h);

}

static int http_close(SIOContext *h)
{
    int ret = 0;
    char footer[] = "0\r\n\r\n";
    HTTPContext *s = h->priv_data;

    /* signal end of chunked encoding if used */
    if ((h->flags & SIO_WRONLY) && s->chunked_post) {
        ret = sio_write(s->hd, footer, sizeof(footer) - 1);
        ret = ret > 0 ? 0 : ret;
    }

    if (s->hd)
        sio_close(s->hd);

	free(s);
    return ret;

}

static int http_buf_read(SIOContext *h, uint8_t *buf, int size)
{
    HTTPContext *s = h->priv_data;
    int len;
    /* read bytes from input buffer first */
    len = s->buf_end - s->buf_ptr;
    if (len > 0) {
        if (len > size)
            len = size;
        memcpy(buf, s->buf_ptr, len);
        s->buf_ptr += len;
    } else {
        if (!s->willclose && s->filesize >= 0 && s->off >= s->filesize)
            return ERROR_EOF;
        len = sio_read(s->hd, buf, size);
    }
    if (len > 0) {
        s->off += len;
        if (s->chunksize > 0)
            s->chunksize -= len;
    }
    return len;
}


static int http_read(SIOContext *h, unsigned char *buf, int size)
{
    HTTPContext *s = h->priv_data;

    if (s->chunksize >= 0) {
        if (!s->chunksize) {
            char line[32];

            for(;;) {
                do {
                    if (http_get_line(s, line, sizeof(line)) < 0)
                        return -1;
                } while (!*line);    /* skip CR LF from last chunk */

                s->chunksize = strtoll(line, NULL, 16);
                if (!s->chunksize)
                    return 0;
                break;
            }
        }
        size = FFMIN(size, s->chunksize);
    }
    return http_buf_read(h, buf, size);

}

static int http_write(SIOContext *h, const unsigned char *buf, int size)
{
    char temp[11] = "";  /* 32-bit hex + CRLF + nul */
    int ret;
    char crlf[] = "\r\n";
    HTTPContext *s = h->priv_data;

    if (!s->chunked_post) {
        /* non-chunked data is sent without any special encoding */
        return sio_write(s->hd, buf, size);
    }

    /* silently ignore zero-size data since chunk encoding that would
     * signal EOF */
    if (size > 0) {
        /* upload data using chunked encoding */
        snprintf(temp, sizeof(temp), "%x\r\n", size);

        if ((ret = sio_write(s->hd, temp, strlen(temp))) < 0 ||
            (ret = sio_write(s->hd, buf, size)) < 0 ||
            (ret = sio_write(s->hd, crlf, sizeof(crlf) - 1)) < 0)
            return ret;
    }
    return size;

}

static int64_t http_seek(SIOContext *h, int64_t off, int whence)
{
    HTTPContext *s = h->priv_data;
    SIOContext *old_hd = s->hd;
    int64_t old_off = s->off;
    uint8_t old_buf[BUF_MAX_SIZE];
    int old_buf_size;

    if (whence == SIOSEEK_SIZE)
        return s->filesize;
    else if ((s->filesize == -1 && whence == SEEK_END) || h->is_streamed)
        return -1;

    /* we save the old context in case the seek fails */
    old_buf_size = s->buf_end - s->buf_ptr;
    memcpy(old_buf, s->buf_ptr, old_buf_size);
    s->hd = NULL;
    if (whence == SEEK_CUR)
        off += s->off;
    else if (whence == SEEK_END)
        off += s->filesize;
    s->off = off;

    /* if it fails, continue on old connection */
    if (http_open_cnx(h) < 0) {
        memcpy(s->buffer, old_buf, old_buf_size);
        s->buf_ptr = s->buffer;
        s->buf_end = s->buffer + old_buf_size;
        s->hd = old_hd;
        s->off = old_off;
        return -1;
    }
    sio_close(old_hd);
    return off;
}

SIOProtocol sio_http_protocol = {
	.name 		= "http",
	.open		= http_open,
	.close		= http_close,
	.read		= http_read,
	.write		= http_write,
	.seek		= http_seek,
};

