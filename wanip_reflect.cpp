/*===============================================================
*   Copyright (C) 2013 All rights reserved.
*   
*   file     : wanip_reflect.cpp
*   author   : joshua.wu
*   date     : 2013-10-20
*   descripe : make intermedia Router telling NAT device's wan ip BY setting IPOPT_RR and ttl to 3,
*              make sure some of intermedia Router send back a ICMP_TIME_EXCEEDED message
*
*   modify   : 
*
================================================================*/
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>

#include  <stdio.h>
#include  <string.h>
#include <stdlib.h>
#include <errno.h>

#include    "wanip_reflect.h"

#ifndef ICMP_FILTER
#define ICMP_FILTER	1
struct icmp_filter {
	uint32_t	data;
};
#endif

#define DEFDATALEN      (64 - 8)        /* default data length */
#define NROUTES         9               /* number of record route slots */

#define SET_ERROR(e) {\
        int err = e;\
        snprintf (m_errStr, 512, "%s, errno: %d, %s %d\n",\
                strerror (err), err, __FILE__, __LINE__);\
        m_err = err; }

namespace joshLib
{


Wanip::Wanip (int ttl, const char *url)
    : m_ttl(ttl), m_RRs(0), m_err(0),
      m_wanip(0)
{
    strncpy (m_pingUrl, url, 256);
    m_pingUrl[255] = '\0';
    memset (m_RRarray, 0, sizeof (m_RRarray));
}

Wanip::~Wanip ()
{
}

uint32_t Wanip::getWanip ()
{
    if (m_wanip == 0)
        do_wanip_reflect ();

    return m_wanip;
}

/**
 * @brief   return IPOPT_RR array
 *
 * @param[out]   ss     count of IPOPT_RR array
 *
 * @return  pointer to IPOPT_RR array
 */
uint32_t *Wanip::getRR (ssize_t &ss)
{
    if (m_RRs == 0)
        do_wanip_reflect ();
    ss = m_RRs;

    return m_RRarray;
}

int Wanip::getLastError ()
{
    return m_err;
}

char *Wanip::getLastErrorStr ()
{
    return m_errStr;
}

/**
 * @brief   core procedure
 *
 * @return  
 *
 * @callergraph
 */
int Wanip::do_wanip_reflect ()
{
    memset((char *)&m_whereto, 0, sizeof(m_whereto));
    m_whereto.sin_family = AF_INET;

    if (inet_aton(m_pingUrl, &m_whereto.sin_addr) == 1) {
        strncpy(m_hostname, m_pingUrl, sizeof(m_pingUrl));
    } else {
        struct hostent *hp;
        hp = gethostbyname(m_pingUrl);
        if (!hp) {
            SET_ERROR (errno);
            return -1;
        }
        memcpy(&m_whereto.sin_addr, hp->h_addr, 4);
        strncpy(m_hostname, hp->h_name, sizeof(m_hostname) - 1);
        m_hostname[sizeof(m_hostname) - 1] = 0;
    }
    int ident = htons(getpid() & 0xFFFF);

    int icmp_sock = init_sock ();

    // 3s timeout
    struct timeval timeout;
    timeout.tv_sec = 3;
    timeout.tv_usec = 0;
    struct timeval start;
    gettimeofday (&start, NULL);

    while (1) {
        int ret;
        struct timeval tv;
        fd_set fdset;
        FD_ZERO (&fdset);
        FD_SET (icmp_sock, &fdset);
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        send_icmp (icmp_sock, ident);

        ret = select (icmp_sock + 1, &fdset, NULL, NULL, &tv);

        if (ret == -1) {
            SET_ERROR (errno);
        } else if (ret) {
            if (0 == recv_icmp (icmp_sock, ident))
                return 0;
        } else {
            // select time out
            struct timeval res;
            struct timeval now;
            gettimeofday (&now, NULL);
            timersub (&now, &start, &res);
            if (timercmp (&res, &timeout, >))
                return 0;
        }
    }

    return 0;
}

/**
 * @brief   initialize icmp socket fd
 *
 * @return  icmp socket fd, with ICMP_FILTER, IP_OPTIONS
 */
int Wanip::init_sock ()
{
    int icmp_sock = socket(PF_INET, SOCK_RAW, IPPROTO_ICMP);

    if (setsockopt(icmp_sock, IPPROTO_IP, IP_TTL,
                        &m_ttl, sizeof(m_ttl)) == -1) {
        SET_ERROR (errno);
    }
    struct icmp_filter filt;
    filt.data = ~((1<<ICMP_DEST_UNREACH) |
                  (1<<ICMP_TIME_EXCEEDED) |
                  (1<<ICMP_ECHOREPLY));
    if (setsockopt(icmp_sock, SOL_RAW, ICMP_FILTER, (char*)&filt, sizeof(filt)) == -1) {
    }
    /* record route option */
    char rspace[3 + 4 * NROUTES + 1];   /* record route space */
    memset(rspace, 0, sizeof(rspace));
    rspace[0] = IPOPT_NOP;
    rspace[1+IPOPT_OPTVAL] = IPOPT_RR;
    rspace[1+IPOPT_OLEN] = sizeof(rspace)-1;
    rspace[1+IPOPT_OFFSET] = IPOPT_MINOFF;
    int optlen = 40;

    if (setsockopt(icmp_sock, IPPROTO_IP, IP_OPTIONS,
                    rspace, sizeof(rspace)) < 0) {
        SET_ERROR (errno);
    }

    return icmp_sock;
}

uint16_t Wanip::in_cksum(const uint16_t *addr, int len, uint16_t csum)
{
    int nleft = len;
    const uint16_t *w = addr;
    uint16_t answer;
    int sum = csum;

    /*
     *  Our algorithm is simple, using a 32 bit accumulator (sum),
     *  we add sequential 16 bit words to it, and at the end, fold
     *  back all the carry bits from the top 16 bits into the lower
     *  16 bits.
     */
    while (nleft > 1)  {
            sum += *w++;
            nleft -= 2;
    }

    /* mop up an odd byte, if necessary */
    if (nleft == 1)
            sum += htons(*(u_char *)w << 8);

    /*
     * add back carry outs from top 16 bits to low 16 bits
     */
    sum = (sum >> 16) + (sum & 0xffff); /* add hi 16 to low 16 */
    sum += (sum >> 16);                 /* add carry */
    answer = ~sum;                              /* truncate to 16 bits */
    return (answer);
}

/**
 * Return an ascii host address as a dotted quad and optionally with
 * a hostname.
 */
char *Wanip::pr_addr(uint32_t addr)
{
    static char buf[4096];
    struct sockaddr_in sin = {0};
    sin.sin_addr.s_addr = addr;

    inet_ntop(AF_INET, &addr, buf, sizeof(struct sockaddr_in));
    return(buf);
}

void Wanip::add_opt (unsigned char *cp, int totlen)
{
    m_RRs = 0;
    int i, j;
    int optlen;
    unsigned char * optptr;
    optptr = cp;

    while (totlen > 0) {
        if (*optptr == IPOPT_EOL)
            break;
        if (*optptr == IPOPT_NOP) {
            totlen--;
            optptr++;
            //printf("\nNOP");
            continue;
        }
        cp = optptr;
        optlen = optptr[1];
        if (optlen < 2 || optlen > totlen)
            break;

        switch (*cp) {
        case IPOPT_RR:
            j = *++cp;          /* get length */
            i = *++cp;          /* and pointer */
            if (i > j)
                i = j;
            i -= IPOPT_MINOFF;
            if (i <= 0)
                break;
            //printf("\nRR: ");
            cp++;
            for (;;) {
                memcpy(&m_RRarray[m_RRs ++], cp, 4);
                cp += 4;
                i -= 4;
                if (i <= 0)
                    break;
            }
            break;
        default:
            //printf("\nunknown option %x", *cp);
            break;
        }
        totlen -= optlen;
        optptr += optlen;
    }

    m_wanip = m_RRarray[1];
}

int Wanip::send_icmp (int icmp_sock, int pid)
{
    int datalen = DEFDATALEN;
    unsigned char buf[DEFDATALEN + 8];
    struct icmphdr *icp;
    int cc;

    icp = (struct icmphdr *)buf;
    icp->type = ICMP_ECHO;
    icp->code = 0;
    icp->checksum = 0;
    icp->un.echo.sequence = htons(2013);
    icp->un.echo.id = pid;                      /* ID */

    cc = datalen + 8;                   /* skips ICMP portion */

    /* compute ICMP checksum here */
    icp->checksum = in_cksum((uint16_t *)icp, cc, 0);

    int r = sendto (icmp_sock, buf, cc, 0,
                    (struct sockaddr *)&m_whereto, sizeof (m_whereto));
    if (r == -1)
        SET_ERROR (errno);

    return (cc == r ? 0 : r);
}

/**
 * @brief   extact IP options
 *
 * @param   icmp_sock
 * @param   pid
 *
 * @retval  0 success, recv record-route
 *
 * ONLY expect ICMP_ECHOREPLY or ICMP_TIME_EXCEEDED
 *
 * @callgraph
 */
int Wanip::recv_icmp (int icmp_sock, int pid)
{
    unsigned char buf[1600];
    int r = recvfrom (icmp_sock, buf, 1600, 0, NULL, NULL);

    /* Check the IP header */
    struct iphdr *ip = (struct iphdr *)buf;
    int hlen = ip->ihl * 4;
    if (r < hlen + 8 || ip->ihl < 5)
        SET_ERROR (errno);

    /* Now the ICMP part */
    r -= hlen;
    struct icmphdr *icp = (struct icmphdr *)(buf + hlen);
    int csfailed = in_cksum((uint16_t *)icp, r, 0);

    if (icp->type == ICMP_ECHOREPLY) {
        if (icp->un.echo.id != pid)
            return -1;                   /* 'Twas not our ECHO */

        unsigned char *cp = buf + sizeof(struct iphdr);                     ///< cp -> ip options
        int totlen = hlen - sizeof (struct iphdr);
        add_opt (cp, totlen);
        return 0;

    } else if (icp->type == ICMP_TIME_EXCEEDED) {
        struct iphdr *ip2 = (struct iphdr *)(icp + 1);
        unsigned char *cp2 = (unsigned char *)ip2 + sizeof(struct iphdr);   ///< cp2 -> ip options
        int hlen2 = ip2->ihl * 4;
        int totlen2 = hlen2 - sizeof (struct iphdr);
        struct icmphdr *icp2 = (struct icmphdr *)((char *)ip2 + hlen2);

        if (icp2->type == ICMP_ECHO) {
            if (icp2->un.echo.id != pid)
                return -1;                       /* 'Twas not our ECHO */
            if (ip2->ihl > 5) {
                add_opt (cp2, totlen2);
                return 0;
            }
        } else {
            //printf ("icp2->type == %d\n", icp2->type);
        }
    } else {
        //printf ("icp->type == %d\n", icp->type);
    }

    return 0;
}

} // namespace joshLib

#undef SET_ERROR
