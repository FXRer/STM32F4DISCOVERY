/**
 * @file mld.c
 * @brief MLD (Multicast Listener Discovery for IPv6)
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
 * @section Description
 *
 * MLD is used by an IPv6 router to discover the presence of multicast
 * listeners on its directly attached links, and to discover specifically
 * which multicast addresses are of interest to those neighboring nodes.
 * Refer to the following RFCs for complete details:
 * - RFC 2710: Multicast Listener Discovery (MLD) for IPv6
 * - RFC 3810: Multicast Listener Discovery Version 2 (MLDv2) for IPv6
 *
 * @author Oryx Embedded (www.oryx-embedded.com)
 * @version 1.3.5
 **/

//Switch to the appropriate trace level
#define TRACE_LEVEL MLD_TRACE_LEVEL

//Dependencies
#include <stdlib.h>
#include "tcp_ip_stack.h"
#include "ip.h"
#include "ipv6.h"
#include "icmpv6.h"
#include "mld.h"
#include "debug.h"

//Check TCP/IP stack configuration
#if (IPV6_SUPPORT == ENABLED && MLD_SUPPORT == ENABLED)


/**
 * @brief MLD initialization
 * @param[in] interface Underlying network interface
 * @return Error code
 **/

error_t mldInit(NetInterface *interface)
{
   //Successful initialization
   return NO_ERROR;
}


/**
 * @brief Start listening to the address on the interface
 * @param[in] interface Underlying network interface
 * @param[in] entry IPv6 filter entry identifying the address to listen to
 * @return Error code
 **/

error_t mldStartListening(NetInterface *interface, Ipv6FilterEntry *entry)
{
   //The link-scope all-nodes address (FF02::1) is handled as a special
   //case. The host starts in Idle Listener state for that address on
   //every interface and never transitions to another state
   if(ipv6CompAddr(&entry->addr, &IPV6_LINK_LOCAL_ALL_NODES_ADDR))
   {
      //Clear flag
      entry->flag = FALSE;
      //Enter the Idle Listener state
      entry->state = MLD_STATE_IDLE_LISTENER;
   }
   else
   {
      //Link is up?
      if(interface->linkState)
      {
         //Send a Multicast Listener Report message for the group on the interface
         mldSendListenerReport(interface, &entry->addr);

         //Set flag
         entry->flag = TRUE;
         //Start timer
         entry->timer = osGetTickCount() + MLD_UNSOLICITED_REPORT_INTERVAL;
         //Enter the Delaying Listener state
         entry->state = MLD_STATE_DELAYING_LISTENER;
      }
      //Link is down?
      else
      {
         //Clear flag
         entry->flag = FALSE;
         //Enter the Idle Listener state
         entry->state = MLD_STATE_IDLE_LISTENER;
      }
   }

   //Successful processing
   return NO_ERROR;
}


/**
 * @brief Stop listening to the address on the interface
 * @param[in] interface Underlying network interface
 * @param[in] entry IPv6 filter entry identifying the multicast address to leave
 * @return Error code
 **/

error_t mldStopListening(NetInterface *interface, Ipv6FilterEntry *entry)
{
   //Check link state
   if(interface->linkState)
   {
      //Send a Multicast Listener Done message if the flag is set
      if(entry->flag)
         mldSendListenerDone(interface, &entry->addr);
   }

   //Switch to the Non-Listener state
   entry->state = MLD_STATE_NON_LISTENER;

   //Successful processing
   return NO_ERROR;
}


/**
 * @brief MLD timer handler
 *
 * This routine must be periodically called by the TCP/IP stack to
 * handle MLD related timers
 *
 * @param[in] interface Underlying network interface
 **/

void mldTick(NetInterface *interface)
{
   uint_t i;
   time_t time;
   Ipv6FilterEntry *entry;

   //Get current time
   time = osGetTickCount();

   //Acquire exclusive access to the IPv6 filter table
   osMutexAcquire(interface->ipv6FilterMutex);

   //Loop through filter table entries
   for(i = 0; i < interface->ipv6FilterSize; i++)
   {
      //Point to the current entry
      entry = &interface->ipv6Filter[i];

      //Delaying Listener state?
      if(entry->state == MLD_STATE_DELAYING_LISTENER)
      {
         //Timer expired?
         if(timeCompare(time, entry->timer) >= 0)
         {
            //Send a Multicast Listener Report message
            mldSendListenerReport(interface, &entry->addr);

            //Set flag
            entry->flag = TRUE;
            //Switch to the Idle Listener state
            entry->state = MLD_STATE_IDLE_LISTENER;
         }
      }
   }

   //Release exclusive access to the IPv6 filter table
   osMutexRelease(interface->ipv6FilterMutex);
}


