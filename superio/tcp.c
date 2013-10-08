#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <errno.h>
#include <poll.h>
#include "priv.h"

typedef struct TCPContext{
	int fd;
}TCPContext;

static int tcp_close(SIOContext *h);
int network_wait(int fd, int write);

/* return non zero if error */
static int tcp_open(SIOContext *h, const char *uri, int flags)
{
    int port, fd = -1;
    TCPContext *cxt = NULL;
    int ret;
    socklen_t optlen;
    int timeout = 50;
    char hostname[1024],proto[1024],path[1024];
    struct sockaddr_in addr;
    socklen_t addr_len;

    sio_url_split(proto, sizeof(proto), NULL, 0, hostname, sizeof(hostname),
        &port, path, sizeof(path), uri);
    if (strcmp(proto,"tcp") || port <= 0 || port >= 65536) {
        ERR("%s is invalid");
        return -1;
    }
    
    addr_len = sizeof(addr);
    memset(&addr, 0, addr_len);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(hostname);
    
 restart:
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        ERR("tcp socket error\n");
        goto _err;
    }
 redo:
    socket_nonblock(fd, 1);
    ret = connect(fd, (struct sockaddr *)&addr, addr_len);

    if (ret < 0) {
        struct pollfd p = {fd, POLLOUT, 0};
        if (errno != EINPROGRESS)
            goto _err;
        /* wait until we are connected or until abort */
        while(timeout--) {
            ret = poll(&p, 1, 100);
            if (ret > 0)
                break;
        }
        
        if (ret <= 0) {
            ERR("tcp connect error");
            goto _err;
        }
        /* test error */
        optlen = sizeof(ret);
        getsockopt (fd, SOL_SOCKET, SO_ERROR, &ret, &optlen);
        if (ret != 0) {
            ERR("tcp getsockopt error");
            goto _err;
        }
    }
    h->is_streamed = 1;
	cxt = (TCPContext *)calloc(1, sizeof(TCPContext));
    if (!cxt) {
        ERR("calloc fail");
        goto _err;
    }
    
    cxt->fd = fd;
	h->priv_data = cxt;
    return 0;

 _err:
    if (fd >= 0)
        close(fd);
    return -1;
}


static int tcp_close(SIOContext *h)
{
	assert(h);
	TCPContext *cxt = (TCPContext *)h->priv_data;
	if (cxt) {
		if (cxt->fd >= 0) close(cxt->fd);
		free(cxt);
		h->priv_data = NULL;
		return 0;
	}

	return -1;
}

int network_wait(int fd, int write)
{
    int ev = write ? POLLOUT : POLLIN;
    struct pollfd p = { .fd = fd, .events = ev, .revents = 0 };
    int ret;
    ret = poll(&p, 1, 100);
    return ret < 0 ? ret : p.revents & (ev | POLLERR | POLLHUP) ? 0 : -1;
}


static int tcp_read(SIOContext *h, unsigned char *buf, int size)
{
	assert(h && buf);

	TCPContext *cxt = (TCPContext *)h->priv_data;
	int ret = -1;

	if (cxt && cxt->fd >= 0) {
		ret = network_wait(cxt->fd, 0);
		if (ret < 0) return -1;
		ret = recv(cxt->fd, buf, size, 0);
	}

    return ret;

}

static int tcp_write(SIOContext *h, const unsigned char *buf, int size)
{
	assert(h && buf);
	TCPContext *cxt = (TCPContext *)h->priv_data;
	int ret = -1;
	if (cxt && cxt->fd >= 0) {
		ret = network_wait(cxt->fd, 1);
		if (ret < 0) return -1;
		ret = send(cxt->fd, buf, size, 0);
	}

    return ret;
}

SIOProtocol sio_tcp_protocol = {
	.name 		= "tcp",
	.open		= tcp_open,
	.close		= tcp_close,
	.read		= tcp_read,
	.write		= tcp_write,
};

