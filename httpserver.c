#include "queue.h"
#include "util.h"
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <fcntl.h>
#include <getopt.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <errno.h>
#include <err.h>
#include <pthread.h>
#include <semaphore.h>
#include <limits.h>
#include <netdb.h>

bool verbose = false;

// stores all key data of each http request
struct http_object {
    int cl;
    char method[5];
    char filename[256]; 
    char httpversion[9];
    char buffer[BUFFER_SIZE];
    ssize_t content_length;  
    uint16_t status_code;
    uint16_t extra_bytes; // used when part of body is read while reading header
    uint16_t index;  // used when part of body is read while reading header
};

// construct response
static void send_response_header(struct http_object* msg) {

    char status_msg[30];
    switch (msg->status_code) {
        case 200:
            strcpy(status_msg, "OK");
            break;
        case 201:
            strcpy(status_msg, "Created");
            break;
        case 400:
            strcpy(status_msg, "Bad Request");
            break;
        case 403:
            strcpy(status_msg, "Forbidden");
            break;
        case 404:
            strcpy(status_msg, "Not Found");
            break;
        default:
            strcpy(status_msg, "Internal Server Error");
            msg->status_code = 500;
    }

    // clear buffer
    memset(msg->buffer, '\0', sizeof(msg->buffer));
    sprintf(msg->buffer, "HTTP/1.1 %d %s\r\nContent-Length: %zu\r\n\r\n", msg->status_code, status_msg, msg->content_length);
    if(write(msg->cl, msg->buffer, strlen(msg->buffer)) < 0) return;
    
}

// sets the status code to 400, 403, 404, or 500 and content length to 0
static void http_error_reply(struct http_object* msg, uint16_t code) {
    msg->status_code = code;
    msg->content_length = 0;
    send_response_header(msg);
    return;
}

// sets http_object fields to 0 or null
static void initialize(struct http_object* msg, int cl) {
    memset(msg->method, '\0', sizeof(msg->method));
    memset(msg->filename, '\0', sizeof(msg->filename));
    memset(msg->httpversion, '\0', sizeof(msg->httpversion));
    memset(msg->buffer, '\0', sizeof(msg->buffer));
    msg->content_length = 0;
    msg->status_code = 0;
    msg->extra_bytes = 0;
    msg->index = 0;
    msg->cl = cl;
}

int read_header(struct http_object* msg) {
    ssize_t bytes = 0;
    char* double_crlf_ptr = NULL;

    // read stream until either found a double CRLF or read 4 KiB
    while(bytes < MAX_HEADER_SIZE) {
        ssize_t bytes_read = read(msg->cl, (void*)(msg->buffer + bytes), MAX_HEADER_SIZE - bytes);
        if(bytes_read < 0) break;

        bytes += bytes_read;
        double_crlf_ptr = strstr(msg->buffer, "\r\n\r\n");
        if(double_crlf_ptr != NULL) {
            // stores amount of "extra" (body) bytes read along with header
            double_crlf_ptr += 4; 
            msg->index = double_crlf_ptr - msg->buffer; // aka header size including double crlf
            msg->extra_bytes = bytes - msg->index; // size of data read in buffer minus header
            return 1;
        }
    }

    return -1;
}

// writes contents of file (filename) to cl. (for a GET response)
static void send_body(struct http_object* msg) {

    int8_t fd = open(msg->filename, O_RDONLY);
    ssize_t bytes_read = read(fd, msg->buffer, BUFFER_SIZE);
    if(bytes_read == -1) return;

    while(bytes_read > 0) {
        ssize_t bytes_written = write(msg->cl, msg->buffer, bytes_read);
        if(bytes_written == -1) break;
        
        bytes_read = read(fd, msg->buffer, BUFFER_SIZE);
        if(bytes_read == -1) break;
    }
    close(fd);
    return;
}

// read http request
static void read_http_request(struct http_object* msg) {

    if(read_header(msg) == -1) {
        http_error_reply(msg, 400);
        return;
    }

    // fill http_object fields (method, filename, http version, content-length)
    int8_t var_filled = sscanf(msg->buffer, "%s /%s %s", msg->method, msg->filename, msg->httpversion);
    if(var_filled != 3) {
        http_error_reply(msg, 400);
    } else {
        if(strcmp(msg->method, "PUT") == 0) {
            // get content length (for a PUT)
            char* cl_ptr = strstr(msg->buffer, "Content-Length:");
            if(cl_ptr == NULL) {
                http_error_reply(msg, 400);
            } else {
                if(sscanf(cl_ptr, "Content-Length: %zd", &msg->content_length) != 1) {
                    http_error_reply(msg, 400);
                }
            }
        }
    }
    
    return;
}

