/**
 * @file sha224.c
 * @brief SHA-224 (Secure Hash Algorithm 224)
 *
 * @section License
 *
 * Copyright (C) 2010-2013 Oryx Embedded. All rights reserved.
 *
 * This file is part of CycloneCrypto Open.
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
 * SHA-224 is a secure hash algorithm for computing a condensed representation
 * of an electronic message. Refer to FIPS 180-4 for more details
 *
 * @author Oryx Embedded (www.oryx-embedded.com)
 * @version 1.3.5
 **/

//Switch to the appropriate trace level
#define TRACE_LEVEL CRYPTO_TRACE_LEVEL

//Dependencies
#include <string.h>
#include "crypto.h"
#include "sha224.h"

//Check crypto library configuration
#if (SHA224_SUPPORT == ENABLED)

//SHA-224 object identifier (2.16.840.1.101.3.4.2.4)
static const uint8_t sha224Oid[] = {0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x04};

//Common interface for hash algorithms
const HashAlgo sha224HashAlgo =
{
   "SHA-224",
   sha224Oid,
   sizeof(sha224Oid),
   sizeof(Sha224Context),
   SHA224_BLOCK_SIZE,
   SHA224_DIGEST_SIZE,
   (HashAlgoCompute) sha224Compute,
   (HashAlgoInit) sha224Init,
   (HashAlgoUpdate) sha224Update,
   (HashAlgoFinal) sha224Final
};


/**
 * @brief Digest a message using SHA-224
 * @param[in] data Pointer to the message being hashed
 * @param[in] length Length of the message
 * @param[out] digest Pointer to the calculated digest
 * @return Error code
 **/

error_t sha224Compute(const void *data, size_t length, uint8_t *digest)
{
   //Allocate a memory buffer to hold the SHA-224 context
   Sha224Context *context = osMemAlloc(sizeof(Sha224Context));
   //Failed to allocate memory?
   if(!context) return ERROR_OUT_OF_MEMORY;

   //Initialize the SHA-224 context
   sha224Init(context);
   //Digest the message
   sha224Update(context, data, length);
   //Finalize the SHA-224 message digest
   sha224Final(context, digest);

   //Free previously allocated memory
   osMemFree(context);
   //Successful processing
   return NO_ERROR;
}


/**
 * @brief Initialize SHA-224 message digest context
 * @param[in] context Pointer to the SHA-224 context to initialize
 **/

void sha224Init(Sha224Context *context)
{
   //Set initial hash value
   context->h[0] = 0xC1059ED8;
   context->h[1] = 0x367CD507;
   context->h[2] = 0x3070DD17;
   context->h[3] = 0xF70E5939;
   context->h[4] = 0xFFC00B31;
   context->h[5] = 0x68581511;
   context->h[6] = 0x64F98FA7;
   context->h[7] = 0xBEFA4FA4;

   //Number of bytes in the buffer
   context->size = 0;
   //Total length of the message
   context->totalSize = 0;
}


/**
 * @brief Update the SHA-224 context with a portion of the message being hashed
 * @param[in] context Pointer to the SHA-224 context
 * @param[in] data Pointer to the buffer being hashed
 * @param[in] length Length of the buffer
 **/

void sha224Update(Sha224Context *context, const void *data, size_t length)
{
   //The function is defined in the exact same manner as SHA-256
   sha256Update(context, data, length);
}


/**
 * @brief Finish the SHA-224 message digest
 * @param[in] context Pointer to the SHA-224 context
 * @param[out] digest Calculated digest (optional parameter)
 **/

void sha224Final(Sha224Context *context, uint8_t *digest)
{
   //The function is defined in the exact same manner as SHA-256
   sha256Final(context, NULL);

   //Copy the resulting digest
   if(digest != NULL)
      memcpy(digest, context->digest, SHA224_DIGEST_SIZE);
}

#endif
