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

sem_t sem; // sem for putting dispatcher to sleep
bool verbose = false;

// stores all key data of each http request
struct http_object {
    char method[5];
    char filename[256]; 
    char httpversion[9];
    char buffer[BUFFER_SIZE];
    ssize_t content_length;  
    uint16_t status_code;
    uint16_t extra_bytes; // used when part of body is read while reading header
    uint16_t index;  // used when part of body is read while reading header
};

// sets the status code to 400, 403, 404, or 500 and content length to 0
static void set_err_code(struct http_object* message, uint16_t code) {
    message->status_code = code;
    message->content_length = 0;
    return;
}

// writes contents of file (filename) to cl. (for a GET response)
static void send_body(ssize_t cl, struct http_object* message) {

    int8_t fd = open(message->filename, O_RDONLY);
    ssize_t bytes_read = read(fd, message->buffer, BUFFER_SIZE);
    if(bytes_read == -1) { set_err_code(message, 500); }

    while(bytes_read > 0) {
        ssize_t bytes_written = write(cl, message->buffer, bytes_read);
        if(bytes_written == -1) {
            set_err_code(message, 500);
            break;
        }
        bytes_read = read(fd, message->buffer, BUFFER_SIZE);
        if(bytes_read == -1) {
            set_err_code(message, 500);
            break;
        }
    }
    close(fd);
    return;
}



// sets http_object fields to 0 or null
static void initialize(struct http_object* message) {
    memset(message->method, '\0', sizeof(message->method));
    memset(message->filename, '\0', sizeof(message->filename));
    memset(message->httpversion, '\0', sizeof(message->httpversion));
    memset(message->buffer, '\0', sizeof(message->buffer));
    message->content_length = 0;
    message->status_code = 0;
    message->extra_bytes = 0;
    message->index = 0;
}

// checks if filename is valid, tries to open file, if opens reads file size
static void process_file(struct http_object* message) {

    if(strcmp(message->httpversion, "HTTP/1.1") != 0) {
        set_err_code(message, 400);
        return;
    }
    if(!(valid_filename(message->filename))) {
        set_err_code(message, 400);
        return;
    }
    
    // attempt to open file (errno 2 = no such file/directory)
    int fd = open(message->filename, O_RDONLY);
    if(fd == -1 && errno == 2) {
        set_err_code(message, 404);
        return;
    }
    
    // pull file info using stat
    struct stat fileStat;
    if(stat(message->filename, &fileStat) < 0) {
        set_err_code(message, 500);
        close(fd);
        return;
    }

    // if file is directory (bad req)
    if(S_ISDIR(fileStat.st_mode)) {
        set_err_code(message, 400);
        close(fd);
        return;
    }

    // if file has read perms (owner)
    if(fileStat.st_mode & S_IRUSR) {
        message->status_code = 200;
        message->content_length = fileStat.st_size;
    }
    else {
        set_err_code(message, 403);
        close(fd);
        return;
    }

    close(fd);
    return;
    
}

// read http request
static void read_http_request(ssize_t cl, struct http_object* message) {

    //initialize http_object fields
    initialize(message);
    ssize_t bytes = 0;
    char* double_crlf_ptr = NULL;

    // read stream until either found a double CRLF or read 4 KiB
    while(double_crlf_ptr == NULL && bytes < MAX_HEADER_SIZE) {
        ssize_t bytes_read = read(cl, (void*)(message->buffer + bytes), MAX_HEADER_SIZE - bytes);
        if(bytes_read < 0) {
            set_err_code(message, 500);
            return;
        }
        bytes += bytes_read;
        double_crlf_ptr = strstr(message->buffer, "\r\n\r\n");
    }

    // Bad Request (?) --- header is over 4 KiB
    if(double_crlf_ptr == NULL) {
        set_err_code(message, 400);
        return;
    }
    
    // stores amount of "extra" (body) bytes read along with header
    double_crlf_ptr += 4; 
    message->index = double_crlf_ptr - message->buffer; // aka header size including double crlf
    message->extra_bytes = bytes - message->index; // size of data read in buffer minus header

    // fill http_object fields (method, filename, http version, content-length)
    int8_t var_filled = sscanf(message->buffer, "%s /%s %s", message->method, message->filename, message->httpversion);
    if(var_filled == 3) {
        if(strcmp(message->method, "PUT") == 0) {
            // get content length (for a PUT)
            char* cl_ptr = strstr(message->buffer, "Content-Length:");
            if(cl_ptr != NULL) {
                var_filled = sscanf(cl_ptr, "Content-Length: %zd", &message->content_length);
                if(var_filled != 1) {
                    set_err_code(message, 400);
                    return;
                }
            }
            else {
                set_err_code(message, 400);
                return;
            }
        }
    }
    else {
        set_err_code(message, 400);
        return;
    }
    
    return;
}

