/**
 * @file ipv6.h
 * @brief IPv6 (Internet Protocol Version 6)
 *
 * @section License
 *
 * Copyright (C) 2010-2013 Oryx Embedded. All rights reserved.
 *
 * This file is part of CycloneTCP Open.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * @author Oryx Embedded (www.oryx-embedded.com)
 * @version 1.3.5
 **/

#ifndef _IPV6_H
#define _IPV6_H

//Dependencies
#include <string.h>
#include "tcp_ip_stack.h"
#include "ethernet.h"

//IPv6 support
#ifndef IPV6_SUPPORT
   #define IPV6_SUPPORT DISABLED
#elif (IPV6_SUPPORT != ENABLED && IPV6_SUPPORT != DISABLED)
   #error IPV6_SUPPORT parameter is invalid
#endif

//Default IPv6 Hop Limit field
#ifndef IPV6_DEFAULT_HOP_LIMIT
   #define IPV6_DEFAULT_HOP_LIMIT 64
#elif (IPV6_DEFAULT_HOP_LIMIT < 1)
   #error IPV6_DEFAULT_HOP_LIMIT parameter is invalid
#endif

//Maximum number of DNS servers
#ifndef IPV6_MAX_DNS_SERVERS
   #define IPV6_MAX_DNS_SERVERS 2
#elif (IPV6_MAX_DNS_SERVERS < 1)
   #error IPV6_MAX_DNS_SERVERS parameter is invalid
#endif

//Maximum size of the IPv6 filter table
#ifndef IPV6_FILTER_MAX_SIZE
   #define IPV6_FILTER_MAX_SIZE 8
#elif (IPV6_FILTER_MAX_SIZE < 4)
   #error IPV6_FILTER_MAX_SIZE parameter is invalid
#endif

//Version number for IPv6
#define IPV6_VERSION 6
//Minimum MTU that routers and physical links are required to handle
#define IPV6_DEFAULT_MTU 1280
//Maximum payload size
#define IPV6_MAX_PAYLOAD_SIZE (ETH_MTU - sizeof(Ipv6Header))

//Macro used for defining IPv6 addresses
#define IPV6_ADDR(a, b, c, d, e, f, g, h) {{{ \
   MSB(a), LSB(a), MSB(b), LSB(b), MSB(c), LSB(c), MSB(d), LSB(d), \
   MSB(e), LSB(e), MSB(f), LSB(f), MSB(g), LSB(g), MSB(h), LSB(h)}}}

//Copy IPv6 address
#define ipv6CopyAddr(destIpAddr, srcIpAddr) \
   memcpy(destIpAddr, srcIpAddr, sizeof(Ipv6Addr))
//Compare IPv6 addresses
#define ipv6CompAddr(ipAddr1, ipAddr2) \
   (!memcmp(ipAddr1, ipAddr2, sizeof(Ipv6Addr)))

//Determine whether an IPv6 address is a link-local unicast address
#define ipv6IsLinkLocalUnicastAddr(ipAddr) \
   ((ipAddr)->b[0] == 0xFE && ((ipAddr)->b[1] & 0xC0) == 0x80)
//Determine whether an IPv6 address is a multicast address
#define ipv6IsMulticastAddr(ipAddr) \
   ((ipAddr)->b[0] == 0xFF)
//Determine whether an IPv6 address is a solicited-node address
#define ipv6IsSolicitedNodeAddr(ipAddr) \
   ipv6CompPrefix(ipAddr, &IPV6_SOLICITED_NODE_ADDR_PREFIX, 104)

//Determine the scope of a multicast address
#define ipv6MulticastAddrScope(ipAddr) ((ipAddr)->b[1] & 0x0F)


/**
 * @brief Multicast scope value
 **/

typedef enum
{
   IPV6_SCOPE_INTERFACE_LOCAL    = 1,
   IPV6_SCOPE_LINK_LOCAL         = 2,
   IPV6_SCOPE_ADMIN_LOCAL        = 4,
   IPV6_SCOPE_SITE_LOCAL         = 5,
   IPV6_SCOPE_ORGANIZATION_LOCAL = 8,
   IPV6_SCOPE_GLOBAL             = 14
} Ipv6MulticastScope;


/**
 * @brief IPv6 address state
 **/