/**
 * @brief Callback function for link change event
 * @param[in] interface Underlying network interface
 **/

void mldLinkChangeEvent(NetInterface *interface)
{
   uint_t i;
   time_t time;
   Ipv6FilterEntry *entry;

   //Get current time
   time = osGetTickCount();

   //Acquire exclusive access to the IPv6 filter table
   osMutexAcquire(interface->ipv6FilterMutex);

   //Link up event?
   if(interface->linkState)
   {
      //Loop through filter table entries
      for(i = 0; i < interface->ipv6FilterSize; i++)
      {
         //Point to the current entry
         entry = &interface->ipv6Filter[i];

         //The link-scope all-nodes address (FF02::1) is handled as a special
         //case. The host starts in Idle Listener state for that address on
         //every interface and never transitions to another state
         if(ipv6CompAddr(&entry->addr, &IPV6_LINK_LOCAL_ALL_NODES_ADDR))
            continue;

         //Send an unsolicited Multicast Listener Report message for that group
         mldSendListenerReport(interface, &entry->addr);

         //Set flag
         entry->flag = TRUE;
         //Start timer
         entry->timer = time + MLD_UNSOLICITED_REPORT_INTERVAL;
         //Enter the Delaying Listener state
         entry->state = MLD_STATE_DELAYING_LISTENER;
      }
   }
   //Link down event?
   else
   {
      //Loop through filter table entries
      for(i = 0; i < interface->ipv6FilterSize; i++)
      {
         //Point to the current entry
         entry = &interface->ipv6Filter[i];

         //Clear flag
         entry->flag = FALSE;
         //Enter the Idle Listener state
         entry->state = MLD_STATE_IDLE_LISTENER;
      }
   }

   //Release exclusive access to the IPv6 filter table
   osMutexRelease(interface->ipv6FilterMutex);
}


/**
 * @brief Process incoming Multicast Listener Query message
 * @param[in] interface Underlying network interface
 * @param[in] pseudoHeader IPv6 pseudo header
 * @param[in] buffer Multi-part buffer containing the incoming MLD message
 * @param[in] offset Offset to the first byte of the MLD message
 * @param[in] hopLimit Hop Limit field from IPv6 header
 **/

void mldProcessListenerQuery(NetInterface *interface, Ipv6PseudoHeader *pseudoHeader,
   const ChunkedBuffer *buffer, size_t offset, uint8_t hopLimit)
{
   uint_t i;
   size_t length;
   time_t time;
   time_t maxRespDelay;
   MldMessage *message;
   Ipv6FilterEntry *entry;

   //Retrieve the length of the MLD message
   length = chunkedBufferGetLength(buffer) - offset;

   //The message must be at least 24 octets long
   if(length < sizeof(MldMessage))
      return;

   //Point to the beginning of the MLD message
   message = chunkedBufferAt(buffer, offset);
   //Sanity check
   if(!message) return;

   //Debug message
   TRACE_INFO("MLD message received (%u bytes)...\r\n", length);
   //Dump message contents for debugging purpose
   mldDumpMessage(message);

   //Make sure the source address of the message is a valid link-local address
   if(!ipv6IsLinkLocalUnicastAddr(&pseudoHeader->srcAddr))
      return;
   //Check the Hop Limit field
   if(hopLimit != MLD_HOP_LIMIT)
      return;

   //Get current time
   time = osGetTickCount();

   //The Max Resp Delay field specifies the maximum time allowed
   //before sending a responding report
   maxRespDelay = message->maxRespDelay * 10;

   //Acquire exclusive access to the IPv6 filter table
   osMutexAcquire(interface->ipv6FilterMutex);

   //Loop through filter table entries
   for(i = 0; i < interface->ipv6FilterSize; i++)
   {
      //Point to the current entry
      entry = &interface->ipv6Filter[i];

      //The link-scope all-nodes address (FF02::1) is handled as a special
      //case. The host starts in Idle Listener state for that address on
      //every interface and never transitions to another state
      if(ipv6CompAddr(&entry->addr, &IPV6_LINK_LOCAL_ALL_NODES_ADDR))
         continue;

      //A General Query is used to learn which multicast addresses have listeners
      //on an attached link. A Multicast-Address-Specific Query is used to learn
      //if a particular multicast address has any listeners on an attached link
      if(ipv6CompAddr(&message->multicastAddr, &IPV6_UNSPECIFIED_ADDR) ||
         ipv6CompAddr(&message->multicastAddr, &entry->addr))
      {
         //Delaying Listener state?
         if(entry->state == MLD_STATE_DELAYING_LISTENER)
         {
            //The timer has not yet expired?
            if(timeCompare(time, entry->timer) < 0)
            {
               //If a timer for the address is already running, it is reset to
               //the new random value only if the requested Max Response Delay
               //is less than the remaining value of the running timer
               if(maxRespDelay < (entry->timer - time))
               {
                  //Restart delay timer
                  entry->timer = time + mldRand(maxRespDelay);
               }
            }
         }
         //Idle Listener state?
         else if(entry->state == MLD_STATE_IDLE_LISTENER)
         {
            //Switch to the Delaying Listener state
            entry->state = MLD_STATE_DELAYING_LISTENER;
            //Delay the response by a random amount of time
            entry->timer = time + mldRand(maxRespDelay);
         }
      }
   }

   //Release exclusive access to the IPv6 filter table
   osMutexRelease(interface->ipv6FilterMutex);
}


