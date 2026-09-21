#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define MAX_HEADER_SIZE 65536

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

typedef struct cache_entry {
    char key[MAXLINE];
    unsigned char *data;
    size_t size;
    struct cache_entry *prev;
    struct cache_entry *next;
} cache_entry_t;

typedef struct {
    char host[MAXLINE];
    char port[16];
    char path[MAXLINE];
    char host_header[MAXLINE];
} url_t;

static cache_entry_t *cache_head;
static cache_entry_t *cache_tail;
static size_t cache_size;
static pthread_mutex_t cache_mutex = PTHREAD_MUTEX_INITIALIZER;

static void *thread(void *vargp);
static void handle_client(int clientfd);
static int parse_url(const char *uri, url_t *url);
static int build_request_headers(rio_t *client_rio, const url_t *url,
                                 char *headers, size_t capacity);
static int append_text(char *dst, size_t capacity, const char *src);
static void send_error(int fd, const char *status, const char *shortmsg,
                       const char *longmsg);
static int cache_get(const char *key, unsigned char **data, size_t *size);
static void cache_put(const char *key, const unsigned char *data, size_t size);
static void cache_remove(cache_entry_t *entry);
static void cache_insert_front(cache_entry_t *entry);

int main(int argc, char **argv)
{
    int listenfd;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        return 1;
    }

    Signal(SIGPIPE, SIG_IGN);
    listenfd = open_listenfd(argv[1]);
    if (listenfd < 0) {
        fprintf(stderr, "Unable to listen on port %s\n", argv[1]);
        return 1;
    }

    while (1) {
        struct sockaddr_storage clientaddr;
        socklen_t clientlen = sizeof(clientaddr);
        pthread_t tid;
        int *clientfdp = malloc(sizeof(int));

        if (clientfdp == NULL) {
            continue;
        }
        *clientfdp = accept(listenfd, (SA *)&clientaddr, &clientlen);
        if (*clientfdp < 0) {
            free(clientfdp);
            if (errno == EINTR) {
                continue;
            }
            continue;
        }
        if (pthread_create(&tid, NULL, thread, clientfdp) != 0) {
            close(*clientfdp);
            free(clientfdp);
        }
    }
}

static void *thread(void *vargp)
{
    int clientfd = *(int *)vargp;

    free(vargp);
    pthread_detach(pthread_self());
    handle_client(clientfd);
    close(clientfd);
    return NULL;
}

static void handle_client(int clientfd)
{
    char request_line[MAXLINE];
    char method[32];
    char uri[MAXLINE];
    char version[32];
    char headers[MAX_HEADER_SIZE];
    char cache_key[MAXLINE];
    char io_buf[MAXBUF];
    unsigned char object[MAX_OBJECT_SIZE];
    unsigned char *cached_data;
    size_t cached_size;
    size_t object_size = 0;
    int cacheable = 1;
    int serverfd;
    ssize_t n;
    rio_t client_rio;
    rio_t server_rio;
    url_t url;

    rio_readinitb(&client_rio, clientfd);
    if (rio_readlineb(&client_rio, request_line, sizeof(request_line)) <= 0) {
        return;
    }

    if (sscanf(request_line, "%31s %8191s %31s", method, uri, version) != 3) {
        send_error(clientfd, "400", "Bad Request", "Malformed request line");
        return;
    }
    if (strcasecmp(method, "GET") != 0) {
        send_error(clientfd, "501", "Not Implemented",
                   "The proxy only supports GET requests");
        return;
    }
    if (parse_url(uri, &url) < 0) {
        send_error(clientfd, "400", "Bad Request", "Invalid HTTP URL");
        return;
    }
    if (build_request_headers(&client_rio, &url, headers, sizeof(headers)) < 0) {
        send_error(clientfd, "431", "Request Header Fields Too Large",
                   "The request headers are too large");
        return;
    }
    if (snprintf(cache_key, sizeof(cache_key), "%s:%s%s", url.host, url.port,
                 url.path) >= (int)sizeof(cache_key)) {
        send_error(clientfd, "414", "URI Too Long", "The requested URL is too long");
        return;
    }

    if (cache_get(cache_key, &cached_data, &cached_size)) {
        rio_writen(clientfd, cached_data, cached_size);
        free(cached_data);
        return;
    }

    serverfd = open_clientfd(url.host, url.port);
    if (serverfd < 0) {
        send_error(clientfd, "502", "Bad Gateway", "Unable to connect to origin server");
        return;
    }

    if (rio_writen(serverfd, headers, strlen(headers)) < 0) {
        close(serverfd);
        send_error(clientfd, "502", "Bad Gateway", "Unable to send request to origin server");
        return;
    }

    rio_readinitb(&server_rio, serverfd);
    while ((n = rio_readnb(&server_rio, io_buf, sizeof(io_buf))) > 0) {
        rio_writen(clientfd, io_buf, (size_t)n);
        if (cacheable && object_size + (size_t)n <= MAX_OBJECT_SIZE) {
            memcpy(object + object_size, io_buf, (size_t)n);
            object_size += (size_t)n;
        } else {
            cacheable = 0;
        }
    }
    close(serverfd);

    if (n == 0 && cacheable) {
        cache_put(cache_key, object, object_size);
    }
}