static void http_get(struct http_object* msg) {
    
    struct stat fileStat;

    if(stat(msg->filename, &fileStat) != 0 ) {
        http_error_reply(msg, 404); // file not found
    } else if(S_ISDIR(fileStat.st_mode)) {
        http_error_reply(msg, 400); // file is a dir
    } if(!(fileStat.st_mode & S_IRUSR)) {
        http_error_reply(msg, 403); // file does not have read perms
    } else {
        msg->status_code = 200;
        msg->content_length = fileStat.st_size;
        send_response_header(msg);
        send_body(msg);
    }

    return;
}

static void http_head(struct http_object* msg) {
    
    struct stat fileStat;

    if(stat(msg->filename, &fileStat) != 0 ) {
        http_error_reply(msg, 404); // file not found
    } else if(S_ISDIR(fileStat.st_mode)) {
        http_error_reply(msg, 400); // file is a dir
    } if(!(fileStat.st_mode & S_IRUSR)) {
        http_error_reply(msg, 403); // file does not have read perms
    } else {
        msg->status_code = 200;
        msg->content_length = fileStat.st_size;
        send_response_header(msg);
    }

    return;
}

static void http_put(struct http_object* msg) {

    int fd;
    struct stat fileStat;

    if(stat(msg->filename, &fileStat) != 0 ) {
        // file doesn't exist, try to create
        fd = open(msg->filename, O_WRONLY | O_CREAT | O_TRUNC, 0664);
        if(fd < 0) {
            http_error_reply(msg, 403); // could not create
            return;
        } else {
            msg->status_code = 201;
        }
    } else if(S_ISDIR(fileStat.st_mode)) {
        http_error_reply(msg, 400); // file is a dir
        return;
    } else if(!(fileStat.st_mode & S_IWUSR)) {
        http_error_reply(msg, 403); // file does not have write perms
        return;
    } else {
        // exists
        fd = open(msg->filename, O_WRONLY | O_TRUNC);
        if(fd < 0) {
            http_error_reply(msg, 403); // could not open
            return;
        } else {
            msg->status_code = 200;
        }
    }

    // read data from server write 2 file
    ssize_t write_count = 0;
    ssize_t bytes_needed = msg->content_length;

    // if there are bytes left over from first read
    if(msg->extra_bytes) {
        char* request_body = msg->buffer + msg->index;
        if (bytes_needed < msg->extra_bytes) {
            msg->extra_bytes = bytes_needed;
        }
        write_count = write(fd, request_body, msg->extra_bytes);
        if(write_count == -1) {
            http_error_reply(msg, 500);
            close(fd);
            return;
        }
        bytes_needed -= write_count;
    }
    while(bytes_needed > 0) {
        ssize_t read_count = read(msg->cl, msg->buffer, BUFFER_SIZE);
        if(read_count == -1) {
            http_error_reply(msg, 500);
            close(fd);
            return;
        }
        if(read_count > bytes_needed) {
            write_count = write(fd, msg->buffer, bytes_needed);
            if(write_count == -1) {
                http_error_reply(msg, 500);
                close(fd);
                return;
            }
            bytes_needed -= write_count;
        }
        else {
            write_count = write(fd, msg->buffer, read_count);
            if(write_count == -1) {
                http_error_reply(msg, 500);
                close(fd);
                return;
            }
            bytes_needed -= write_count;
        }
    }
    close(fd);

    msg->content_length = 0;
    send_response_header(msg);

}

// process request
static void process_request(struct http_object* msg) {

    if(strcmp(msg->httpversion, "HTTP/1.1") != 0) {
        http_error_reply(msg, 400);
    } else if(!(valid_filename(msg->filename))) {
        http_error_reply(msg, 400);
    } else if(!strcmp(msg->method, "GET")) {
        http_get(msg);
    } else if(!strcmp(msg->method, "HEAD")) {
        http_head(msg);
    } else if(!strcmp(msg->method, "PUT")) {
        http_put(msg);
    } else {
        http_error_reply(msg, 400);
    }

    return;
}

// worker thread data
struct worker {
    int tid;
    int cl;
    pthread_t worker_id;
    struct http_object msg;
    sem_t sem;
    queue available_threads;
};

