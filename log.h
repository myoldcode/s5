#ifndef _LOG_H_
#define _LOG_H_

#include <stdio.h>
#include <time.h>

enum
{
	DBG_ERR,
	DBG_INFO,
	DBG_TRACE,
};

#define log(logf, fmt, args...) \
do \
{ \
	fprintf(logf, fmt, ##args); \
} while (0)

#define err(logf, fmt, args...) \
do \
{ \
	log(logf, "ERROR: "fmt, ##args); \
} while (0)

#define trace(logf, fmt, args...) \
do \
{ \
	log(logf, "TRACE: %s:%d: "fmt, __func__, __LINE__, ##args); \
} while (0)

#define trace_ts(logf, fmt, args...) \
do \
{ \
	struct timespec tp; \
	int rc; \
	rc = clock_gettime(CLOCK_MONOTONIC, &tp); \
	if (rc == 0) \
	{ \
		trace(logf, "%ld.%ld: "fmt, tp.tv_sec, tp.tv_nsec, ##args); \
	} \
	else \
	{ \
		trace(logf, "0.0: "fmt, ##args); \
	} \
} while (0)

#define ok_or_goto(cond, label) \
do \
{ \
	if (!(cond)) \
	{ \
		goto label; \
	} \
} while (0)

#define assert_or_goto_raw(cond, label, logf, fmt, args...) \
do \
{ \
	if (!(cond)) \
	{ \
		err(logf, fmt, ##args); \
		goto label; \
	} \
} while (0)

#define assert_or_return_raw(cond, retval, logf, fmt, args...) \
do \
{ \
	if (!(cond)) \
	{ \
		err(logf, fmt, ##args); \
		return retval; \
	} \
} while (0)

#endif
