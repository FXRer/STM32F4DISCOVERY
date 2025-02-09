/**
 * @file ripemd128.h
 * @brief RIPEMD-128 hash function
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

#ifndef _RIPEMD128_H
#define _RIPEMD128_H

//Dependencies
#include "crypto.h"

//RIPEMD-128 block size
#define RIPEMD128_BLOCK_SIZE 64
//RIPEMD-128 digest size
#define RIPEMD128_DIGEST_SIZE 16
//Common interface for hash algorithms
#define RIPEMD128_HASH_ALGO (&ripemd128HashAlgo)


/**
 * @brief RIPEMD-128 algorithm context
 **/

typedef struct
{
   union
   {
      uint32_t h[4];
      uint8_t digest[16];
   };
   union
   {
      uint32_t x[16];
      uint8_t buffer[64];
   };
   size_t size;
   uint64_t totalSize;
} Ripemd128Context;


//RIPEMD-128 related constants
extern const HashAlgo ripemd128HashAlgo;

//RIPEMD-128 related functions
error_t ripemd128Compute(const void *data, size_t length, uint8_t *digest);
void ripemd128Init(Ripemd128Context *context);
void ripemd128Update(Ripemd128Context *context, const void *data, size_t length);
void ripemd128Final(Ripemd128Context *context, uint8_t *digest);
void ripemd128ProcessBlock(Ripemd128Context *context);

#endif
