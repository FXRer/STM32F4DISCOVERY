/**
 * @file tcp_ip_stack_mem.c
 * @brief Memory management
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
#define TRACE_LEVEL MEM_TRACE_LEVEL

//Dependencies
#include "tcp_ip_stack.h"
#include "tcp_ip_stack_mem.h"
#include "debug.h"

//Maximum number of chunks for dynamically allocated buffers
#if (IPV4_SUPPORT == ENABLED && IPV6_SUPPORT == ENABLED)
   #define MAX_CHUNK_COUNT (N(max(IPV4_MAX_FRAG_DATAGRAM_SIZE, IPV6_MAX_FRAG_DATAGRAM_SIZE)) + 3)
#elif (IPV4_SUPPORT == ENABLED)
   #define MAX_CHUNK_COUNT (N(IPV4_MAX_FRAG_DATAGRAM_SIZE) + 3)
#elif (IPV6_SUPPORT == ENABLED)
   #define MAX_CHUNK_COUNT (N(IPV6_MAX_FRAG_DATAGRAM_SIZE) + 3)
#endif

//Use fixed-size blocks allocation?
#if (MEM_POOL_SUPPORT == ENABLED)

static uint8_t memPool[MEM_POOL_BUFFER_COUNT][MEM_POOL_BUFFER_SIZE];
static bool_t memPoolAllocTable[MEM_POOL_BUFFER_COUNT];

#endif


/**
 * @brief Memory pool initialization
 **/

void memPoolInit(void)
{
//Use fixed-size blocks allocation?
#if (MEM_POOL_SUPPORT == ENABLED)
   //Clear allocation table
   memset(memPoolAllocTable, 0, sizeof(memPoolAllocTable));
#endif
}


/**
 * @brief Allocate a memory block
 * @param[in] size Bytes to allocate
 * @return Pointer to the allocated space or NULL if there is insufficient memory available
 **/

void *memPoolAlloc(size_t size)
{
#if (MEM_POOL_SUPPORT == ENABLED)
   uint_t i;
#endif

   //Pointer to the allocated memory block
   void *p = NULL;

   //Debug message
   TRACE_DEBUG("Allocating %u bytes...\r\n", size);

//Use fixed-size blocks allocation?
#if (MEM_POOL_SUPPORT == ENABLED)
   //Enter critical section
   osTaskSuspendAll();

   //Enforce block size
   if(size <= MEM_POOL_BUFFER_SIZE)
   {
      //Loop through allocation table
      for(i = 0; i < MEM_POOL_BUFFER_COUNT; i++)
      {
         //Check whether the current block is free
         if(!memPoolAllocTable[i])
         {
            //Mark the current entry as used
            memPoolAllocTable[i] = TRUE;
            //Point to the corresponding memory block
            p = memPool[i];
            //Exit immediately
            break;
         }
      }
   }

   //Leave critical section
   osTaskResumeAll();
#else
   //Allocate a memory block
   p = osMemAlloc(size);
#endif

   //Failed to allocate memory?
   if(!p)
   {
      //Debug message
      TRACE_WARNING("Memory allocation failed!\r\n");
   }

   //Return a pointer to the allocated memory block
   return p;
}


/**
 * @brief Release a memory block
 * @param[in] p Previously allocated memory block to be freed
 **/

void memPoolFree(void *p)
{
//Use fixed-size blocks allocation?
#if (MEM_POOL_SUPPORT == ENABLED)
   uint_t i;

   //Enter critical section
   osTaskSuspendAll();

   //Loop through allocation table
   for(i = 0; i < MEM_POOL_BUFFER_COUNT; i++)
   {
      if(memPool[i] == p)
      {
         //Mark the current block as free
         memPoolAllocTable[i] = FALSE;
         //Exit immediately
         break;
      }
   }

   //Leave critical section
   osTaskResumeAll();
#else
   //Release memory block
   osMemFree(p);
#endif
}


/**
 * @brief Allocate a multi-part buffer
 * @param[in] length Desired length
 * @return Pointer to the allocated buffer or NULL if there is
 *   insufficient memory available
 **/

