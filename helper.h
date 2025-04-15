#ifndef HELPER_H
#define HELPER_H 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <cassert>
#include <cerrno>

const size_t k_max_msg = 4096;

void die(const char* s);
void msg(const char *s);
int32_t read_full(int fd, char *rbuf, size_t n);
int32_t write_all(int fd, const char *wbuf, size_t n);

#endif