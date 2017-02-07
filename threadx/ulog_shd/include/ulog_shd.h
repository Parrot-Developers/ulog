#ifndef __ULOG_SHD_H__
#define __ULOG_SHD_H__

#include <ulog.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ULOG_SHD_NB_SAMPLES 2048

/* make sure the structure size is multiple of int size */
struct ulog_shd_blob {
	unsigned short int index;	/* ulog message index */
	unsigned char prio;		/* Priority level */
	unsigned long int tid;		/* thread id */
	int thnsize;			/* Thread name size */
	int tagsize;			/* tag name size */
	int logsize;			/* Log message size */
	char buf[ULOG_BUF_SIZE];	/* buffer for thread/tag/log */
};
#ifdef __cplusplus
}
#endif

#endif
