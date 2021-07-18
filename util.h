#ifndef _UTIL_H_INCLUDE_
#define _UTIL_H_INCLUDE_

#define BUFFER_SIZE 4096
#define MAX_HEADER_SIZE 4096
#define MAX_THREADS 64

#define USAGE_MSG "usage: ./httpserver <hostname:port> [-N nthreads] [-v verbose]\n"

int valid_filename(char *s);

int valid_port(char *p);

int warn_exit(char *msg);


#endif