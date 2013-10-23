/*===============================================================
*   Copyright (C) 2013 All rights reserved.
*   
*   file     : test.cpp
*   author   : joshua.wu
*   date     : 2013-10-20
*   descripe : 
*
*   modify   : 
*
================================================================*/
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#define NROUTES         9               /* number of record route slots */

#define SET_ERROR(e) {\
        int err = e;\
        printf ("%s, errno: %d, %s %d\n",\
                strerror (err), err, __FILE__, __LINE__); }

char *m_ip = NULL;
int m_ttl = 3;
uint16_t m_port = 53;  ///< most firewall won't block DNS request

void exit_usage ()
{
    printf ("*** no use, sorce address was dequerate by NAT device ***"
            "focking TP-Link");

    printf ("usage\n"
            "\t./udpreflect [ -t [ttl] -p [port] ] ip\n"
            "\tdefault: ttl = 3, port = 53\n");
    exit (0);
}

int main (int argc, char *argv[])
{
    int ch;
    while ((ch = getopt (argc, argv, "h?" "t:p:")) != EOF) {
        switch (ch) {
        case 't':
            m_ttl = atoi (optarg);
            break;
        case 'p':
            m_port = atoi (optarg);
            break;
        case 'h':
        case '?':
        default:
            exit_usage ();
        }
    }
    argc -= optind;
    argv += optind;
    if (argc == 1)
        m_ip = *argv;
    else
        exit_usage ();

    int udp_sock = socket (PF_INET, SOCK_DGRAM, 0);

    if (setsockopt (udp_sock, IPPROTO_IP, IP_TTL,
                        &m_ttl, sizeof (m_ttl)) == -1) {
        SET_ERROR (errno);
    }
    /* record route option */
    char rspace[3 + 4 * NROUTES + 1];   /* record route space */
    memset (rspace, 0, sizeof (rspace));
    rspace[0] = IPOPT_NOP;
    rspace[1+IPOPT_OPTVAL] = IPOPT_RR;
    rspace[1+IPOPT_OLEN] = sizeof (rspace)-1;
    rspace[1+IPOPT_OFFSET] = IPOPT_MINOFF;
    int optlen = 40;

    /*
    if (setsockopt (udp_sock, IPPROTO_IP, IP_OPTIONS,
                    rspace, sizeof (rspace)) < 0) {
        SET_ERROR (errno);
    }
    */

    struct sockaddr_in whereto = {0};
    whereto.sin_port = htons (m_port);
    whereto.sin_family = AF_INET;
    if (inet_pton (AF_INET, m_ip, &whereto.sin_addr) == 1) {

    } else {
        struct hostent *hp;
        hp = gethostbyname (m_ip);
        if (!hp) {
            SET_ERROR (errno);
            return -1;
        }
        memcpy (&whereto.sin_addr, hp->h_addr, 4);
    }

    uint16_t pid = htons (getpid () & 0xFFFF);
    char buf[128] = {0};
    memcpy (buf, &pid, sizeof (pid));
    int r = sendto (udp_sock, buf, sizeof (buf), 0,
                    (struct sockaddr *)&whereto, sizeof (whereto));
    if (r == -1)
        SET_ERROR (errno);

    return 0;
}
