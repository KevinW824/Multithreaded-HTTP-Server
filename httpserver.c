#include "helper_funcs.h"
#include "connection.h"
#include "response.h"
#include "request.h"
#include "queue.h"

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include <sys/stat.h>
#include <sys/file.h>

void *do_work();

void handle_connection(int);
void handle_get(conn_t *);
void handle_put(conn_t *);
void handle_unsupported(conn_t *);

queue_t *conn_queue = NULL;

pthread_mutex_t global_lock = PTHREAD_MUTEX_INITIALIZER;

int main(int argc, char **argv) {
    if (argc < 2) {
        warnx("wrong arguments: %s port_num", argv[0]);
        fprintf(stderr, "usage: %s [-t threads] <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int opt, num_threads = 4;
    while ((opt = getopt(argc, argv, "t:")) != -1) {
        switch (opt) {
        case 't': num_threads = atoi(optarg); break;
        }
    }

    char *endptr = NULL;
    size_t port;
    if (optind < argc) {
        port = (size_t) strtoull(argv[optind], &endptr, 10);
        if (endptr && *endptr != '\0') {
            warnx("invalid port number: %s", argv[1]);
            return EXIT_FAILURE;
        }
    } else {
        fprintf(stderr, "Usage: %s [-t threads] <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // printf("Listening on port %zu with %d threads\n", port, num_threads);

    signal(SIGPIPE, SIG_IGN);
    Listener_Socket sock;
    listener_init(&sock, port);

    pthread_t threads[num_threads];
    for (int i = 0; i < num_threads; i++) {
        pthread_create(&threads[i], NULL, &do_work, NULL);
    }

    conn_queue = queue_new(num_threads);

    // dispatcher
    while (1) {
        uintptr_t connfd = listener_accept(&sock);
        queue_push(conn_queue, (void *) connfd);
    }

    return EXIT_SUCCESS;
}

void *do_work() {
    while (1) {
        uintptr_t fd;
        queue_pop(conn_queue, (void *) &fd);
        handle_connection(fd);
        close(fd);
    }
}

void handle_connection(int connfd) {

    conn_t *conn = conn_new(connfd);

    const Response_t *res = conn_parse(conn);

    if (res != NULL) {
        conn_send_response(conn, res);
    } else {
        const Request_t *req = conn_get_request(conn);
        if (req == &REQUEST_GET) {
            handle_get(conn);
        } else if (req == &REQUEST_PUT) {
            handle_put(conn);
        } else {
            handle_unsupported(conn);
        }
    }

    conn_delete(&conn);
}

void handle_get(conn_t *conn) {

    char *uri = conn_get_uri(conn);
    const Response_t *res = NULL;
    char *id = NULL;

    // What are the steps in here?

    // 1. Open the file.
    // If  open it returns < 0, then use the result appropriately
    //   a. Cannot access -- use RESPONSE_FORBIDDEN
    //   b. Cannot find the file -- use RESPONSE_NOT_FOUND
    //   c. other error? -- use RESPONSE_INTERNAL_SERVER_ERROR
    // (hint: check errno for these cases)!

    // mutex lock before open
    pthread_mutex_lock(&global_lock);

    int fd = open(uri, O_RDONLY, 0600);
    if (fd < 0) {
        if (errno == EACCES || errno == EISDIR) {
            res = &RESPONSE_FORBIDDEN;
            pthread_mutex_unlock(&global_lock);
            goto out;
        } else if (errno == ENOENT) {
            res = &RESPONSE_NOT_FOUND;
            pthread_mutex_unlock(&global_lock);
            goto out;
        } else {
            res = &RESPONSE_INTERNAL_SERVER_ERROR;
            pthread_mutex_unlock(&global_lock);
            goto out;
        }
    }

    // flock lock lock_sh reader lock
    if (flock(fd, LOCK_SH) < 0) {
        res = &RESPONSE_INTERNAL_SERVER_ERROR;
        goto out;
    }

    // unlock mutex
    pthread_mutex_unlock(&global_lock);

    // 2. Get the size of the file.
    // (hint: checkout the function fstat)!
    struct stat st;
    if (fstat(fd, &st) < 0) {
        res = &RESPONSE_INTERNAL_SERVER_ERROR;
        goto out;
    }
    off_t file_size = st.st_size;

    // 3. Check if the file is a directory, because directories *will*
    // open, but are not valid.
    // (hint: checkout the macro "S_IFDIR", which you can use after you call fstat!)
    if (S_ISDIR(st.st_mode)) {
        res = &RESPONSE_FORBIDDEN;
        goto out;
    }

    // 4. Send the file
    // (hint: checkout the conn_send_file function!)
    res = conn_send_file(conn, fd, file_size);

    if (res == NULL) {
        res = &RESPONSE_OK;
    }

out:
    if (res != &RESPONSE_OK) {
        conn_send_response(conn, res);
    }
    id = conn_get_header(conn, "Request-Id");
    if (id == NULL)
        id = "0";
    fprintf(stderr, "GET,/%s,%d,%s\n", uri, response_get_code(res), id);
    close(fd);
}

void handle_unsupported(conn_t *conn) {

    // send responses
    conn_send_response(conn, &RESPONSE_NOT_IMPLEMENTED);
}

void handle_put(conn_t *conn) {

    char *uri = conn_get_uri(conn);
    const Response_t *res = NULL;
    char *id = NULL;

    // lock before accessing file
    pthread_mutex_lock(&global_lock);

    // Check if file already exists before opening it.
    bool existed = access(uri, F_OK) == 0;

    if (!existed)
        res = &RESPONSE_CREATED;

    // Open the file..
    int fd = open(uri, O_CREAT | O_WRONLY, 0600);
    if (fd < 0) {
        if (errno == EACCES || errno == EISDIR || errno == ENOENT) {
            res = &RESPONSE_FORBIDDEN;
            pthread_mutex_unlock(&global_lock);
            goto out;
        } else {
            res = &RESPONSE_INTERNAL_SERVER_ERROR;
            pthread_mutex_unlock(&global_lock);
            goto out;
        }
    }

    // flock writer lock
    if (flock(fd, LOCK_EX) < 0) {
        res = &RESPONSE_INTERNAL_SERVER_ERROR;
        goto out;
    }

    // unlock mutex
    pthread_mutex_unlock(&global_lock);

    // RESPONSE 200 or 201
    ftruncate(fd, 0);

    res = conn_recv_file(conn, fd);

    if (res == NULL && existed) {
        res = &RESPONSE_OK;
    } else if (res == NULL && !existed) {
        res = &RESPONSE_CREATED;
    }

out:
    conn_send_response(conn, res);
    id = conn_get_header(conn, "Request-Id");
    if (id == NULL)
        id = "0";
    fprintf(stderr, "PUT,/%s,%d,%s\n", uri, response_get_code(res), id);
    close(fd);
}
