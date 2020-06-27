/* Default visibility */
#define DEFAULT_VISIBILITY __attribute__((visibility("default")))

/* Start with debug message logging enabled */
#undef ENABLE_DEBUG_LOGGING

/* Message logging */
#undef ENABLE_LOGGING

/* Define to 1 if you have the <asm/types.h> header file. */
/* #undef HAVE_ASM_TYPES_H */

/* Define to 1 if you have the `gettimeofday' function. */
/* #undef HAVE_GETTIMEOFDAY */

/* Define to 1 if you have the `udev' library (-ludev). */
/* #undef HAVE_LIBUDEV */

/* Define to 1 if you have the <linux/filter.h> header file. */
/* #undef HAVE_LINUX_FILTER_H */

/* Define to 1 if you have the <linux/netlink.h> header file. */
/* #undef HAVE_LINUX_NETLINK_H */

/* Define to 1 if you have the <poll.h> header file. */
/* #undef HAVE_POLL_H */

/* Define to 1 if you have the <signal.h> header file. */
#define HAVE_SIGNAL_H 1

/* Define to 1 if you have the <strings.h> header file. */
/* #undef HAVE_STRINGS_H */

/* Define to 1 if the system has the type `struct timespec'. */
/* #undef HAVE_STRUCT_TIMESPEC */

/* syslog() function available */
/* #undef HAVE_SYSLOG_FUNC */

/* Define to 1 if you have the <syslog.h> header file. */
/* #undef HAVE_SYSLOG_H */

/* Define to 1 if you have the <sys/socket.h> header file. */
/* #undef HAVE_SYS_SOCKET_H */

/* Define to 1 if you have the <sys/time.h> header file. */
/* #undef HAVE_SYS_TIME_H */

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Darwin backend */
/* #undef OS_DARWIN */

/* Linux backend */
/* #undef OS_LINUX */

/* NetBSD backend */
/* #undef OS_NETBSD */

/* OpenBSD backend */
/* #undef OS_OPENBSD */

/* Windows backend */
#define OS_WINDOWS 1

/* type of second poll() argument */
#define POLL_NFDS_TYPE unsigned int

/* Use POSIX Threads */
/* #undef THREADS_POSIX */

/* timerfd headers available */
/* #undef USBI_TIMERFD_AVAILABLE */

/* Enable output to system log */
#define USE_SYSTEM_LOGGING_FACILITY 1

/* Use udev for device enumeration/hotplug */
/* #undef USE_UDEV */

/* Use GNU extensions */
#define _GNU_SOURCE

/* Oldest Windows version supported */
#define WINVER 0x0501
