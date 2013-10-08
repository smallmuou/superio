#ifndef _SIO_UTIL_H_
#define _SIO_UTIL_H_
#ifdef __cplusplus
extern C {

#endif

#define AVERROR(e) (e)
#define FFMIN(a, b) ((a) > (b) ? (b) : (a))
#define MKTAG(a,b,c,d) ((a) | ((b) << 8) | ((c) << 16) | ((unsigned)(d) << 24))
#define ERROR_EOF                (-MKTAG( 'E','O','F',' ')) ///< End of file
#define SPACE_CHARS " \t\r\n"



struct addrinfo {
    int ai_flags;
    int ai_family;
    int ai_socktype;
    int ai_protocol;
    int ai_addrlen;
    struct sockaddr *ai_addr;
    char *ai_canonname;
    struct addrinfo *ai_next;
};


/**
 *@name have_prefix
 *@decs if str have prefix, return the pos after prefix, else return NULL
 */
const char *have_prefix(const char *str, const char *prefix);
size_t sio_strlcpy(char *dst, const char *src, size_t size);
size_t sio_strlcatf(char *dst, size_t size, const char *fmt, ...);
size_t sio_strlcat(char *dst, const char *src, size_t size);



static inline int is_dos_path(const char *path)
{
    if (path[0] && path[1] == ':')
        return 1;
    return 0;
}

int socket_nonblock(int socket, int enable);
int sio_url_join(char *str, int size, const char *proto,
                const char *authorization, const char *hostname,
                int port, const char *fmt, ...);
    
void sio_url_split(char *proto, int proto_size,
                  char *authorization, int authorization_size,
                  char *hostname, int hostname_size,
                  int *port_ptr,
                  char *path, int path_size,
                  const char *url);

int sio_find_info_tag(char *arg, int arg_size, const char *tag1, const char *info);
int sio_stristart(const char *str, const char *pfx, const char **ptr);
char *sio_stristr(const char *s1, const char *s2);


#ifdef __cplusplus
}
#endif
#endif

