/**
 * @file sha512.h
 * @brief SHA-512 (Secure Hash Algorithm 512)
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
 * @author Oryx Embedded (www.oryx-embedded.com)
 * @version 1.3.5
 **/

#ifndef _SHA512_H
#define _SHA512_H

//Dependencies
#include "crypto.h"

//SHA-512 block size
#define SHA512_BLOCK_SIZE 128
//SHA-512 digest size
#define SHA512_DIGEST_SIZE 64
//Common interface for hash algorithms
#define SHA512_HASH_ALGO (&sha512HashAlgo)


/**
 * @brief SHA-512 algorithm context
 **/

typedef struct
{
   union
   {
      uint64_t h[8];
      uint8_t digest[64];
   };
   union
   {
      uint64_t w[80];
      uint8_t buffer[128];
   };
   size_t size;
   uint64_t totalSize;
} Sha512Context;


//SHA-512 related constants
extern const HashAlgo sha512HashAlgo;

//SHA-512 related functions
error_t sha512Compute(const void *data, size_t length, uint8_t *digest);
void sha512Init(Sha512Context *context);
void sha512Update(Sha512Context *context, const void *data, size_t length);
void sha512Final(Sha512Context *context, uint8_t *digest);
void sha512ProcessBlock(Sha512Context *context);

#endif