// process request
static void process_request(ssize_t cl, struct http_object* message) {

    // error occured at some point previously, return and proceed to send response
    if(message->status_code != 0) {
        return;
    }
    else if(strcmp(message->method, "GET") == 0) {
        process_file(message);
    }
    else if(strcmp(message->method, "HEAD") == 0) {
        process_file(message);
    }
    else if(strcmp(message->method, "PUT") == 0) {

        if(strcmp(message->httpversion, "HTTP/1.1") != 0) {
            set_err_code(message, 400);
            return;
        }
        if(!(valid_filename(message->filename))) {
            set_err_code(message, 400);
            return;
        }
        // create file if doesnt exist, or verify its perms/type if exists
        int fd = open(message->filename, O_WRONLY | O_TRUNC);
        if(fd == -1 && errno == 2) {        
            fd = open(message->filename, O_WRONLY | O_CREAT | O_TRUNC, 0664);
            message->status_code = 201;
        }
        else {
            struct stat fileStat;
            if(stat(message->filename, &fileStat) < 0) {
                set_err_code(message, 500);
                close(fd);
                return;
            }
            // check if file is actually a directory
            if(S_ISDIR(fileStat.st_mode)) {
                set_err_code(message, 400);
                close(fd);
                return;
            }
            // check if file has write perms (owner)
            if(fileStat.st_mode & S_IWUSR) {
                message->status_code = 200;
            }
            else {
                set_err_code(message, 403);
                close(fd);
                return;
            }

        }
        close(fd);

        // since everything seems good so far, send an 100 continue (if expected)
        char* ex_continue_ptr = strstr(message->buffer, "Expect: 100-continue");
        if(ex_continue_ptr != NULL) {
            char continue_resp[50] = "HTTP/1.1 100 Continue\r\n\r\n";
            if( write(cl, continue_resp, strlen(continue_resp)) == -1 ) {
                set_err_code(message, 500);
                return;
            }
        }
    }
    else {
        set_err_code(message, 400);
    }

    // write data (for a put request)
    if(strcmp(message->method, "PUT") == 0 && (message->status_code == 200 || message->status_code == 201)) {
        
        int fd = open(message->filename, O_WRONLY);
        if(fd < 0) {
            set_err_code(message, 500);
            close(fd);
            return;
        }
        
        // read data from server write 2 file
        ssize_t write_count = 0;
        ssize_t bytes_needed = message->content_length;

        // if there are bytes left over from first read
        if(message->extra_bytes) {
            char* request_body = message->buffer + message->index;
            if (bytes_needed < message->extra_bytes) {
                message->extra_bytes = bytes_needed;
            }
            write_count = write(fd, request_body, message->extra_bytes);
            if(write_count == -1) {
                set_err_code(message, 500);
                close(fd);
                return;
            }
            bytes_needed -= write_count;
        }
        while(bytes_needed > 0) {
            ssize_t read_count = read(cl, message->buffer, BUFFER_SIZE);
            if(read_count == -1) {
                set_err_code(message, 500);
                close(fd);
                return;
            }
            if(read_count > bytes_needed) {
                write_count = write(fd, message->buffer, bytes_needed);
                if(write_count == -1) {
                    set_err_code(message, 500);
                    close(fd);
                    return;
                }
                bytes_needed -= write_count;
            }
            else {
                write_count = write(fd, message->buffer, read_count);
                if(write_count == -1) {
                    set_err_code(message, 500);
                    close(fd);
                    return;
                }
                bytes_needed -= write_count;
            }
        }
        close(fd);
    }

    return;
}

