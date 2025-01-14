#include "types.h"
#include "user.h"
#include "socket.h"

int
main (int argc, char *argv[])
{
    int soc, peerlen, ret;
    struct sockaddr_in self, peer;
    unsigned char *addr;
    char buf[2048];

    printf(1, "Starting UDP Echo Server\n");
    soc = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (soc == 1) {
        printf(1, "socket: failure\n");
        exit();
    }
    printf(1, "socket: success, soc=%d\n", soc);
    self.sin_family = AF_INET;
    self.sin_addr.s_addr = INADDR_ANY;
    self.sin_port = htons(7);
    if (bind(soc, (struct sockaddr *)&self, sizeof(self)) == -1) {
        printf(1, "bind: failure\n");
        close(soc);
        exit();
    }
    addr = (unsigned char *)&self.sin_addr.s_addr;
    printf(1, "bind: success, self=%d.%d.%d.%d:%d\n",
        addr[0], addr[1], addr[2], addr[3], ntohs(self.sin_port));
    printf(1, "waiting for message...\n");
    while (1) {
        peerlen = sizeof(peer);
        ret = recvfrom(soc, buf, sizeof(buf), (struct sockaddr *)&peer, &peerlen);
        if (ret <= 0) {
            printf(1, "EOF\n");
            break;
        }
        if (ret == 2 && buf[0] == '.' && buf[1] == '\n') {
            printf(1, "quit\n");
            break;  
        }
        addr = (unsigned char *)&peer.sin_addr.s_addr;
        printf(1, "recvfrom: %d bytes data received, peer=%d.%d.%d.%d:%d\n",
            ret, addr[0], addr[1], addr[2], addr[3], ntohs(peer.sin_port));
        sendto(soc, buf, ret, (struct sockaddr *)&peer, peerlen);
    }
    close(soc);  
    exit();
}
