/** 
 @file  vita.c
 @brief ENet PlayStation Vita system specific functions
*/
#if defined(__vita__)
#include <psp2/net/net.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <netinet/in.h>

int sceClibPrintf(const char *fmt, ...);


#define ENET_BUILDING_LIB 1
#include "enet/enet.h"

#define TCP_NODELAY SCE_NET_TCP_NODELAY
#define INADDR_ANY SCE_NET_INADDR_ANY

#define SOMAXCONN 128
#define MSG_NOSIGNAL 0
#define EEINPROGRESS (SCE_NET_ERROR_EINPROGRESS & 0xFF)
#define EEWOULDBLOCK (SCE_NET_ERROR_EWOULDBLOCK & 0xFF)

static enet_uint32 timeBase = 0;

int
enet_initialize (void)
{
    return 0;
}

void
enet_deinitialize (void)
{
}

enet_uint32
enet_host_random_seed (void)
{
    struct timeval timeVal;
    
    gettimeofday (& timeVal, NULL);
    
    return (timeVal.tv_sec * 1000) ^ (timeVal.tv_usec / 1000);
}

enet_uint32
enet_time_get (void)
{
    struct timeval timeVal;

    gettimeofday (& timeVal, NULL);

    return timeVal.tv_sec * 1000 + timeVal.tv_usec / 1000 - timeBase;
}

void
enet_time_set (enet_uint32 newTimeBase)
{
    struct timeval timeVal;

    gettimeofday (& timeVal, NULL);
    
    timeBase = timeVal.tv_sec * 1000 + timeVal.tv_usec / 1000 - newTimeBase;
}

int
enet_address_equal (ENetAddress * address1, ENetAddress * address2)
{
    if (address1 -> address.ss_family != address2 -> address.ss_family)
      return 0;

    switch (address1 -> address.ss_family)
    {
    case AF_INET:
    case AF_INET6:
    {
        struct sockaddr_in *sin1, *sin2;
        sin1 = (struct sockaddr_in *) & address1 -> address;
        sin2 = (struct sockaddr_in *) & address2 -> address;
        return sin1 -> sin_port == sin2 -> sin_port &&
            sin1 -> sin_addr.s_addr == sin2 -> sin_addr.s_addr;
    }
    default:
    {
        return 0;
    }
    }
}

int
enet_address_set_port (ENetAddress * address, enet_uint16 port)
{
    if (address -> address.ss_family == AF_INET ||
        address -> address.ss_family == AF_INET6)
    {
        struct sockaddr_in *sin = (struct sockaddr_in *) &address -> address;
        sin -> sin_port = ENET_HOST_TO_NET_16 (port);
        return 0;
    }
    else
    {
        return -1;
    }
}

int
enet_address_set_address (ENetAddress * address, struct sockaddr * addr, socklen_t addrlen)
{
    if (addrlen > sizeof(struct sockaddr_storage))
      return -1;

    memcpy (&address->address, addr, addrlen);
    address->addressLength = addrlen;
    return 0;
}

int
enet_address_set_host (ENetAddress * address, const char * name)
{
    struct hostent * hostEntry = NULL;
    hostEntry = gethostbyname (name);

    struct sockaddr_in sin;
    memcpy (&sin, &address->address, sizeof(sockaddr_in));

    if (hostEntry != NULL && hostEntry -> h_addrtype == AF_INET)
    {
        sin.sin_addr.s_addr = * (enet_uint32 *) hostEntry -> h_addr_list [0];
        return enet_address_set_address(address, (struct sockaddr *)& sin, sizeof (struct sockaddr));
    }
    if (sceNetInetPton (AF_INET, name, & sin.sin_addr.s_addr) < 0) {
        return -1;
    }
    return enet_address_set_address(address, (struct sockaddr *)& sin, sizeof (struct sockaddr));
}

int
enet_socket_bind (ENetSocket socket, const ENetAddress * address)
{
    return bind (socket,
        (struct sockaddr *) & address -> address,
        address -> addressLength);
}

