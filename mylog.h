#ifndef _MYLOG_H_
#define _MYLOG_H_

#include "log.h"

extern FILE *logfile;
extern int debuglvl;

#define myerr(fmt, args...) err(logfile, fmt, ##args)
#define mylog(fmt, args...) \
	do { if (debuglvl >= DBG_INFO) log(logfile, fmt, ##args); } while (0)
#define mytrace(fmt, args...) \
	do { if (debuglvl >= DBG_TRACE) trace(logfile, fmt, ##args); } while (0)
#define mytrace_ts(fmt, args...) \
	do { if (debuglvl >= DBG_TRACE) trace_ts(logfile, fmt, ##args); } while (0)

#define assert_or_goto(cond, label, fmt, args...) assert_or_goto_raw(cond, label, logfile, fmt, ##args)

#define assert_or_return(cond, retval, fmt, args...) assert_or_return_raw(cond, retval, logfile, fmt, ##args)

#endif
