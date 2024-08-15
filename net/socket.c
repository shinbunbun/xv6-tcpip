#include "platform.h"

#include "fs.h"
#include "sleeplock.h"
#include "file.h"
#include "socket.h"

#include "tcp.h"
#include "udp.h"

struct socket {
    int type;
    int desc;
};

struct file*
socketalloc(int domain, int type, int protocol)
{
    struct file *f;
    struct socket *s;

    if (domain != AF_INET || type != SOCK_DGRAM || protocol != 0) {
        return NULL;
    }
    f = filealloc();
    if (!f) {
        return NULL;
    }
    s = (struct socket *)kalloc();
    if (!s) {
        fileclose(f);
        return NULL;
    }
    s->type = type;
    s->desc = udp_open();
    f->type = FD_SOCKET;
    f->readable = 1;
    f->writable = 1;
    f->socket = s;
    return f;
}

int
socketclose(struct socket *s)
{
    if (s->type == SOCK_DGRAM)
        return udp_close(s->desc);
    return -1;
}

int
socketbind(struct socket *s, struct sockaddr *addr, int addrlen)
{
    struct ip_endpoint local;

    local.addr = ((struct sockaddr_in *)addr)->sin_addr.s_addr;
    local.port = ((struct sockaddr_in *)addr)->sin_port;
    if (s->type == SOCK_DGRAM)
        return udp_bind(s->desc, &local);
    return -1;
}

int
socketrecvfrom(struct socket *s, char *buf, int n, struct sockaddr *addr, int *addrlen)
{
    struct ip_endpoint foreign;
    int ret;

    if (s->type != SOCK_DGRAM)
        return -1;
    ret = udp_recvfrom(s->desc, (uint8_t *)buf, n, &foreign);
    if (addr) {
        ((struct sockaddr_in *)addr)->sin_family = AF_INET;
        ((struct sockaddr_in *)addr)->sin_addr.s_addr = foreign.addr;
        ((struct sockaddr_in *)addr)->sin_port = foreign.port;
    }
    return ret;
}

int
socketsendto(struct socket *s, char *buf, int n, struct sockaddr *addr, int addrlen)
{
    struct ip_endpoint foreign;

    if (s->type != SOCK_DGRAM)
        return -1;
    foreign.addr = ((struct sockaddr_in *)addr)->sin_addr.s_addr;
    foreign.port = ((struct sockaddr_in *)addr)->sin_port;
    return udp_sendto(s->desc, (uint8_t *)buf, n, &foreign);
}
