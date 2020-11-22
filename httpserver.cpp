#include "queue.h"
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
#define BUFFER_SIZE 4096
#define MAX_HEADER_SIZE 4096

sem_t sem; // sem for putting dispatcher to sleep
pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER; // LOG FILE INDEX LOCK
u_int64_t LOG_INDEX = 0;   // GLOBAL INDEX OF UP TO WHERE SPACE IS RESERVED ON THE LOG FILE
char* LOG_FILE_NAME;       // NAME OF LOG FILE ARGUMENT GIVEN
bool LOGGING = false;      // WETHER OR NOT USER WANTS TO LOG
bool verbose = false;

// stores all key data of each http request
struct httpObject {
    char method[5];
    char filename[256]; 
    char httpversion[9];
    char buffer[BUFFER_SIZE];
    ssize_t content_length;  
    uint16_t status_code;
    uint16_t extra_bytes; // used when part of body is read while reading header
    uint16_t index;  // used when part of body is read while reading header
    u_int64_t log_start_index; // index of where this request will be logged
};

// sets the status code to 400, 403, 404, or 500 and content length to 0
static void SET_ERR_CODE(struct httpObject* message, uint16_t code) {
    message->status_code = code;
    message->content_length = 0;
    return;
}

// writes contents of file (filename) to cl. (for a GET response)
static void send_body(ssize_t cl, struct httpObject* message) {

    int8_t fd = open(message->filename, O_RDONLY);
    ssize_t bytes_read = read(fd, message->buffer, BUFFER_SIZE);
    if(bytes_read == -1) {
        SET_ERR_CODE(message, 500);
    }

    while(bytes_read > 0) {

        ssize_t bytes_written = write(cl, message->buffer, bytes_read);
        if(bytes_written == -1) {
            SET_ERR_CODE(message, 500);
            break;
        }

        bytes_read = read(fd, message->buffer, BUFFER_SIZE);
        if(bytes_read == -1) {
            SET_ERR_CODE(message, 500);
            break;
        }
    }

    close(fd);
    return;

}

// checks if filename is valid (according to class specs)
static bool validFilename(char* s) {
    if(s[27] != '\0') return false;
    for(int i = 0; i < (int)(strlen(s)); i++) {
        if(!((s[i] == '-') || (s[i] == '.')
            || (s[i] == '_')
            || (s[i] >= '0' && s[i] <= '9')
            || (s[i] >= 'A' && s[i] <= 'Z')
            || (s[i] >= 'a' && s[i] <= 'z'))) {
            return false;
        }
    }
    return true;
}

// sets httpObject fields to 0 or null
static void initialize(struct httpObject* message) {
    memset(message->method, '\0', sizeof(message->method));
    memset(message->filename, '\0', sizeof(message->filename));
    memset(message->httpversion, '\0', sizeof(message->httpversion));
    memset(message->buffer, '\0', sizeof(message->buffer));
    message->content_length = 0;
    message->status_code = 0;
    message->extra_bytes = 0;
    message->index = 0;
    message->log_start_index = 0;
}

// ** checks if filename is valid, tries to open file, if opens reads file size
static void process_file(struct httpObject* message) {

    if(strcmp(message->httpversion, "HTTP/1.1") != 0) {
        SET_ERR_CODE(message, 400);
        return;
    }
    if(!(validFilename(message->filename))) {
        SET_ERR_CODE(message, 400);
        return;
    }
    
    // attempt to open file (errno 2 = no such file/directory)
    int fd = open(message->filename, O_RDONLY);
    if(fd == -1 && errno == 2) {
        SET_ERR_CODE(message, 404);
        return;
    }
    
    // pull file info using stat
    struct stat fileStat;
    if(stat(message->filename, &fileStat) < 0) {
        SET_ERR_CODE(message, 500);
        close(fd);
        return;
    }

    // if file is directory (bad req)
    if(S_ISDIR(fileStat.st_mode)) {
        SET_ERR_CODE(message, 400);
        close(fd);
        return;
    }

    // if file has read perms (owner)
    if(fileStat.st_mode & S_IRUSR) {
        message->status_code = 200;
        message->content_length = fileStat.st_size;
    }
    else {
        SET_ERR_CODE(message, 403);
        close(fd);
        return;
    }

    close(fd);
    return;
    
}

