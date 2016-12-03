#ifndef _LOG_H_
# define _LOG_H_

#define warn(fmt, args...) do { fprintf(stderr, "warning: %s: " fmt "\n", __func__, ## args); } while (0)
#define error(fmt, args...) do { fprintf(stderr, "error: %s: " fmt "\n", __func__, ## args); if (!force) longjmp(setjmp_env, EXIT_FAILURE); } while (0)
#define error_errno(s, args...) error(s ": %s", ##args, strerror(errno))
#define critical_error(fmt, args...) do { fprintf(stderr, "critical error: %s: " fmt "\n", __func__, ## args); longjmp(setjmp_env, EXIT_FAILURE); } while (0)
#define critical_error_errno(s, args...) critical_error(s ": %s", ##args, strerror(errno))

#ifndef min /* already defined by windows.h */
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif


#endif /* !_LOG_H_ */
