#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"

#ifndef CONFIG_H
#define CONFIG_H

#include <grub/disk.h>

typedef long int off_t;
typedef grub_disk_t dev_t;
typedef int uid_t;
typedef int gid_t;
typedef int pid_t;

#define HAVE_STDIO_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STDINT_H 1
#define HAVE_STRING_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_STDDEF_H 1
#define HAVE_TIME_H 1
#define HAVE_CTYPE_H 1
#define HAVE_LIMITS_H 1
#define HAVE_STDARG_H 1
#define HAVE_ERRNO_H 1

#undef linux

#define NO_NTFS_DEVICE_DEFAULT_IO_OPS 1

#define HAVE_DAEMON 1

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define random rand
#define srandom srand

#endif /* CONFIG_H */