static int parse_url(const char *uri, url_t *url)
{
    const char *authority;
    const char *path_start;
    char authority_buf[MAXLINE];
    char *colon;
    char *endptr;
    long port_number;
    size_t authority_len;
    size_t host_len;
    size_t port_len;

    if (strncasecmp(uri, "http://", 7) != 0) {
        return -1;
    }
    authority = uri + 7;
    path_start = strpbrk(authority, "/?#");
    authority_len = path_start == NULL ? strlen(authority)
                                       : (size_t)(path_start - authority);
    if (authority_len == 0 || authority_len >= sizeof(authority_buf)) {
        return -1;
    }
    memcpy(authority_buf, authority, authority_len);
    authority_buf[authority_len] = '\0';

    strcpy(url->port, "80");
    if (authority_buf[0] == '[') {
        char *closing = strchr(authority_buf, ']');

        if (closing == NULL) {
            return -1;
        }
        *closing = '\0';
        if (snprintf(url->host, sizeof(url->host), "%s", authority_buf + 1) >=
            (int)sizeof(url->host)) {
            return -1;
        }
        if (closing[1] == ':') {
            if (snprintf(url->port, sizeof(url->port), "%s", closing + 2) >=
                (int)sizeof(url->port)) {
                return -1;
            }
        } else if (closing[1] != '\0') {
            return -1;
        }
    } else {
        colon = strrchr(authority_buf, ':');
        if (colon != NULL) {
            *colon = '\0';
            if (snprintf(url->port, sizeof(url->port), "%s", colon + 1) >=
                (int)sizeof(url->port)) {
                return -1;
            }
        }
        if (authority_buf[0] == '\0' ||
            snprintf(url->host, sizeof(url->host), "%s", authority_buf) >=
                (int)sizeof(url->host)) {
            return -1;
        }
    }

    errno = 0;
    port_number = strtol(url->port, &endptr, 10);
    if (errno != 0 || *url->port == '\0' || *endptr != '\0' ||
        port_number < 1 || port_number > 65535) {
        return -1;
    }

    if (path_start == NULL || *path_start == '#') {
        strcpy(url->path, "/");
    } else if (*path_start == '?') {
        if (snprintf(url->path, sizeof(url->path), "/%s", path_start) >=
            (int)sizeof(url->path)) {
            return -1;
        }
    } else {
        if (snprintf(url->path, sizeof(url->path), "%s", path_start) >=
            (int)sizeof(url->path)) {
            return -1;
        }
    }
    colon = strchr(url->path, '#');
    if (colon != NULL) {
        *colon = '\0';
    }

    host_len = strlen(url->host);
    port_len = strlen(url->port);
    if (strchr(url->host, ':') != NULL) {
        if (strcmp(url->port, "80") == 0) {
            if (host_len + 2 >= sizeof(url->host_header)) {
                return -1;
            }
            url->host_header[0] = '[';
            memcpy(url->host_header + 1, url->host, host_len);
            url->host_header[host_len + 1] = ']';
            url->host_header[host_len + 2] = '\0';
        } else {
            if (host_len + port_len + 3 >= sizeof(url->host_header)) {
                return -1;
            }
            url->host_header[0] = '[';
            memcpy(url->host_header + 1, url->host, host_len);
            url->host_header[host_len + 1] = ']';
            url->host_header[host_len + 2] = ':';
            memcpy(url->host_header + host_len + 3, url->port, port_len + 1);
        }
    } else if (strcmp(url->port, "80") == 0) {
        if (host_len >= sizeof(url->host_header)) {
            return -1;
        }
        memcpy(url->host_header, url->host, host_len + 1);
    } else {
        if (host_len + port_len + 1 >= sizeof(url->host_header)) {
            return -1;
        }
        memcpy(url->host_header, url->host, host_len);
        url->host_header[host_len] = ':';
        memcpy(url->host_header + host_len + 1, url->port, port_len + 1);
    }
    return 0;
}