// worker thread function
void* handle_request(void* thread) {

    struct worker *w_thread = (struct worker *)thread;

    while (true) {

        if(verbose) printf("\033[0;31m[%d] Thread is available.\n\033[0m", w_thread->tid);
        
        while (w_thread->cl < 0) {
            sem_wait(&w_thread->sem);
        }
        
        if(verbose) printf("\033[0;33m[%d] Thread handling request from socket %d\n\033[0m", w_thread->tid, w_thread->cl);

        // read, process, respond to request
        initialize(&w_thread->msg, w_thread->cl);
        read_http_request(&w_thread->msg);
        if(w_thread->msg.status_code == 0) {
            process_request(&w_thread->msg);
        }
        
        
        if(verbose) printf("\033[0;32m[%d] Thread finished request, connection closing with socket %d\n\033[0m", w_thread->tid, w_thread->cl);
        close(w_thread->cl);

        w_thread->cl = INT_MIN;
        enqueue(w_thread->available_threads, w_thread->tid);
    }
}

// return -1 if error, else 1-64
int get_nthreads_arg(char *optarg) {

    if(optarg == NULL) return -1;
    if(strlen(optarg) == 0 || strlen(optarg) > 2) return -1;

    int nthreads = 0;
    for(size_t i = 0; i < strlen(optarg); ++i) {
        if(optarg[i] < '0' || optarg[i] > '9') return -1;
        nthreads = (nthreads * 10) + (optarg[i] - '0');
    }

    if(nthreads == 0 || nthreads > MAX_THREADS) return -1;
    return nthreads;
}

int main(int argc, char** argv) {

    int port = 8080;              // port number (from args)
    char *hostname = "localhost"; // hostname (from args)
    int8_t nthreads = 4;          // number of threads. Defaults 4
    int client_socket = INT_MIN;
    int c = 0;
    opterr = 0;
    
    // parse flags
    while ((c = getopt(argc, argv, "N:v")) != -1) {
        switch (c) {
            case 'N': {
                if ((nthreads = get_nthreads_arg(optarg)) == -1) {
                    warn_exit("error: invalid thread arg\n");
                }
                break;
            }
            case 'v': {
                verbose = true;
                break;
            }
            default: {
                warn_exit(USAGE_MSG);
                return 0;
            }
        }
    }

    // parse hostname and port
    if (optind < argc) {
        for (int index = optind; index < argc; index++) {
            
            char *token = strtok(argv[index], ":");
            
            if ((hostname = token) == NULL) {
                warn_exit(USAGE_MSG);
            }
            if ((token = strtok(NULL, ":")) == NULL) {
                warn_exit(USAGE_MSG);
            }
            if ((port = valid_port(token)) == -1) {
                warn_exit(USAGE_MSG);
            }
        }
    }

    // resolve hostname
    struct hostent *hent = gethostbyname(hostname);
    if (hent == NULL) {
        warn_exit("error: gethostbyname failed.\n");
    }

    // initialize socket
    struct sockaddr_in addr;
    memcpy(&addr.sin_addr.s_addr, hent->h_addr, hent->h_length);
    addr.sin_port = htons(port);
    addr.sin_family = AF_INET;
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    int enable = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
    bind(sock, (struct sockaddr *)&addr, sizeof(addr));
    listen(sock, 128);

    // allocate thread structs and sync queue 
    struct worker *workers = malloc(nthreads * sizeof(struct worker));
    queue available_threads = new_queue(nthreads);

    printf("Starting httpserver... [host: %s port: %d threads: %d]\n", hostname, port, nthreads);

    // initializes threads
    for(int i = 0; i < nthreads; i++) {
        workers[i].cl = INT_MIN;
        workers[i].tid = i;
        workers[i].available_threads = available_threads;
        sem_init(&workers[i].sem, 0, 1);
        
        if (pthread_create(&workers[i].worker_id, NULL, &handle_request, &workers[i]) < 0) {
            warn_exit("error creating a thread.\n");
        }
        enqueue(available_threads, i);
    }

    while (true) {
        if((client_socket = accept(sock, NULL, NULL)) < 0) {
            warn_exit("error connecting to client\n");
        }

        int target_thread = dequeue(available_threads);
        workers[target_thread].cl = client_socket;
        sem_post(&workers[target_thread].sem);
    }

    return EXIT_SUCCESS;
}