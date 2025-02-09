/**
 * @file nic.c
 * @brief Network interface controller abstraction layer
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

//Switch to the appropriate trace level
#define TRACE_LEVEL NIC_TRACE_LEVEL

//Dependencies
#include "tcp_ip_stack.h"
#include "nic.h"
#include "socket.h"
#include "igmp.h"
#include "mld.h"
#include "tcp_misc.h"
#include "udp.h"
#include "raw_socket.h"
#include "debug.h"


/**
 * @brief Ethernet controller timer handler
 *
 * This routine is periodically called by the TCP/IP stack to
 * handle periodic operations such as polling the link state
 *
 * @param[in] interface Underlying network interface
 **/

void nicTick(NetInterface *interface)
{
   //Get exclusive access to the device
   osTaskSuspendAll();
   //Disable interrupts
   interface->nicDriver->disableIrq(interface);

   //Handle periodic operations
   interface->nicDriver->tick(interface);

   //Re-enable interrupts
   interface->nicDriver->enableIrq(interface);
   //Release exclusive access to the device
   osTaskResumeAll();
}


/**
 * @brief Configure multicast MAC address filtering
 * @param[in] interface Underlying network interface
 * @return Error code
 **/

error_t nicSetMacFilter(NetInterface *interface)
{
   error_t error;

   //Get exclusive access to the device
   osTaskSuspendAll();
   //Disable interrupts
   interface->nicDriver->disableIrq(interface);

   //Update MAC filter table
   error = interface->nicDriver->setMacFilter(interface);

   //Re-enable interrupts
   interface->nicDriver->enableIrq(interface);
   //Release exclusive access to the device
   osTaskResumeAll();

   //Return status code
   return error;
}


/**
 * @brief Send a packet to the network controller
 * @param[in] interface Underlying network interface
 * @param[in] buffer Multi-part buffer containing the data to send
 * @param[in] offset Offset to the first data byte
 * @return Error code
 **/

error_t nicSendPacket(NetInterface *interface, const ChunkedBuffer *buffer, size_t offset)
{
   error_t error;

#if (TRACE_LEVEL >= TRACE_LEVEL_DEBUG)
   //Retrieve the length of the packet
   size_t length = chunkedBufferGetLength(buffer) - offset;

   //Debug message
   TRACE_DEBUG("Sending packet (%u bytes)...\r\n", length);
   TRACE_DEBUG_CHUNKED_BUFFER("  ", buffer, offset, length);
#endif

   //Wait for the transmitter to be ready to send
   osEventWait(interface->nicTxEvent, INFINITE_DELAY);

   //Get exclusive access to the device
   osTaskSuspendAll();
   //Disable interrupts
   interface->nicDriver->disableIrq(interface);

   //Send Ethernet frame
   error = interface->nicDriver->sendPacket(interface, buffer, offset);

   //Re-enable interrupts
   interface->nicDriver->enableIrq(interface);
   //Release exclusive access to the device
   osTaskResumeAll();

   //Return status code
   return error;
}


/**
 * @brief Handle a packet received by the network controller
 * @param[in] interface Underlying network interface
 * @param[in] packet Incoming packet to process
 * @param[in] length Total packet length
 **/

void nicProcessPacket(NetInterface *interface, void *packet, size_t length)
{
   //Re-enable interrupts
   interface->nicDriver->enableIrq(interface);
   //Release exclusive access to the device
   osTaskResumeAll();

   //Debug message
   TRACE_DEBUG("Packet received (%u bytes)...\r\n", length);
   TRACE_DEBUG_ARRAY("  ", packet, length);

   //Process incoming Ethernet frame
   ethProcessFrame(interface, packet, length);

   //Get exclusive access to the device
   osTaskSuspendAll();
   //Disable interrupts
   interface->nicDriver->disableIrq(interface);
}


/**
 * @brief Process link state change event
 * @param[in] interface Underlying network interface
 **/

void nicNotifyLinkChange(NetInterface *interface)
{
   uint_t i;
   Socket *socket;

   //Re-enable interrupts
   interface->nicDriver->enableIrq(interface);
   //Release exclusive access to the device
   osTaskResumeAll();

#if (IPV4_SUPPORT == ENABLED)
   //Flush ARP cache contents
   arpFlushCache(interface);
#endif

#if (IPV4_SUPPORT == ENABLED && IPV4_FRAG_SUPPORT == ENABLED)
   //Flush the reassembly queue
   ipv4FlushFragQueue(interface);
#endif

#if (IPV4_SUPPORT == ENABLED && IGMP_SUPPORT == ENABLED)
   //Notify IGMP of link state changes
   igmpLinkChangeEvent(interface);
#endif

#if (IPV6_SUPPORT == ENABLED)
   //Flush Neighbor cache contents
   ndpFlushCache(interface);
#endif

#if (IPV6_SUPPORT == ENABLED && IPV6_FRAG_SUPPORT == ENABLED)
   //Flush the reassembly queue
   ipv6FlushFragQueue(interface);
#endif

#if (IPV6_SUPPORT == ENABLED && MLD_SUPPORT == ENABLED)
   //Notify MLD of link state changes
   mldLinkChangeEvent(interface);
#endif

   //Loop through opened sockets
   for(i = 0; i < SOCKET_MAX_COUNT; i++)
   {
      //Point to the current socket
      socket = socketTable + i;

#if (TCP_SUPPORT == ENABLED)
      //Connection-oriented socket?
      if(socket->type == SOCKET_TYPE_STREAM)
         tcpUpdateEvents(socket);
#endif
#if (UDP_SUPPORT == ENABLED)
      //Connectionless socket?
      if(socket->type == SOCKET_TYPE_DGRAM)
         udpUpdateEvents(socket);
#endif
#if (RAW_SOCKET_SUPPORT == ENABLED)
      //Raw socket?
      if(socket->type == SOCKET_TYPE_RAW)
         rawSocketUpdateEvents(socket);
#endif
   }

   //Get exclusive access to the device
   osTaskSuspendAll();
   //Disable interrupts
   interface->nicDriver->disableIrq(interface);
}