// **
static void read_http_request(ssize_t cl, struct httpObject* message) {

    //initialize httpObject fields
    initialize(message);

    ssize_t bytes = 0;
    char* double_crlf_ptr = NULL;

    // read stream until either found a double CRLF or read 4 KiB
    while(double_crlf_ptr == NULL && bytes < MAX_HEADER_SIZE) {
        ssize_t bytes_read = read(cl, (void*)(message->buffer + bytes), MAX_HEADER_SIZE - bytes);
        if(bytes_read < 0) {
            SET_ERR_CODE(message, 500);
            return;
        }
        bytes += bytes_read;
        double_crlf_ptr = strstr(message->buffer, "\r\n\r\n");
    }

    // Bad Request (?) --- header is over 4 KiB
    if(double_crlf_ptr == NULL) {
        SET_ERR_CODE(message, 400);
        return;
    }
    
    // stores amount of "extra" (body) bytes read along with header
    double_crlf_ptr += 4; 
    message->index = double_crlf_ptr - message->buffer; // aka header size including double crlf
    message->extra_bytes = bytes - message->index; // size of data read in buffer minus header

    // fill httpObject fields (method, filename, http version, content-length)
    int8_t var_filled = sscanf(message->buffer, "%s /%s %s", message->method, message->filename, message->httpversion);
    if(var_filled == 3) {
        if(strcmp(message->method, "PUT") == 0) {
            // get content length (for a PUT)
            char* cl_ptr = strstr(message->buffer, "Content-Length:");
            if(cl_ptr != NULL) {
                var_filled = sscanf(cl_ptr, "Content-Length: %zd", &message->content_length);
                if(var_filled != 1) {
                    SET_ERR_CODE(message, 400);
                    return;
                }
            }
            else {
                SET_ERR_CODE(message, 400);
                return;
            }
        }
    }
    else {
        SET_ERR_CODE(message, 400);
        return;
    }
    
    return;
}

