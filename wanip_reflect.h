/*===============================================================
*   Copyright (C) 2013 All rights reserved.
*   
*   file     : wanip_reflect.h
*   author   : joshua.wu
*   date     : 2013-10-20
*   descripe : 
*
*   modify   : 
*
================================================================*/
#ifndef _WANIP_REFLECT_H
#define _WANIP_REFLECT_H

#include  <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>


namespace joshLib
{

/**
 * @brief   get wan ip of NAT(Router) device
 *
 * send an ICMP_ECHO packet with IPOPT_RR option, and ttl < 9,
 * make sure a some of Router send back an ICMP_TIME_EXCEEDED 
 * packet, which contatin our ICMP_ECHO packet, which's IP header
 * hold the record-route items. 
 *
 * This class only work for 1 level NAT topology,
 * for N(<9) level NAT topology, have to extract from m_RRarray[N]
 */
class Wanip
{
public:
    Wanip (int ttl = 3, const char *url = "1.1.1.1" /*, const char *if = "eth1" */);
    ~Wanip ();
public:
    uint32_t getWanip ();
    uint32_t *getRR (ssize_t &ss);

    int getLastError ();
    char *getLastErrorStr ();

private:
    int m_err;
    char m_errStr[512];

    int m_ttl;
    char m_pingUrl[256];
    char m_hostname[256];
    struct sockaddr_in m_whereto; ///< who to ping
    int m_RRs;
    uint32_t m_RRarray[9];
    uint32_t m_wanip;               ///< 1 level NAT topology
                                    ///< may be incorrect of multi level NAT topology
                                    //< m_RRarray[N] maybe, I haven't test
    char m_wanip_str[16];
private:
    int do_wanip_reflect ();

    int init_sock ();
    int recv_icmp (int icmp_sock, int pid);
    int send_icmp (int icmp_sock, int pid);
    void add_opt (unsigned char *cp, int totlen);
    uint16_t in_cksum(const uint16_t *addr, int len, uint16_t csum);

public:
    static char *pr_addr(uint32_t addr);
};





/**
 * @brief   test case of class Wanip
 *
 * test.cpp
 * g++ -g -o test test.cpp wanip_reflect.cpp
 * 
 * #include  <stdio.h>
 * #include  "wanip_reflect.h"
 * 
 * using namespace joshLib;
 * 
 * int main(int argc, char *argv[])
 * {
 *     return test_Wanip ();
 * }
 * 
 * 
 * joshua:UDP_hole_punching$ sudo ./test                               
 * 
 * RR:     192.168.200.2
 *         183.20.163.152
 *         183.57.105.50
 *         183.57.116.5
 * 
 * wan ip of my ADSL Router: 183.20.163.152
 *
 */
int test_Wanip ();

}
#endif // _WANIP_REFLECT_H
