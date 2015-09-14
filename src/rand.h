#pragma once
#ifndef __RAND_H
#define __RAND_H

#include <stdio.h>
#include <glib.h>

extern FILE *randFile;
uint32_t getRand();
uint16_t getRandShort();
uint8_t getRandByte();
uint8_t *getRandBytes(size_t numBytes);
uint32_t genRandId(GHashTable *byId);

#endif