ChunkedBuffer *chunkedBufferAlloc(size_t length)
{
   error_t error;
   ChunkedBuffer *buffer;

   //Allocate memory to hold the multi-part buffer
   buffer = memPoolAlloc(MEM_POOL_BUFFER_SIZE);
   //Failed to allocate memory?
   if(!buffer) return NULL;

   //The multi-part buffer consists of a single chunk
   buffer->chunkCount = 1;
   buffer->maxChunkCount = MAX_CHUNK_COUNT;
   buffer->chunk[0].address = (uint8_t *) buffer + MAX_CHUNK_COUNT * sizeof(ChunkDesc);
   buffer->chunk[0].length = MEM_POOL_BUFFER_SIZE - MAX_CHUNK_COUNT * sizeof(ChunkDesc);
   buffer->chunk[0].size = 0;

   //Adjust the length of the buffer
   error = chunkedBufferSetLength(buffer, length);
   //Any error to report?
   if(error)
   {
      //Clean up side effects
      chunkedBufferFree(buffer);
      //Report an failure
      return NULL;
   }

   //Successful memory allocation
   return buffer;
}


/**
 * @brief Dispose a multi-part buffer
 * @param[in] buffer Pointer to the multi-part buffer to be released
 **/

void chunkedBufferFree(ChunkedBuffer *buffer)
{
   //Properly dispose data chunks
   chunkedBufferSetLength(buffer, 0);
   //Release multi-part buffer
   memPoolFree(buffer);
}


/**
 * @brief Get the actual length of a multi-part buffer
 * @param[in] buffer Pointer to a multi-part buffer
 * @return Actual length in bytes
 **/

size_t chunkedBufferGetLength(const ChunkedBuffer *buffer)
{
   uint_t i;

   //Total length
   size_t length = 0;

   //Loop through data chunks
   for(i = 0; i < buffer->chunkCount; i++)
      length += buffer->chunk[i].length;

   //Return total length
   return length;
}


/**
 * @brief Adjust the length of a multi-part buffer
 * @param[in] buffer Pointer to the multi-part buffer whose length is to be changed
 * @param[in] length Desired length
 * @return Error code
 **/

error_t chunkedBufferSetLength(ChunkedBuffer *buffer, size_t length)
{
   uint_t i;
   uint_t chunkCount;
   ChunkDesc *chunk;

   //Get the actual number of chunks
   chunkCount = buffer->chunkCount;

   //Loop through data chunks
   for(i = 0; i < chunkCount && length > 0; i++)
   {
      //Point to the chunk descriptor;
      chunk = &buffer->chunk[i];

      //Adjust the length of the current chunk when possible
      if(length <= chunk->length)
      {
         chunk->length = length;
      }
      else if(chunk->size > 0 && i == (chunkCount - 1))
      {
         chunk->length = min(length, chunk->size);
      }

      //Prepare to process next chunk
      length -= chunk->length;
   }

   //The size of the buffer should be decreased?
   if(!length)
   {
      //Adjust the number of chunks
      buffer->chunkCount = i;

      //Delete unnecessary data chunks
      while(i < chunkCount)
      {
         //Point to the chunk descriptor;
         chunk = &buffer->chunk[i];

         //Release previously allocated memory
         if(chunk->size > 0)
            memPoolFree(chunk->address);

         //Mark the current chunk as free
         chunk->address = NULL;
         chunk->length = 0;
         chunk->size = 0;

         //Next chunk
         i++;
      }
   }
   //The size of the buffer should be increased?
   else
   {
      //Add as many chunks as necessary
      while(i < buffer->maxChunkCount && length > 0)
      {
         //Point to the chunk descriptor;
         chunk = &buffer->chunk[i];

         //Allocate memory to hold a new chunk
         chunk->address = memPoolAlloc(MEM_POOL_BUFFER_SIZE);
         //Failed to allocate memory?
         if(!chunk->address) return ERROR_OUT_OF_MEMORY;

         //Allocated memory
         chunk->size = MEM_POOL_BUFFER_SIZE;
         //Actual length of the data chunk
         chunk->length = min(length, MEM_POOL_BUFFER_SIZE);

         //Prepare to process next chunk
         length -= chunk->length;
         buffer->chunkCount++;
         i++;
      }
   }

   //Return status code
   return (length > 0) ? ERROR_OUT_OF_RESOURCES : NO_ERROR;
}