int
enet_socket_get_address (ENetSocket socket, ENetAddress * address)
{
    address -> addressLength = sizeof (address -> address);

    if (getsockname (socket, (struct sockaddr *) & address -> address, & address -> addressLength) == -1)
      return -1;

    return 0;
}

int 
enet_socket_listen (ENetSocket socket, int backlog)
{
    return listen (socket, backlog < 0 ? SOMAXCONN : backlog);
}

ENetSocket
enet_socket_create (int af, ENetSocketType type)
{
    return socket (PF_INET, type == ENET_SOCKET_TYPE_DATAGRAM ? SOCK_DGRAM : SOCK_STREAM, 0);
}

int
enet_socket_set_option (ENetSocket socket, ENetSocketOption option, int value)
{
    int result = -1;
    int _true = 1;
    switch (option)
    {
        case ENET_SOCKOPT_NONBLOCK:
            result = setsockopt (socket, SOL_SOCKET, SCE_NET_SO_NBIO, (char * ) & _true, sizeof(int));
            break;

        case ENET_SOCKOPT_REUSEADDR:
            result = setsockopt (socket, SOL_SOCKET, SO_REUSEADDR, (char *) & value, sizeof (int));
            break;

        case ENET_SOCKOPT_RCVBUF:
            result = setsockopt (socket, SOL_SOCKET, SO_RCVBUF, (char *) & value, sizeof (int));
            break;

        case ENET_SOCKOPT_SNDBUF:
            result = setsockopt (socket, SOL_SOCKET, SO_SNDBUF, (char *) & value, sizeof (int));
            break;

        case ENET_SOCKOPT_RCVTIMEO:
        {
            struct timeval timeVal;
            timeVal.tv_sec = value / 1000;
            timeVal.tv_usec = (value % 1000) * 1000;
            result = setsockopt (socket, SOL_SOCKET, SO_RCVTIMEO, (char *) & timeVal, sizeof (struct timeval));
            break;
        }

        case ENET_SOCKOPT_SNDTIMEO:
        {
            struct timeval timeVal;
            timeVal.tv_sec = value / 1000;
            timeVal.tv_usec = (value % 1000) * 1000;
            result = setsockopt (socket, SOL_SOCKET, SO_SNDTIMEO, (char *) & timeVal, sizeof (struct timeval));
            break;
        }

        case ENET_SOCKOPT_NODELAY:
            result = setsockopt (socket, IPPROTO_TCP, TCP_NODELAY, (char *) & value, sizeof (int));
            break;

        default:
            break;
    }
    return result == -1 ? -1 : 0;
}

int
enet_socket_get_option (ENetSocket socket, ENetSocketOption option, int * value)
{
    int result = -1;
    socklen_t len;
    switch (option)
    {
        case ENET_SOCKOPT_ERROR:
            len = sizeof (int);
            result = getsockopt (socket, SOL_SOCKET, SO_ERROR, value, & len);
            break;

        default:
            break;
    }
    return result == -1 ? -1 : 0;
}

int
enet_socket_connect (ENetSocket socket, const ENetAddress * address)
{
    int result;

    result = connect (socket, (struct sockaddr *) & address -> address, address -> addressLength);
    if (result < 0) {
        if (errno == EEINPROGRESS)
            return 0;
        result = -1;
    }

    return result;
}

ENetSocket
enet_socket_accept (ENetSocket socket, ENetAddress * address)
{
    int result;

    if (address != NULL)
      address -> addressLength = sizeof (address -> address);

    result = accept (socket, 
                     address != NULL ? (struct sockaddr *) & address -> address : NULL, 
                     address != NULL ? & address -> addressLength : NULL);
    
    if (result == -1)
      return ENET_SOCKET_NULL;

    return result;
}

int
enet_socket_shutdown (ENetSocket socket, ENetSocketShutdown how)
{
    return shutdown (socket, (int) how);
}

void
enet_socket_destroy (ENetSocket socket)
{
    if (socket != -1)
      close (socket);
}

