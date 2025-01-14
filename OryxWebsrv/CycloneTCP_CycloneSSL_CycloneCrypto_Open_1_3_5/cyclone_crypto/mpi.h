/**
 * @file mpi.h
 * @brief MPI (Multiple Precision Integer Arithmetic)
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

#ifndef _MPI_H
#define _MPI_H

//Dependencies
#include <stdio.h>
#include "crypto.h"

//Size of the sub data type
#define MPI_INT_SIZE sizeof(uint_t)

//Check macro
#define MPI_CHECK(f) if((error = f) != NO_ERROR) goto end

//Miscellaneous macros
#define mpiIsEven(a) !mpiGetBitValue(a, 0)
#define mpiIsOdd(a) mpiGetBitValue(a, 0)


/**
 * @brief Arbitrary precision integer
 **/

typedef struct
{
   int_t sign;
   uint_t size;
   uint_t *data;
} Mpi;


//MPI related functions
void mpiInit(Mpi *x);
void mpiFree(Mpi *x);

error_t mpiGrow(Mpi *x, uint_t size);

uint_t mpiGetLength(const Mpi *a);
uint_t mpiGetByteLength(const Mpi *a);
uint_t mpiGetBitLength(const Mpi *a);

error_t mpiSetBitValue(Mpi *x, uint_t index, uint_t value);
uint_t mpiGetBitValue(const Mpi *a, uint_t index);

int_t mpiComp(const Mpi *a, const Mpi *b);
int_t mpiCompInt(const Mpi *a, int_t b);
int_t mpiCompAbs(const Mpi *a, const Mpi *b);

error_t mpiCopy(Mpi *x, const Mpi *a);
error_t mpiSetValue(Mpi *a, int_t b);

error_t mpiRand(Mpi *x, uint_t length, const PrngAlgo *prngAlgo, void *prngContext);

error_t mpiReadRaw(Mpi *x, const uint8_t *data, uint_t length);
error_t mpiWriteRaw(const Mpi *a, uint8_t *data, uint_t length);

error_t mpiAdd(Mpi *x, const Mpi *a, const Mpi *b);
error_t mpiAddInt(Mpi *x, const Mpi *a, int_t b);

error_t mpiSub(Mpi *x, const Mpi *a, const Mpi *b);
error_t mpiSubInt(Mpi *x, const Mpi *a, int_t b);

error_t mpiAddAbs(Mpi *x, const Mpi *a, const Mpi *b);
error_t mpiSubAbs(Mpi *x, const Mpi *a, const Mpi *b);

error_t mpiShiftLeft(Mpi *x, uint_t n);
error_t mpiShiftRight(Mpi *x, uint_t n);

error_t mpiMul(Mpi *x, const Mpi *a, const Mpi *b);
error_t mpiMulInt(Mpi *x, const Mpi *a, int_t b);

error_t mpiDiv(Mpi *x, Mpi *y, const Mpi *a, const Mpi *b);
error_t mpiDivInt(Mpi *x, Mpi *y, const Mpi *a, int_t b);

error_t mpiMod(Mpi *x, const Mpi *a, const Mpi *b);
error_t mpiMulMod(Mpi *x, const Mpi *a, const Mpi *b, const Mpi *p);
error_t mpiInvMod(Mpi *x, const Mpi *a, const Mpi *p);
error_t mpiExpMod(Mpi *x, const Mpi *a, const Mpi *e, const Mpi *p);

error_t mpiMontgomeryMul(Mpi *x, const Mpi *a, const Mpi *b, uint_t k, const Mpi *p);
error_t mpiMontgomeryRed(Mpi *x, uint_t k, const Mpi *p);

void mpiDump(FILE *stream, const char_t *prepend, const Mpi *a);

#endif