// construct response
static void construct_http_response(ssize_t cl, struct http_object* message) {

    char status_message[30];
    switch (message->status_code) {
        case 200:
            strcpy(status_message, "OK");
            break;
        case 201:
            strcpy(status_message, "Created");
            break;
        case 400:
            strcpy(status_message, "Bad Request");
            break;
        case 403:
            strcpy(status_message, "Forbidden");
            break;
        case 404:
            strcpy(status_message, "Not Found");
            break;
        default:
            strcpy(status_message, "Internal Server Error");
            message->status_code = 500;
    }

    // clear buffer
    memset(message->buffer, '\0', sizeof(message->buffer));

    ssize_t content_len = 0;
    if(strcmp(message->method, "GET") == 0 || strcmp(message->method, "HEAD") == 0) {
        content_len = message->content_length;
    }
    sprintf(message->buffer, "HTTP/1.1 %d %s\r\nContent-Length: %zu\r\n\r\n", message->status_code, status_message, content_len);
    ssize_t bytes_written = write(cl, message->buffer, strlen(message->buffer));
    if(bytes_written < 0) {
        set_err_code(message, 500);
        return;
    }

    // if we have a (proper) GET request, send the data aka body of response
    if(strcmp(message->method, "GET") == 0 && message->status_code == 200) {
        send_body(cl, message);
    }
    
    return;
}

// worker thread data
struct worker {
    int id;
    int cl;
    pthread_t worker_id;
    struct http_object message;
    pthread_cond_t condition_var;
    pthread_mutex_t* lock;
    queue available_threads;
};

// worker thread function
void* handle_request(void* thread) {
    struct worker *w_thread = (struct worker *)thread;
    while (true) {

        if(verbose) printf("\033[0;31m[%d] Thread is available.\n\033[0m", w_thread->id);
        pthread_mutex_lock(w_thread->lock);
        while (w_thread->cl < 0) {
            pthread_cond_wait(&w_thread->condition_var, w_thread->lock);
        }
        pthread_mutex_unlock(w_thread->lock);
        if(verbose) printf("\033[0;33m[%d] Thread handling request from socket %d\n\033[0m", w_thread->id, w_thread->cl);

        // read, process, respond to request
        read_http_request(w_thread->cl, &w_thread->message);
        process_request(w_thread->cl, &w_thread->message);
        construct_http_response(w_thread->cl, &w_thread->message);
        
        if(verbose) printf("\033[0;32m[%d] Thread finished request, connection closing with socket %d\n\033[0m", w_thread->id, w_thread->cl);
        close(w_thread->cl);
        w_thread->cl = INT_MIN;
        pthread_mutex_lock(w_thread->lock);
        enqueue(w_thread->available_threads, w_thread->id);
        pthread_mutex_unlock(w_thread->lock);
        sem_post(&sem);
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

    int port = 8080;        // port number (from args)
    char *hostname = "localhost"; // hostname (from args)
    int8_t nthreads = 4;      // number of threads. Defaults 4
    
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

    struct sockaddr_in addr;
    memcpy(&addr.sin_addr.s_addr, hent->h_addr, hent->h_length);
    addr.sin_port = htons(port);
    addr.sin_family = AF_INET;
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    int enable = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
    bind(sock, (struct sockaddr *)&addr, sizeof(addr));
    listen(sock, 128);

    struct worker *workers = malloc(nthreads * sizeof(struct worker));
    queue available_threads = new_queue(nthreads);
    sem_init(&sem, 0, nthreads);
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

    printf("Starting httpserver... [host: %s port: %d threads: %d]\n", hostname, port, nthreads);

    // initializes threads
    for(int i = 0; i < nthreads; i++) {
        workers[i].cl = INT_MIN;
        workers[i].id = i;
        int ret = pthread_cond_init(&workers[i].condition_var, NULL);
        if(ret != 0) warn("wtf"); // debug
        workers[i].lock = &lock;
        workers[i].available_threads = available_threads;
        
        int err = pthread_create(&workers[i].worker_id, NULL, &handle_request, &workers[i]);
        if (err < 0) {
            fprintf(stderr, "error creating a thread.\n");
        }
        enqueue(available_threads, i);
    }

    while (true) {
        int cl = accept(sock, NULL, NULL);
        if (cl < 0) { err(EXIT_FAILURE, "error connecting to client\n"); }

        sem_wait(&sem);
        pthread_mutex_lock(&lock);
        int target_thread = dequeue(available_threads);
        workers[target_thread].cl = cl;
        pthread_cond_signal(&workers[target_thread].condition_var);
        pthread_mutex_unlock(&lock);
    }

    return EXIT_SUCCESS;
}