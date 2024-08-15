#include "types.h"
#include "defs.h"
#include "param.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"

int
sys_socket(void)
{
  int fd, domain, type, protocol;
  struct file *f;

  if (argint(0, &domain) < 0 || argint(1, &type) < 0 || argint(2, &protocol) < 0)
    return -1;
  if ((f = socketalloc(domain, type, protocol)) == 0 || (fd = fdalloc(f)) < 0){
    if (f)
      fileclose(f);
    return -1;
  }
  return fd;
}

int
sys_bind(void)
{
  struct file *f;
  int addrlen;
  struct sockaddr *addr;

  if (argfd(0, 0, &f) < 0 || argint(2, &addrlen) < 0 || argptr(1, (void*)&addr, addrlen) < 0)
    return -1;
  if (f->type != FD_SOCKET)
    return -1;
  return socketbind(f->socket, addr, addrlen);
}

int
sys_recvfrom(void)
{
  struct file *f;
  int n;
  char *p;
  int *addrlen;
  struct sockaddr *addr = NULL;

  if (argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argptr(1, &p, n) < 0 || argptr(4, (void*)&addrlen, sizeof(*addrlen)) < 0)
    return -1;
  if (addrlen && argptr(3, (void*)&addr, *addrlen) < 0)
    return -1;
  if (f->type != FD_SOCKET)
    return -1;
  return socketrecvfrom(f->socket, p, n, addr, addrlen);
}

int
sys_sendto(void)
{
  struct file *f;
  int n;
  char *p;
  int addrlen;
  struct sockaddr *addr;

  if (argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argptr(1, &p, n) < 0 || argint(4, &addrlen) < 0 || argptr(3, (void*)&addr, addrlen) < 0)
    return -1;
  if (f->type != FD_SOCKET)
    return -1;
  return socketsendto(f->socket, p, n, addr, addrlen);
}
