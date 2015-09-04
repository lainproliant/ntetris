#pragma once
#ifndef __RAND_H
#define __RAND_H

#include "macros.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

extern FILE *randFile; 
uint32_t getRand();

#endif