typedef enum
{
   IPV6_ADDR_STATE_INVALID    = 0, ///<An address that is not assigned to any interface
   IPV6_ADDR_STATE_TENTATIVE  = 1, ///<An address whose uniqueness on a link is being verified
   IPV6_ADDR_STATE_VALID      = 2, ///<A preferred or deprecated address
   IPV6_ADDR_STATE_PREFERRED  = 2, ///<An address assigned to an interface whose use is unrestricted
   IPV6_ADDR_STATE_DEPRECATED = 3  ///<An address assigned to an interface whose use is discouraged
} Ipv6AddrState;


/**
 * @brief IPv6 Next Header types
 **/

typedef enum
{
   IPV6_HOP_BY_HOP_OPT_HEADER = 0,
   IPV6_TCP_HEADER            = 6,
   IPV6_UDP_HEADER            = 17,
   IPV6_ROUTING_HEADER        = 43,
   IPV6_FRAGMENT_HEADER       = 44,
   IPV6_ESP_HEADER            = 50,
   IPV6_AUTH_HEADER           = 51,
   IPV6_ICMPV6_HEADER         = 58,
   IPV6_NO_NEXT_HEADER        = 59,
   IPV6_DEST_OPT_HEADER       = 60
} Ipv6NextHeaderType;


/**
 * @brief IPv6 fragment offset field
 **/

typedef enum
{
   IPV6_OFFSET_MASK = 0xFFF8,
   IPV6_FLAG_RES1   = 0x0004,
   IPV6_FLAG_RES2   = 0x0002,
   IPV6_FLAG_M      = 0x0001
} Ipv6FragmentOffset;


#if (defined(__GNUC__) || defined(_WIN32))
   #define __packed
   #pragma pack(push, 1)
#endif


/**
 * @brief IPv6 network address
 **/

typedef __packed struct
{
   __packed union
   {
      uint8_t b[16];
      uint16_t w[8];
      uint32_t dw[4];
   };
} Ipv6Addr;


/**
 * @brief IPv6 header
 **/

typedef __packed struct
{
   uint8_t trafficClassH : 4; //0
   uint8_t version : 4;
   uint8_t flowLabelH : 4;    //1
   uint8_t trafficClassL : 4;
   uint16_t flowLabelL;       //2-3
   uint16_t payloadLength;    //4-5
   uint8_t nextHeader;        //6
   uint8_t hopLimit;          //7
   Ipv6Addr srcAddr;          //8-23
   Ipv6Addr destAddr;         //24-39
   uint8_t payload[];         //40
} Ipv6Header;


/**
 * @brief IPv6 Hop-by-Hop Options header
 **/

typedef __packed struct
{
   uint8_t nextHeader; //0
   uint8_t hdrExtLen;  //1
   uint8_t options[];  //2
} Ipv6HopByHopOptHeader;


/**
 * @brief IPv6 Destination Options header
 **/

typedef __packed struct
{
   uint8_t nextHeader; //0
   uint8_t hdrExtLen;  //1
   uint8_t options[];  //2
} Ipv6DestOptHeader;


/**
 * @brief IPv6 Type 0 Routing header
 **/

typedef __packed struct
{
   uint8_t nextHeader;   //0
   uint8_t hdrExtLen;    //1
   uint8_t routingType;  //2
   uint8_t segmentsLeft; //3
   uint32_t reserved;    //4-7
   Ipv6Addr address[];   //8
} Ipv6RoutingHeader;


/**
 * @brief IPv6 Fragment header
 **/

typedef __packed struct
{
   uint8_t nextHeader;      //0
   uint8_t reserved;        //1
   uint16_t fragmentOffset; //2-3
   uint32_t identification; //4-7
} Ipv6FragmentHeader;


/**
 * @brief IPv6 Authentication header
 **/

typedef __packed struct
{
   uint8_t nextHeader;          //0
   uint8_t payloadLength;       //1
   uint16_t reserved;           //2-3
   uint32_t securityParamIndex; //4-7
   uint32_t sequenceNumber;     //8-11
   uint8_t authData[];          //12
} Ipv6AuthHeader;


/**
 * @brief IPv6 Encapsulating Security Payload header
 **/

typedef __packed struct
{
   uint32_t securityParamIndex; //0-3
   uint32_t sequenceNumber;     //4-7
   uint8_t payloadData[];       //8
} Ipv6EspHeader;


/**
 * @brief IPv6 pseudo header
 **/

typedef __packed struct
{
   Ipv6Addr srcAddr;        //0-15
   Ipv6Addr destAddr;       //16-31
   uint32_t length;         //32-35
   uint32_t reserved : 24;  //36-38
   uint32_t nextHeader : 8; //39
} Ipv6PseudoHeader;


