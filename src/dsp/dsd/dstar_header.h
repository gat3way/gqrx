/* This is the header file for dstar_header.c, which is under the GPL. */

#ifndef _DSTAR_HEADER_H
#define _DSTAR_HEADER_H
#include "dsd.h"
void dstar_header_decode(int radioheaderbuffer[660], dsd_state *state);
#endif /* _DSTAR_HEADER_H */