#ifndef __ULOG_SHD_H__
#define __ULOG_SHD_H__

#include <ulog.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ULOG_SHD_NB_SAMPLES 2048

/* make sure the structure size is multiple of int size */
struct ulog_shd_blob {
	uint16_t index;			/* ulog message index */
	uint8_t prio;			/* Priority level */
	uint32_t tid;			/* thread id */
	int32_t thnsize;		/* Thread name size */
	int32_t tagsize;		/* tag name size */
	int32_t logsize;		/* Log message size */
	char buf[ULOG_BUF_SIZE];	/* buffer for thread/tag/log */
};
#ifdef __cplusplus
}
#endif

#endif
