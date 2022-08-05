/*
 *  clip_share_web.c - web server of the application
 *  Copyright (C) 2022 H. Thevindu J. Wijesekera
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "utils/net_utils.h"
#include "utils/utils.h"
#include "config.h"

#ifndef NO_WEB
#include <stdio.h>
#include <string.h>
#ifdef __linux__
#include <unistd.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#elif _WIN32
#include <io.h>
#include <windows.h>
#endif

#define FAIL -1

extern char blob_page[];
extern int blob_size_page;

static int say(char *, socket_t *);
static void receiver_web(socket_t *);

static int say(char *msg, socket_t *sock)
{
    return write_sock(sock, msg, strlen(msg));
}

static void receiver_web(socket_t *sock)
{
    char method[8];
    {
        int ind = 0;
        do
        {
            if (read_sock(sock, method + ind, 1) != EXIT_SUCCESS)
                return;
            ind++;
        } while (method[ind - 1] != ' ');
        method[ind - 1] = 0;
    }
#ifdef DEBUG_MODE
    puts(method);
#endif

    char path[2049];
    {
        int ind = 0;
        do
        {
            if (read_sock(sock, path + ind, 1) != EXIT_SUCCESS)
                return;
            ind++;
        } while (path[ind - 1] != ' ');
        path[ind - 1] = 0;
    }
#ifdef DEBUG_MODE
    puts(path);
#endif

    if (!strcmp(method, "GET"))
    {
        char buf[128];
        while (read_sock_no_wait(sock, buf, 128) > 0)
            ;
        if (!strcmp(path, "/"))
        {
            if (say("HTTP/1.0 200 OK\r\n", sock) != EXIT_SUCCESS)
                return;
            char tmp[96];                                                                                                              // = (char *)malloc(96);
            sprintf(tmp, "Content-Type: text/html; charset=utf-8\r\nContent-Length: %i\r\nConnection: close\r\n\r\n", blob_size_page); // Content-Disposition: attachment; filename="filename.ext" // put this, filename parameter is optional
            if (say(tmp, sock) != EXIT_SUCCESS)
                return;
            if (write_sock(sock, blob_page, blob_size_page) != EXIT_SUCCESS)
                return;
        }
        else if (!strcmp(path, "/clip"))
        {
            size_t len;
            char *buf;
            if (get_clipboard_text(&buf, &len) != EXIT_SUCCESS || len <= 0) // do not change the order
            {
                say("HTTP/1.0 500 Internal Server Error\r\n\r\n", sock);
                return;
            }
            if (say("HTTP/1.0 200 OK\r\n", sock) != EXIT_SUCCESS)
                return;
            if (say("Content-Type: text/plain; charset=utf-8\r\n", sock) != EXIT_SUCCESS)
                return;
            char tmp[64];
            sprintf(tmp, "Content-Length: %zu\r\nConnection: close\r\n\r\n", len);
            if (say(tmp, sock) != EXIT_SUCCESS)
                return;
            if (write_sock(sock, buf, len) != EXIT_SUCCESS)
                return;
            free(buf);
        }
        else if (!strcmp(path, "/img"))
        {
            size_t len = 0;
            char *buf;
            if (get_image(&buf, &len) == EXIT_FAILURE || len <= 0)
            {
                say("HTTP/1.0 404 Not Found\r\n\r\n", sock);
                return;
            }
            if (say("HTTP/1.0 200 OK\r\n", sock) != EXIT_SUCCESS)
                return;
            if (say("Content-Type: image/png\r\nContent-Disposition: attachment; filename=\"clip.png\"\r\n", sock) != EXIT_SUCCESS)
                return;
            char tmp[64];
            sprintf(tmp, "Content-Length: %zu\r\nConnection: close\r\n\r\n", len);
            if (say(tmp, sock) != EXIT_SUCCESS)
                return;
            if (write_sock(sock, buf, len) != EXIT_SUCCESS)
                return;
            free(buf);
        }
        else
        {
            say("HTTP/1.0 404 Not Found\r\n\r\n", sock);
            return;
        }
    }
    else if (!strcmp(method, "POST"))
    {
        unsigned int len = 2048;
        char *headers = (char *)malloc(len);
        *headers = 0;
        int r = 0, cnt = 0;
        char buf[256];
        {
            char *ptr = headers;
            char *check = ptr;
            while (1)
            {
                r = read_sock_no_wait(sock, buf, 256);
                if (r > 0)
                {
                    // buf[r] = '\0';
                    memcpy(ptr, buf, r);
                    ptr += r;
                    *ptr = 0;
                    if (ptr - headers >= len - 256)
                        break;
                    cnt = 0;
                }
                else
                {
                    if (cnt == 0)
                    {
                        if (strstr(check, "\r\n\r\n"))
                            break;
                        check = ptr - 3;
                        check = check > headers ? check : headers;
                    }
                    else if (cnt++ > 50)
                    {
                        break;
                    }
                }
            }
        }
        unsigned long data_len;
        {
            char *cont_len_header = strstr(headers, "Content-Length: ");
            if (cont_len_header == NULL)
            {
                free(headers);
                return;
            }
            data_len = strtoul(cont_len_header + 16, NULL, 10);
            if (data_len <= 0 || 1048576 < data_len)
            {
                free(headers);
                return;
            }
        }
        char *data = strstr(headers, "\r\n\r\n");
        if (data == NULL)
        {
            free(headers);
            return;
        }
        data += 4;
        *(data - 1) = '\0';
        unsigned int header_len = (unsigned int)(data - headers);
        headers = (char *)realloc(headers, header_len + data_len + 1);
        data = headers + header_len;
        {
            cnt = 0;
            char *ptr = data + strlen(data);
            while (ptr - data < (long)data_len)
            {
                if ((r = read_sock_no_wait(sock, buf, 256)) > 0)
                {
                    // buf[r] = '\0';
                    memcpy(ptr, buf, r);
                    ptr += r;
                    *ptr = 0;
                    cnt = 0;
                }
                else
                {
                    if (cnt++ > 50)
                    {
                        free(headers);
                        return;
                    }
                }
            }
        }

        if (say("HTTP/1.0 204 No Content\r\n\r\n", sock) != EXIT_SUCCESS)
            return;
        put_clipboard_text(data, data_len);
        free(headers);
        return;
    }
}

#ifdef _WIN32
static DWORD WINAPI webServerThreadFn(void *arg)
{
    socket_t socket;
    memcpy(&socket, arg, sizeof(socket_t));
    free(arg);
    receiver_web(&socket);
    close_socket(&socket);
    return 0;
}
#endif

int web_server(const int port, config cfg)
{
    if (cfg.allowed_clients == NULL || cfg.allowed_clients->len <= 0 || cfg.web_port <= 0)
    {
#ifdef DEBUG_MODE
        puts("Invalid config for secure mode");
#endif
        return EXIT_FAILURE;
    }
#ifdef __linux__
    signal(SIGCHLD, SIG_IGN);
#endif
    listener_t listener = open_listener_socket(1, cfg.priv_key, cfg.server_cert, cfg.ca_cert);
    bind_port(listener, port);
    if (listen(listener.socket, 10) == -1)
    {
        error("Can\'t listen");
        return EXIT_FAILURE;
    }
    while (1)
    {
        socket_t connect_sock = get_connection(listener, cfg.allowed_clients);
        if (connect_sock.type == NULL_SOCK)
        {
            close_socket(&connect_sock);
            continue;
        }
#ifdef __linux__
#ifndef DEBUG_MODE
        pid_t p1 = fork();
        if (p1)
        {
            close_socket(&connect_sock);
        }
        else
        {
            close(listener.socket);
#endif
            receiver_web(&connect_sock);
            close_socket(&connect_sock);
#ifndef DEBUG_MODE
            break;
        }
#endif
#elif _WIN32
        socket_t *connect_ptr = malloc(sizeof(socket_t));
        memcpy(connect_ptr, &connect_sock, sizeof(socket_t));
        HANDLE serveThread = CreateThread(NULL, 0, webServerThreadFn, (LPDWORD)connect_ptr, 0, NULL);
#ifdef DEBUG_MODE
        if (serveThread == NULL)
        {
            error("Thread creation failed");
            return EXIT_FAILURE;
        }
#else
        (void)serveThread;
#endif
#endif
    }
    return EXIT_SUCCESS;
}
#endif