static int build_request_headers(rio_t *client_rio, const url_t *url,
                                 char *headers, size_t capacity)
{
    char line[MAXLINE];
    ssize_t n;

    headers[0] = '\0';
    if (snprintf(headers, capacity, "GET %s HTTP/1.0\r\n", url->path) >=
        (int)capacity ||
        append_text(headers, capacity, "Host: ") < 0 ||
        append_text(headers, capacity, url->host_header) < 0 ||
        append_text(headers, capacity, "\r\n") < 0 ||
        append_text(headers, capacity, user_agent_hdr) < 0 ||
        append_text(headers, capacity, "Connection: close\r\n") < 0 ||
        append_text(headers, capacity, "Proxy-Connection: close\r\n") < 0) {
        return -1;
    }

    while ((n = rio_readlineb(client_rio, line, sizeof(line))) > 0) {
        if (!strcmp(line, "\r\n") || !strcmp(line, "\n")) {
            return append_text(headers, capacity, "\r\n");
        }
        if (!strncasecmp(line, "Host:", 5) ||
            !strncasecmp(line, "User-Agent:", 11) ||
            !strncasecmp(line, "Connection:", 11) ||
            !strncasecmp(line, "Proxy-Connection:", 17)) {
            continue;
        }
        if (append_text(headers, capacity, line) < 0) {
            return -1;
        }
    }
    return -1;
}

static int append_text(char *dst, size_t capacity, const char *src)
{
    size_t used = strlen(dst);
    size_t needed = strlen(src);

    if (needed >= capacity - used) {
        return -1;
    }
    memcpy(dst + used, src, needed + 1);
    return 0;
}

static void send_error(int fd, const char *status, const char *shortmsg,
                       const char *longmsg)
{
    char body[MAXBUF];
    char headers[MAXBUF];
    int body_len;
    int header_len;

    body_len = snprintf(body, sizeof(body),
                        "<html><head><title>Proxy Error</title></head>"
                        "<body><h1>%s %s</h1><p>%s</p></body></html>\n",
                        status, shortmsg, longmsg);
    if (body_len < 0 || body_len >= (int)sizeof(body)) {
        return;
    }
    header_len = snprintf(headers, sizeof(headers),
                          "HTTP/1.0 %s %s\r\n"
                          "Content-Type: text/html\r\n"
                          "Content-Length: %d\r\n"
                          "Connection: close\r\n\r\n",
                          status, shortmsg, body_len);
    if (header_len < 0 || header_len >= (int)sizeof(headers)) {
        return;
    }
    rio_writen(fd, headers, (size_t)header_len);
    rio_writen(fd, body, (size_t)body_len);
}

static int cache_get(const char *key, unsigned char **data, size_t *size)
{
    cache_entry_t *entry;
    int found = 0;

    pthread_mutex_lock(&cache_mutex);
    for (entry = cache_head; entry != NULL; entry = entry->next) {
        if (strcmp(entry->key, key) == 0) {
            unsigned char *copy = malloc(entry->size == 0 ? 1 : entry->size);

            if (copy != NULL) {
                memcpy(copy, entry->data, entry->size);
                *data = copy;
                *size = entry->size;
                cache_remove(entry);
                cache_insert_front(entry);
                found = 1;
            }
            break;
        }
    }
    pthread_mutex_unlock(&cache_mutex);
    return found;
}

static void cache_put(const char *key, const unsigned char *data, size_t size)
{
    cache_entry_t *entry;
    cache_entry_t *current;

    if (size > MAX_OBJECT_SIZE || size > MAX_CACHE_SIZE) {
        return;
    }
    entry = malloc(sizeof(*entry));
    if (entry == NULL) {
        return;
    }
    entry->data = malloc(size == 0 ? 1 : size);
    if (entry->data == NULL) {
        free(entry);
        return;
    }
    memcpy(entry->data, data, size);
    snprintf(entry->key, sizeof(entry->key), "%s", key);
    entry->size = size;
    entry->prev = NULL;
    entry->next = NULL;

    pthread_mutex_lock(&cache_mutex);
    for (current = cache_head; current != NULL; current = current->next) {
        if (strcmp(current->key, key) == 0) {
            cache_remove(current);
            cache_size -= current->size;
            free(current->data);
            free(current);
            break;
        }
    }
    while (cache_tail != NULL && cache_size + size > MAX_CACHE_SIZE) {
        current = cache_tail;
        cache_remove(current);
        cache_size -= current->size;
        free(current->data);
        free(current);
    }
    cache_insert_front(entry);
    cache_size += size;
    pthread_mutex_unlock(&cache_mutex);
}

static void cache_remove(cache_entry_t *entry)
{
    if (entry->prev != NULL) {
        entry->prev->next = entry->next;
    } else {
        cache_head = entry->next;
    }
    if (entry->next != NULL) {
        entry->next->prev = entry->prev;
    } else {
        cache_tail = entry->prev;
    }
    entry->prev = NULL;
    entry->next = NULL;
}

static void cache_insert_front(cache_entry_t *entry)
{
    entry->prev = NULL;
    entry->next = cache_head;
    if (cache_head != NULL) {
        cache_head->prev = entry;
    } else {
        cache_tail = entry;
    }
    cache_head = entry;
}
