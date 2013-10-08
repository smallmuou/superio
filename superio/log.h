#ifndef _SIO_LOG_H_
#define _SIO_LOG_H_
#ifdef __cplusplus
extern C {
#endif

typedef enum {
	LOG_LEVEL_INFO = 0,
	LOG_LEVEL_WARMING,
	LOG_LEVEL_ERR,
	LOG_LEVEL_DEBUG,
	LOG_LEVEL_NUM,
}log_level;

void log_message(log_level level, char *fmt, ...);

#define INFO(x, y...) log_message(LOG_LEVEL_INFO, x, ## y)
#define WARM(x, y...) log_message(LOG_LEVEL_WARMING, x, ## y)
#define ERR(x, y...) log_message(LOG_LEVEL_ERR, x, ## y)

#ifdef NDEBUG
#define DBG(x, y...)	log_message(LOG_LEVEL_DEBUG, "[%s:%d]=========> " x, __FILE__, __LINE__, ## y)
#else
#define DBG(x, y...) do {} while(0)
#endif

#ifdef __cplusplus
}
#endif
#endif