int
enet_socket_send (ENetSocket socket,
                  const ENetAddress * address,
                  const ENetBuffer * buffers,
                  size_t bufferCount)
{
    int sentLength;

    struct msghdr msgHdr;

    memset (& msgHdr, 0, sizeof (struct msghdr));

    if (address != NULL)
    {
        msgHdr.msg_name = (void*) & address -> address;
        msgHdr.msg_namelen = address -> addressLength;
    }

    msgHdr.msg_iov = (struct iovec *) buffers;
    msgHdr.msg_iovlen = bufferCount;

    sentLength = sendmsg (socket, & msgHdr, MSG_NOSIGNAL);

    if (sentLength < 0) {
        if (errno == EEWOULDBLOCK)
            return 0;

        sceClibPrintf("enet_socket_send failed! socket 0x%x error 0x%x\n", socket, sentLength);
        sceClibPrintf("tried to send buffers 0x%x bufferCount %d\n", buffers, bufferCount);

        return -1;
    }

    return sentLength;
}

int
enet_socket_receive (ENetSocket socket,
                     ENetAddress * address,
                     ENetBuffer * buffers,
                     size_t bufferCount)
{
    int recvLength;

    struct msghdr msgHdr;

    memset (& msgHdr, 0, sizeof (struct msghdr));

    if (address != NULL)
    {
        msgHdr.msg_name = & address -> address;
        msgHdr.msg_namelen = sizeof (address -> address);
    }

    msgHdr.msg_iov = (struct iovec *) buffers;
    msgHdr.msg_iovlen = bufferCount;

    recvLength = recvmsg (socket, & msgHdr, MSG_NOSIGNAL);

    if (recvLength < 0) {
        if (errno == EEWOULDBLOCK)
            return 0;

        sceClibPrintf("enet_socket_receive failed! socket 0x%x recvLength 0x%x\n", socket, recvLength);

        return -1;
    }
    
    if (address != NULL)
      address -> addressLength = msgHdr.msg_namelen;

#ifdef HAS_MSGHDR_FLAGS
    if (msgHdr.msg_flags & MSG_TRUNC)
      return -1;
#endif

    return recvLength;
}

int
enet_socketset_select (ENetSocket maxSocket, ENetSocketSet * readSet, ENetSocketSet * writeSet, enet_uint32 timeout)
{
    struct timeval timeVal;

    timeVal.tv_sec = timeout / 1000;
    timeVal.tv_usec = (timeout % 1000) * 1000;

    return select (maxSocket + 1, readSet, writeSet, NULL, & timeVal);
}

int
enet_socket_wait (ENetSocket socket, enet_uint32 * condition, enet_uint32 timeout)
{
    fd_set readSet, writeSet;
    struct timeval timeVal;
    int selectCount;

    timeVal.tv_sec = timeout / 1000;
    timeVal.tv_usec = (timeout % 1000) * 1000;

    FD_ZERO (& readSet);
    FD_ZERO (& writeSet);

    if (* condition & ENET_SOCKET_WAIT_SEND)
      FD_SET (socket, & writeSet);

    if (* condition & ENET_SOCKET_WAIT_RECEIVE)
      FD_SET (socket, & readSet);

    selectCount = select (socket + 1, & readSet, & writeSet, NULL, & timeVal);

    if (selectCount < 0)
    {
        if (errno == EINTR && * condition & ENET_SOCKET_WAIT_INTERRUPT)
        {
            * condition = ENET_SOCKET_WAIT_INTERRUPT;

            return 0;
        }
      
        return -1;
    }

    * condition = ENET_SOCKET_WAIT_NONE;

    if (selectCount == 0)
      return 0;

    if (FD_ISSET (socket, & writeSet))
      * condition |= ENET_SOCKET_WAIT_SEND;

    if (FD_ISSET (socket, & readSet))
      * condition |= ENET_SOCKET_WAIT_RECEIVE;

    return 0;
}
#endif
