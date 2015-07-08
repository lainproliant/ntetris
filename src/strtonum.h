#pragma once
#ifndef __STRTONUM_H
#define __STRTONUM_H

long long
strtonum(const char *numstr, long long minval, long long maxval,
const char **errstrp);

#endif