/**
 * @brief Returns a pointer to the data at the specified position
 * @param[in] buffer Pointer to a multi-part buffer
 * @param[in] offset Offset from the beginning of the buffer
 * @return Pointer the data at the specified position
 **/

void *chunkedBufferAt(const ChunkedBuffer *buffer, size_t offset)
{
   uint_t i;

   //Loop through data chunks
   for(i = 0; i < buffer->chunkCount; i++)
   {
      //The data at the specified offset resides in the current chunk?
      if(offset < buffer->chunk[i].length)
         return (uint8_t *) buffer->chunk[i].address + offset;

      //Jump to the next chunk
      offset -= buffer->chunk[i].length;
   }

   //Invalid offset...
   return NULL;
}


/**
 * @brief Concatenate two multi-part buffers
 * @param[out] dest Pointer to the destination buffer
 * @param[in] src Pointer to the source buffer
 * @param[in] srcOffset Read offset
 * @param[in] length Number of bytes to read from the source buffer
 * @return Error code
 **/

error_t chunkedBufferConcat(ChunkedBuffer *dest,
   const ChunkedBuffer *src, size_t srcOffset, size_t length)
{
   uint_t i;
   uint_t j;

   //Skip the beginning of the source data
   for(j = 0; j < src->chunkCount; j++)
   {
      //The data at the specified offset resides in the current chunk?
      if(srcOffset < src->chunk[j].length)
         break;

      //Jump to the next chunk
      srcOffset -= src->chunk[j].length;
   }

   //Invalid offset?
   if(j >= src->chunkCount)
      return ERROR_INVALID_PARAMETER;

   //Position to the end of the destination data
   i = dest->chunkCount;

   //Copy data blocks
   while(length > 0 && i < dest->maxChunkCount && j < src->chunkCount)
   {
      //Copy current block
      dest->chunk[i].address = (uint8_t *) src->chunk[j].address + srcOffset;
      dest->chunk[i].length = src->chunk[j].length - srcOffset;
      dest->chunk[i].size = 0;

      //Limit the number of bytes to copy
      if(length < dest->chunk[i].length)
         dest->chunk[i].length = length;

      //Decrement the number of remaining bytes
      length -= dest->chunk[i].length;
      //Increment the number of chunks
      dest->chunkCount++;

      //Adjust variables
      srcOffset = 0;
      i++;
      j++;
   }

   //Return status code
   return (length > 0) ? ERROR_FAILURE : NO_ERROR;
}


/**
 * @brief Copy data between multi-part buffers
 * @param[out] dest Pointer to the destination buffer
 * @param[in] destOffset Write offset
 * @param[in] src Pointer to the source buffer
 * @param[in] srcOffset Read offset
 * @param[in] length Number of bytes to be copied
 * @return Error code
 **/

error_t chunkedBufferCopy(ChunkedBuffer *dest, size_t destOffset,
   const ChunkedBuffer *src, size_t srcOffset, size_t length)
{
   uint_t i;
   uint_t j;
   uint_t n;
   uint8_t *p;
   uint8_t *q;

   //Skip the beginning of the source data
   for(i = 0; i < dest->chunkCount; i++)
   {
      //The data at the specified offset resides in the current chunk?
      if(destOffset < dest->chunk[i].length)
         break;

      //Jump to the next chunk
      destOffset -= dest->chunk[i].length;
   }

   //Invalid offset?
   if(i >= dest->chunkCount)
      return ERROR_INVALID_PARAMETER;

   //Skip the beginning of the source data
   for(j = 0; j < src->chunkCount; j++)
   {
      //The data at the specified offset resides in the current chunk?
      if(srcOffset < src->chunk[j].length)
         break;

      //Jump to the next chunk
      srcOffset -= src->chunk[j].length;
   }

   //Invalid offset?
   if(j >= src->chunkCount)
      return ERROR_INVALID_PARAMETER;

   while(length > 0 && i < dest->chunkCount && j < src->chunkCount)
   {
      //Point to the first data byte
      p = (uint8_t *) dest->chunk[i].address + destOffset;
      q = (uint8_t *) src->chunk[j].address + srcOffset;

      //Compute the number of bytes to copy
      n = min(length, dest->chunk[i].length - destOffset);
      n = min(n, src->chunk[j].length - srcOffset);

      //Copy data
      memcpy(p, q, n);

      destOffset += n;
      srcOffset += n;
      length -= n;

      if(destOffset >= dest->chunk[i].length)
      {
         destOffset = 0;
         i++;
      }

      if(srcOffset >= src->chunk[j].length)
      {
         srcOffset = 0;
         j++;
      }
   }

   //Return status code
   return (length > 0) ? ERROR_FAILURE : NO_ERROR;
}