/**
 * @brief Process incoming Multicast Listener Report message
 * @param[in] interface Underlying network interface
 * @param[in] pseudoHeader IPv6 pseudo header
 * @param[in] buffer Multi-part buffer containing the incoming MLD message
 * @param[in] offset Offset to the first byte of the MLD message
 * @param[in] hopLimit Hop Limit field from IPv6 header
 **/

void mldProcessListenerReport(NetInterface *interface, Ipv6PseudoHeader *pseudoHeader,
   const ChunkedBuffer *buffer, size_t offset, uint8_t hopLimit)
{
   uint_t i;
   size_t length;
   MldMessage *message;
   Ipv6FilterEntry *entry;

   //Retrieve the length of the MLD message
   length = chunkedBufferGetLength(buffer) - offset;

   //The message must be at least 24 octets long
   if(length < sizeof(MldMessage))
      return;

   //Point to the beginning of the MLD message
   message = chunkedBufferAt(buffer, offset);
   //Sanity check
   if(!message) return;

   //Debug message
   TRACE_INFO("MLD message received (%u bytes)...\r\n", length);
   //Dump message contents for debugging purpose
   mldDumpMessage(message);

   //Make sure the source address of the message is a valid link-local address
   if(!ipv6IsLinkLocalUnicastAddr(&pseudoHeader->srcAddr))
      return;
   //Check the Hop Limit field
   if(hopLimit != MLD_HOP_LIMIT)
      return;

   //Acquire exclusive access to the IPv6 filter table
   osMutexAcquire(interface->ipv6FilterMutex);

   //Loop through filter table entries
   for(i = 0; i < interface->ipv6FilterSize; i++)
   {
      //Point to the current entry
      entry = &interface->ipv6Filter[i];

      //Report messages are ignored for multicast addresses
      //in the Non-Listener or Idle Listener state
      if(entry->state == MLD_STATE_DELAYING_LISTENER)
      {
         //The Multicast Listener Report message matches the current entry?
         if(ipv6CompAddr(&message->multicastAddr, &entry->addr))
         {
            //Clear flag
            entry->flag = FALSE;
            //Switch to the Idle Listener state
            entry->state = MLD_STATE_IDLE_LISTENER;
         }
      }
   }

   //Release exclusive access to the IPv6 filter table
   osMutexRelease(interface->ipv6FilterMutex);
}


/**
 * @brief Send Multicast Listener Report message
 * @param[in] interface Underlying network interface
 * @param[in] ipAddr IPv6 address specifying the multicast address
 * @return Error code
 **/

error_t mldSendListenerReport(NetInterface *interface, Ipv6Addr *ipAddr)
{
   error_t error;
   size_t offset;
   MldMessage *message;
   ChunkedBuffer *buffer;
   Ipv6PseudoHeader pseudoHeader;

   //Make sure the specified address is a valid multicast address
   if(!ipv6IsMulticastAddr(ipAddr))
      return ERROR_INVALID_ADDRESS;

   //The link-scope all-nodes address (FF02::1) is handled as a special
   //case. The host never sends a report for that address
   if(ipv6CompAddr(ipAddr, &IPV6_LINK_LOCAL_ALL_NODES_ADDR))
      return ERROR_INVALID_ADDRESS;

   //Allocate a memory buffer to hold a MLD message
   buffer = ipAllocBuffer(sizeof(MldMessage), &offset);
   //Failed to allocate memory?
   if(!buffer) return ERROR_OUT_OF_MEMORY;

   //Point to the beginning of the MLD message
   message = chunkedBufferAt(buffer, offset);

   //Format the Multicast Listener Report message
   message->type = ICMPV6_TYPE_MULTICAST_LISTENER_REPORT_V1;
   message->code = 0;
   message->checksum = 0;
   message->maxRespDelay = 0;
   message->reserved = 0;
   message->multicastAddr = *ipAddr;

   //Format IPv6 pseudo header
   pseudoHeader.srcAddr = interface->ipv6Config.linkLocalAddr;
   pseudoHeader.destAddr = *ipAddr;
   pseudoHeader.length = HTONS(sizeof(MldMessage));
   pseudoHeader.reserved = 0;
   pseudoHeader.nextHeader = IPV6_ICMPV6_HEADER;

   //Message checksum calculation
   message->checksum = ipCalcUpperLayerChecksumEx(&pseudoHeader,
      sizeof(Ipv6PseudoHeader), buffer, offset, sizeof(MldMessage));

   //Debug message
   TRACE_INFO("Sending MLD message (%u bytes)...\r\n", sizeof(MldMessage));
   //Dump message contents for debugging purpose
   mldDumpMessage(message);

   //The Multicast Listener Report message is sent to the multicast address being reported
   error = ipv6SendDatagram(interface, &pseudoHeader, buffer, offset, MLD_HOP_LIMIT);

   //Free previously allocated memory
   chunkedBufferFree(buffer);
   //Return status code
   return error;
}