// **
static void process_request(ssize_t cl, struct httpObject* message) {

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
            SET_ERR_CODE(message, 400);
            return;
        }
        if(!(validFilename(message->filename))) {
            SET_ERR_CODE(message, 400);
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
                SET_ERR_CODE(message, 500);
                close(fd);
                return;
            }
            // check if file is actually a directory
            if(S_ISDIR(fileStat.st_mode)) {
                SET_ERR_CODE(message, 400);
                close(fd);
                return;
            }
            // check if file has write perms (owner)
            if(fileStat.st_mode & S_IWUSR) {
                message->status_code = 200;
            }
            else {
                SET_ERR_CODE(message, 403);
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
                SET_ERR_CODE(message, 500);
                return;
            }
        }
    }
    else {
        SET_ERR_CODE(message, 400);
    }

    // write data (for a put request)
    if(strcmp(message->method, "PUT") == 0 && (message->status_code == 200 || message->status_code == 201)) {
        
        int fd = open(message->filename, O_WRONLY);
        if(fd < 0) {
            SET_ERR_CODE(message, 500);
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
                SET_ERR_CODE(message, 500);
                close(fd);
                return;
            }
            bytes_needed -= write_count;
        }
        while(bytes_needed > 0) {
            ssize_t read_count = read(cl, message->buffer, BUFFER_SIZE);
            if(read_count == -1) {
                SET_ERR_CODE(message, 500);
                close(fd);
                return;
            }
            if(read_count > bytes_needed) {
                write_count = write(fd, message->buffer, bytes_needed);
                if(write_count == -1) {
                    SET_ERR_CODE(message, 500);
                    close(fd);
                    return;
                }
                bytes_needed -= write_count;
            }
            else {
                write_count = write(fd, message->buffer, read_count);
                if(write_count == -1) {
                    SET_ERR_CODE(message, 500);
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

// **
static void construct_http_response(ssize_t cl, struct httpObject* message) {

    char* status_message;
    switch (message->status_code) {
        case 200:
            status_message = "OK";
            break;
        case 201:
            status_message = "Created";
            break;
        case 400:
            status_message = "Bad Request";
            break;
        case 403:
            status_message = "Forbidden";
            break;
        case 404:
            status_message = "Not Found";
            break;
        case 500:
            status_message = "Internal Server Error";
            break;
        default:
            status_message = "Internal Server Error";
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
        SET_ERR_CODE(message, 500);
        return;
    }

    // if we have a (proper) GET request, send the data aka body of response
    if(strcmp(message->method, "GET") == 0 && message->status_code == 200) {
        send_body(cl, message);
    }
    
    return;
}

// **
static void WRITE_TO_LOGFILE(struct httpObject* message) {

    if(message->status_code == 200 || message->status_code == 201) {
        // CALCULATE SPACE NEEDED ON LOG FILE
        ssize_t offset = 9; // for ========\n
        ssize_t data_lines_needed = 0;
        if(strcmp(message->method, "PUT") == 0 || strcmp(message->method, "GET") == 0) {
            u_int8_t overflow_bytes = message->content_length % 20;
            if(overflow_bytes != 0) {
                data_lines_needed += 1;
            }
            data_lines_needed += (message->content_length / 20);
            offset += (9 * data_lines_needed); // for ======== ..... \n on each line of hex data
            offset += (3 * message->content_length); // since each byte of data turns into " XX" 
        }
        offset += strlen(message->method) + 1; // for "GET " , "HEAD " , or "PUT " 
        offset += strlen(message->filename) + 2; // for "/FILENAME "
        offset += 8; // for "length ... \n"
        char snum[20];
        sprintf(snum, "%ld", message->content_length);
        offset += strlen(snum); // for the value X in "length X\n"
        // LOCK MUTEX AND ADJUST LOG INDICES
        pthread_mutex_lock(&log_lock);
        message->log_start_index = LOG_INDEX;
        LOG_INDEX += offset;
        pthread_mutex_unlock(&log_lock);
        // LOGS HEADER INTO LOG FILE
        int log_fd = open(LOG_FILE_NAME, O_WRONLY);
        if(log_fd < 0) {
            fprintf(stderr, "ERROR: writing to log file failed, couldnt open log file.\n");
            return;
        }
        memset(message->buffer, '\0', sizeof(message->buffer));
        sprintf(message->buffer, "%s /%s length %ld\n", message->method, message->filename, message->content_length);
        ssize_t log_bytes_written = pwrite(log_fd, message->buffer, strlen(message->buffer), message->log_start_index);
        if(log_bytes_written == -1) {
            fprintf(stderr, "ERROR: writing to log file failed, pwrite() returned -1.\n");
            close(log_fd);
            return;
        }
        message->log_start_index += log_bytes_written;
        if(strcmp(message->method, "GET") == 0 || strcmp(message->method, "PUT") == 0) {
            // NEED TO LOG DATA AND END LINE
            ssize_t bytes_read_log = 0;
            // CLEAR BUFFER AND OPEN FILE SUBJECT TO THE GET REQUEST
            memset(message->buffer, '\0', sizeof(message->buffer));
            int fd = open(message->filename, O_RDONLY);
            if(fd < 0) {
                fprintf(stderr, "ERROR: writing to log file failed, open() failed.\n");
                close(log_fd);
                return;
            }
            // READ FROM FILE UNTIL EOF
            ssize_t bytes_read = read(fd, message->buffer, BUFFER_SIZE);
            if(bytes_read == -1) {
                fprintf(stderr, "ERROR: writing to log file failed, pwrite() returned -1.\n");
                close(log_fd);
                close(fd);
                return;
            }
            while(bytes_read) {
                for(int i = 0; i < bytes_read; i+= 20) {
                    // write 00000000
                    char buffer[9];
                    sprintf(buffer, "%08d", (uint16_t)bytes_read_log);
                    if(pwrite(log_fd, buffer, strlen(buffer), message->log_start_index) == -1) {
                        fprintf(stderr, "ERROR: writing to log file failed, pwrite() returned -1.\n");
                        close(log_fd);
                        close(fd);
                        return;
                    }
                    message->log_start_index += 8;
                    // write (up to) 20 hex values
                    int j = i;
                    while(j < i+20 && j < bytes_read) {
                        sprintf(buffer, " %02x", (uint8_t)message->buffer[j]);
                        if(pwrite(log_fd, buffer, 3, message->log_start_index) == -1) {
                            fprintf(stderr, "ERROR: writing to log file failed, pwrite() returned -1.\n");
                            close(log_fd);
                            close(fd);
                            return;
                        }
                        message->log_start_index += 3;
                        j++;
                        bytes_read_log++;
                    }
                    // write \n
                    char* new_line = "\n";
                    if(pwrite(log_fd, new_line, 1, message->log_start_index) == -1) {
                        fprintf(stderr, "ERROR: writing to log file failed, pwrite() returned -1.\n");
                        close(log_fd);
                        close(fd);
                        return;
                    }
                    message->log_start_index += 1;
                }
                bytes_read = read(fd, message->buffer, BUFFER_SIZE);
                if(bytes_read == -1) {
                    fprintf(stderr, "ERROR: writing to log file failed, read() returned -1.\n");
                    close(log_fd);
                    close(fd);
                    return;
                }
            }
            // LOG END LINE AND RETURN ========\n
            char* end_line = "========\n";
            if(pwrite(log_fd, end_line, strlen(end_line), message->log_start_index) == -1) {
                fprintf(stderr, "ERROR: writing to log file failed, read() returned -1.\n");
            }
            close(log_fd);
            close(fd);
            return;
        }
        else if(strcmp(message->method, "HEAD") == 0) {
            char* end_line = "========\n";
            log_bytes_written = pwrite(log_fd, end_line, strlen(end_line), message->log_start_index);
            if(log_bytes_written == -1) {
                fprintf(stderr, "ERROR: writing to log file failed, pwrite() returned -1.\n");
            }
            close(log_fd);
            return;
        }
    }
    else {
        // CALCULATE SPACE NEEDED IN LOG FILE
        ssize_t offset = 9; // for ========\n
        offset += 6; // for "FAIL: "
        offset += strlen(message->method) + 1; // for "GET " , "HEAD " , or "PUT " 
        offset += strlen(message->filename) + 2; // for "/FILENAME "
        offset += strlen(message->httpversion); // for "HTTPVERSION"
        offset += 18; // for " --- response ...\n"
        // LOCK MUTEX AND ADJUST LOG INDICES
        pthread_mutex_lock(&log_lock);
        message->log_start_index = LOG_INDEX;
        LOG_INDEX += offset;
        pthread_mutex_unlock(&log_lock);
        // LOG (FAILED REQUEST)
        int log_fd = open(LOG_FILE_NAME, O_WRONLY);
        if(log_fd == -1) {
            fprintf(stderr, "ERROR: writing to log file failed, open() returned -1.\n");
            return;
        }
        memset(message->buffer, '\0', sizeof(message->buffer));
        sprintf(message->buffer, "FAIL: %s /%s %s --- response %d\n========\n", message->method, message->filename, message->httpversion, message->status_code);
        ssize_t log_bytes_written = pwrite(log_fd, message->buffer, strlen(message->buffer), message->log_start_index);
        if(log_bytes_written == -1) {
            fprintf(stderr, "ERROR: writing to log file failed, pwrite() returned -1.\n");
        }
        close(log_fd);
        return;
    }
}

// ** "neccessary" stuff for the threads
struct worker {
    int id;
    int cl;
    pthread_t worker_id;
    struct httpObject message;
    pthread_cond_t condition_var;
    pthread_mutex_t* lock;
    queue available_threads;
};

// **
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
        
        if(LOGGING) { WRITE_TO_LOGFILE(&w_thread->message); }
        if(verbose) printf("\033[0;32m[%d] Thread finished request, connection closing with socket %d\n\033[0m", w_thread->id, w_thread->cl);
        close(w_thread->cl);
        w_thread->cl = INT_MIN;
        pthread_mutex_lock(w_thread->lock);
        enqueue(w_thread->available_threads, w_thread->id);
        pthread_mutex_unlock(w_thread->lock);
        sem_post(&sem);
    }
}

// **
int main(int argc, char** argv) {

    char *PORT_NUMBER;        // port number (from args)
    char *SERVER_NAME_STRING; // hostname (from args)
    int NUM_THREADS = 4;      // number of threads. Defaults 4
    int c = 0;
    opterr = 0;
    while ((c = getopt(argc, argv, "N:l:v")) != -1) {
        switch (c) {
        case 'N':
        NUM_THREADS = atoi(optarg);
        break;
        case 'l':
        LOG_FILE_NAME = optarg;
        LOGGING = true;
        break;
        case 'v':
        verbose = true;
        break;
        default:
        fprintf(stderr, "usage: ./httpserver <hostname:port> [-N num_threads] [-l log_file_name] [-v verbose]\n");
        return EXIT_FAILURE;
        }
    }
    int index = optind;
    if (index >= argc || NUM_THREADS > 100 || NUM_THREADS <= 0) {
        fprintf(stderr, "usage: ./httpserver <hostname:port> [-N num_threads] [-l log_file_name] [-v verbose]\n");
        return EXIT_FAILURE;
    } else {
        for (index = optind; index < argc; index++) {
            char *token = strtok(argv[index], ":");
            if (token == NULL) {
                fprintf(stderr, "usage: ./httpserver <hostname:port> [-N num_threads] [-l log_file_name] [-v verbose]\n");
                return EXIT_FAILURE;
            }
            SERVER_NAME_STRING = token;
            token = strtok(NULL, ":");
            if (token == NULL) {
                fprintf(stderr, "usage: ./httpserver <hostname:port> [-N num_threads] [-l log_file_name] [-v verbose]\n");
                return EXIT_FAILURE;
            }
            PORT_NUMBER = token;
            int PORT_NUMBER_len = (int)strlen(PORT_NUMBER);
            for (int i = 0; i < PORT_NUMBER_len; i++) {
                if (PORT_NUMBER[i] < '0' || PORT_NUMBER[i] > '9') {
                fprintf(stderr, "usage: ./httpserver <hostname:port> [-N num_threads] [-l log_file_name] [-v verbose]\n");
                return EXIT_FAILURE;
                }
            }
        }
    }
    if(LOGGING) {
        int log_fd = open(LOG_FILE_NAME, O_RDWR | O_CREAT | O_TRUNC, 0664);
        if(log_fd == -1) {
            warn("error creating log file\n");
            return EXIT_FAILURE;
        }
        close(log_fd);
        LOGGING = true;
    }
    
    struct hostent *hent = gethostbyname(SERVER_NAME_STRING);
    if (hent == NULL) {
        fprintf(stderr, "Error: gethostbyname failed.\n");
        return EXIT_FAILURE;
    }
    struct sockaddr_in addr;
    memcpy(&addr.sin_addr.s_addr, hent->h_addr, hent->h_length);
    addr.sin_port = htons(atoi(PORT_NUMBER));
    addr.sin_family = AF_INET;
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    int enable = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
    bind(sock, (struct sockaddr *)&addr, sizeof(addr));
    listen(sock, 128);

    struct worker *workers = (worker *)malloc(NUM_THREADS * sizeof(worker));
    queue available_threads = new_queue();
    sem_init(&sem, 0, NUM_THREADS);
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;   // initializes worker mutex

    printf("Starting httpserver... (threads: %d)\n", NUM_THREADS);

    // initializes threads
    for(int i = 0; i < NUM_THREADS; i++) {
        workers[i].cl = INT_MIN;
        workers[i].id = i;
        workers[i].condition_var = PTHREAD_COND_INITIALIZER;
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