/**
 * @brief Append data a multi-part buffer
 * @param[out] dest Pointer to a multi-part buffer
 * @param[in] src User buffer containing the data to be appended
 * @param[in] length Number of bytes in the user buffer
 * @return Error code
 **/

error_t chunkedBufferAppend(ChunkedBuffer *dest, const void *src, size_t length)
{
   uint_t i;

   //Make sure there is enough space to add an extra chunk
   if(dest->chunkCount >= dest->maxChunkCount)
      return ERROR_FAILURE;

   //Position to the end of the buffer
   i = dest->chunkCount;

   //Insert a new chunk at the end of the list
   dest->chunk[i].address = (void *) src;
   dest->chunk[i].length = length;
   dest->chunk[i].size = 0;

   //Increment the number of chunks
   dest->chunkCount++;

   //Successful processing
   return NO_ERROR;
}


/**
 * @brief Write data to a multi-part buffer
 * @param[out] dest Pointer to a multi-part buffer
 * @param[in] destOffset Offset from the beginning of the multi-part buffer
 * @param[in] src User buffer containing the data to be written
 * @param[in] length Number of bytes to copy
 * @return Actual number of bytes copied
 **/

size_t chunkedBufferWrite(ChunkedBuffer *dest,
   size_t destOffset, const void *src, size_t length)
{
   uint_t i;
   uint_t n;
   size_t totalLength;
   uint8_t *p;

   //Total number of bytes written
   totalLength = 0;

   //Loop through data chunks
   for(i = 0; i < dest->chunkCount && totalLength < length; i++)
   {
      //Is there any data to copy in the current chunk?
      if(destOffset < dest->chunk[i].length)
      {
         //Point to the first byte to be written
         p = (uint8_t *) dest->chunk[i].address + destOffset;
         //Compute the number of bytes to copy at a time
         n = min(length - totalLength, dest->chunk[i].length - destOffset);

         //Copy data
         memcpy(p, src, n);

         //Advance read pointer
         src = (uint8_t *) src + n;
         //Total number of bytes written
         totalLength += n;
         //Process the next block from the start
         destOffset = 0;
      }
      else
      {
         //Skip the current chunk
         destOffset -= dest->chunk[i].length;
      }
   }

   //Return the actual number of bytes written
   return totalLength;
}


/**
 * @brief Read data from a multi-part buffer
 * @param[out] dest Pointer to the buffer where to return the data
 * @param[in] src Pointer to a multi-part buffer
 * @param[in] srcOffset Offset from the beginning of the multi-part buffer
 * @param[in] length Number of bytes to copy
 * @return Actual number of bytes copied
 **/

size_t chunkedBufferRead(void *dest, const ChunkedBuffer *src,
   size_t srcOffset, size_t length)
{
   uint_t i;
   uint_t n;
   size_t totalLength;
   uint8_t *p;

   //Total number of bytes copied
   totalLength = 0;

   //Loop through data chunks
   for(i = 0; i < src->chunkCount && totalLength < length; i++)
   {
      //Is there any data to copy from the current chunk?
      if(srcOffset < src->chunk[i].length)
      {
         //Point to the first byte to be read
         p = (uint8_t *) src->chunk[i].address + srcOffset;
         //Compute the number of bytes to copy at a time
         n = min(length - totalLength, src->chunk[i].length - srcOffset);

         //Copy data
         memcpy(dest, p, n);

         //Advance write pointer
         dest = (uint8_t *) dest + n;
         //Total number of bytes copied
         totalLength += n;
         //Process the next block from the start
         srcOffset = 0;
      }
      else
      {
         //Skip the current chunk
         srcOffset -= src->chunk[i].length;
      }
   }

   //Return the actual number of bytes copied
   return totalLength;
}