/**
 * @brief Send Multicast Listener Done message
 * @param[in] interface Underlying network interface
 * @param[in] ipAddr IPv6 address specifying the multicast address being left
 * @return Error code
 **/

error_t mldSendListenerDone(NetInterface *interface, Ipv6Addr *ipAddr)
{
   error_t error;
   size_t offset;
   MldMessage *message;
   ChunkedBuffer *buffer;
   Ipv6PseudoHeader pseudoHeader;

   //Make sure the specified address is a valid multicast address
   if(!ipv6IsMulticastAddr(ipAddr))
      return ERROR_INVALID_ADDRESS;

   //The link-scope all-nodes address (FF02::1) is handled as a special
   //case. The host never sends a report for that address
   if(ipv6CompAddr(ipAddr, &IPV6_LINK_LOCAL_ALL_NODES_ADDR))
      return ERROR_INVALID_ADDRESS;

   //Allocate a memory buffer to hold a MLD message
   buffer = ipAllocBuffer(sizeof(MldMessage), &offset);
   //Failed to allocate memory?
   if(!buffer) return ERROR_OUT_OF_MEMORY;

   //Point to the beginning of the MLD message
   message = chunkedBufferAt(buffer, offset);

   //Format the Multicast Listener Done message
   message->type = ICMPV6_TYPE_MULTICAST_LISTENER_DONE_V1;
   message->code = 0;
   message->checksum = 0;
   message->maxRespDelay = 0;
   message->reserved = 0;
   message->multicastAddr = *ipAddr;

   //Format IPv6 pseudo header
   pseudoHeader.srcAddr = interface->ipv6Config.linkLocalAddr;
   pseudoHeader.destAddr = IPV6_LINK_LOCAL_ALL_ROUTERS_ADDR;
   pseudoHeader.length = HTONS(sizeof(MldMessage));
   pseudoHeader.reserved = 0;
   pseudoHeader.nextHeader = IPV6_ICMPV6_HEADER;

   //Message checksum calculation
   message->checksum = ipCalcUpperLayerChecksumEx(&pseudoHeader,
      sizeof(Ipv6PseudoHeader), buffer, offset, sizeof(MldMessage));

   //Debug message
   TRACE_INFO("Sending MLD message (%u bytes)...\r\n", sizeof(MldMessage));
   //Dump message contents for debugging purpose
   mldDumpMessage(message);

   //The Multicast Listener Done message is sent to the all-routers multicast address
   error = ipv6SendDatagram(interface, &pseudoHeader, buffer, offset, MLD_HOP_LIMIT);

   //Free previously allocated memory
   chunkedBufferFree(buffer);
   //Return status code
   return error;
}


/**
 * @brief Get a random value in the specified range
 * @param[in] max Upper bound
 * @return Random value in the specified range
 **/

uint32_t mldRand(uint32_t max)
{
   //Return a random value in the given range
   return rand() % (max + 1);
}


/**
 * @brief Dump MLD message for debugging purpose
 * @param[in] message Pointer to the MLD message
 **/

void mldDumpMessage(const MldMessage *message)
{
   //Dump MLD message
   TRACE_DEBUG("  Type = %u\r\n", message->type);
   TRACE_DEBUG("  Code = %u\r\n", message->code);
   TRACE_DEBUG("  Checksum = 0x%04X\r\n", ntohs(message->checksum));
   TRACE_DEBUG("  Max Resp Delay = %u\r\n", message->maxRespDelay);
   TRACE_DEBUG("  Multicast Address = %s\r\n", ipv6AddrToString(&message->multicastAddr, NULL));
}

#endif