#if (defined(__GNUC__) || defined(_WIN32))
   #undef __packed
   #pragma pack(pop)
#endif


/**
 * @brief IPv6 configuration
 **/

typedef struct
{
   Ipv6Addr linkLocalAddr;                   ///<IPv6 link-local address
   Ipv6AddrState linkLocalAddrState;         ///<Current state of link-local address
   Ipv6Addr globalAddr;                      ///<IPv6 global address
   Ipv6AddrState globalAddrState;            ///<Current state of global address
   Ipv6Addr prefix;                          ///<IPv6 prefix information
   uint_t prefixLength;                      ///<IPv6 prefix length
   Ipv6Addr router;                          ///<IPv6 router
   Ipv6Addr dnsServer[IPV6_MAX_DNS_SERVERS]; ///<IPv6 DNS servers
   uint_t dnsServerCount;                    ///<Number of IPv6 DNS servers
} Ipv6Config;


/**
 * @brief IPv6 filter table entry
 **/

typedef struct
{
   Ipv6Addr addr;   ///<IPv6 address
   uint_t refCount; ///<Reference count for the current entry
   uint_t state;    ///<MLD node state
   bool_t flag;     ///<MLD flag
   time_t timer;    ///<Delay timer
} Ipv6FilterEntry;


//IPv6 related constants
extern const Ipv6Addr IPV6_UNSPECIFIED_ADDR;
extern const Ipv6Addr IPV6_LOOPBACK_ADDR;
extern const Ipv6Addr IPV6_LINK_LOCAL_ALL_NODES_ADDR;
extern const Ipv6Addr IPV6_LINK_LOCAL_ALL_ROUTERS_ADDR;
extern const Ipv6Addr IPV6_SOLICITED_NODE_ADDR_PREFIX;

//IPv6 related functions
error_t ipv6Init(NetInterface *interface);

void ipv6ProcessPacket(NetInterface *interface,
   const MacAddr *srcMacAddr, ChunkedBuffer *buffer);

error_t ipv6ParseHopByHopOptHeader(NetInterface *interface,
   const ChunkedBuffer *buffer, size_t *offset, size_t *nextHeaderOffset);

error_t ipv6ParseDestOptHeader(NetInterface *interface,
   const ChunkedBuffer *buffer, size_t *offset, size_t *nextHeaderOffset);

error_t ipv6ParseRoutingHeader(NetInterface *interface,
   const ChunkedBuffer *buffer, size_t *offset, size_t *nextHeaderOffset);

error_t ipv6ParseAuthHeader(NetInterface *interface,
   const ChunkedBuffer *buffer, size_t *offset, size_t *nextHeaderOffset);

error_t ipv6ParseEspHeader(NetInterface *interface,
   const ChunkedBuffer *buffer, size_t *offset, size_t *nextHeaderOffset);

error_t ipv6SendDatagram(NetInterface *interface, Ipv6PseudoHeader *pseudoHeader,
   ChunkedBuffer *buffer, size_t offset, uint8_t hopLimit);

error_t ipv6SendPacket(NetInterface *interface, Ipv6PseudoHeader *pseudoHeader,
   uint32_t fragId, uint16_t fragOffset, ChunkedBuffer *buffer, size_t offset, uint8_t hopLimit);

error_t ipv6CheckSourceAddr(NetInterface *interface, const Ipv6Addr *ipAddr);
error_t ipv6CheckDestAddr(NetInterface *interface, const Ipv6Addr *ipAddr);

error_t ipv6SelectSourceAddr(NetInterface **interface,
   const Ipv6Addr *destAddr, Ipv6Addr *srcAddr);

error_t ipv6JoinMulticastGroup(NetInterface *interface, const Ipv6Addr *groupAddr);
error_t ipv6LeaveMulticastGroup(NetInterface *interface, const Ipv6Addr *groupAddr);

bool_t ipv6CompPrefix(const Ipv6Addr *ipAddr1, const Ipv6Addr *ipAddr2, size_t length);
error_t ipv6ComputeSolicitedNodeAddr(const Ipv6Addr *ipAddr, Ipv6Addr *solicitedNodeAddr);
error_t ipv6MapMulticastAddrToMac(const Ipv6Addr *ipAddr, MacAddr *macAddr);

error_t ipv6StringToAddr(const char_t *str, Ipv6Addr *ipAddr);
char_t *ipv6AddrToString(const Ipv6Addr *ipAddr, char_t *str);

void ipv6DumpHeader(const Ipv6Header *ipHeader);

#endif
