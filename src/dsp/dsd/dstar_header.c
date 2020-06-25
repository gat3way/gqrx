/*
 *
 * This code is taken largely from on1arf's GMSK code. Original copyright below:
 *
 *
* Copyright (C) 2011 by Kristoff Bonne, ON1ARF
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; version 2 of the License.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
 *
 */

#include "fcs.h"
#include "descramble.h"
#include "dstar_header.h"

void dstar_header_decode(int radioheaderbuffer[660], dsd_state *state) {
	int radioheaderbuffer2[660];
	unsigned char radioheader[41];
	int octetcount, bitcount, loop;
	unsigned char bit2octet[] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80};
        char msg[1024];

        msg[0] = 0;

	scramble(radioheaderbuffer, radioheaderbuffer2);
	deinterleave(radioheaderbuffer2, radioheaderbuffer);
	(void)FECdecoder(radioheaderbuffer, radioheaderbuffer2);
	memset(radioheader, 0, 41);
	// note we receive 330 bits, but we only use 328 of them (41 octets)
	// bits 329 and 330 are unused
	octetcount = 0;
	bitcount = 0;
	for (loop = 0; loop < 328; loop++) {
		if (radioheaderbuffer2[loop]) {
			radioheader[octetcount] |= bit2octet[bitcount];
		};
		bitcount++;
		// increase octetcounter and reset bitcounter every 8 bits
		if (bitcount >= 8) {
			octetcount++;
			bitcount = 0;
		}
	}
	// print header
	sprintf(msg, "\nDSTAR HEADER: ");
        strcat(state->msgbuf,msg);
	//printf("FLAG1: %02X - FLAG2: %02X - FLAG3: %02X\n", radioheader[0],
	//		radioheader[1], radioheader[2]);
	sprintf(msg, "RPT 2: %c%c%c%c%c%c%c%c ", radioheader[3], radioheader[4],
			radioheader[5], radioheader[6], radioheader[7], radioheader[8],
			radioheader[9], radioheader[10]);
        strcat(state->msgbuf,msg);
	sprintf(msg, "RPT 1: %c%c%c%c%c%c%c%c ", radioheader[11], radioheader[12],
			radioheader[13], radioheader[14], radioheader[15], radioheader[16],
			radioheader[17], radioheader[18]);
        strcat(state->msgbuf,msg);
	sprintf(msg, "YOUR: %c%c%c%c%c%c%c%c ", radioheader[19], radioheader[20],
			radioheader[21], radioheader[22], radioheader[23], radioheader[24],
			radioheader[25], radioheader[26]);
        strcat(state->msgbuf,msg);
	sprintf(msg,"MY: %c%c%c%c%c%c%c%c/%c%c%c%c\n", radioheader[27],
			radioheader[28], radioheader[29], radioheader[30], radioheader[31],
			radioheader[32], radioheader[33], radioheader[34], radioheader[35],
			radioheader[36], radioheader[37], radioheader[38]);
        strcat(state->msgbuf,msg);
	//FCSinheader = ((radioheader[39] << 8) | radioheader[40]) & 0xFFFF;
	//FCScalculated = calc_fcs((unsigned char*) radioheader, 39);
	//printf("Check sum = %04X ", FCSinheader);
	//if (FCSinheader == FCScalculated) {
	//	printf("(OK)\n");
	//} else {
	//	printf("(NOT OK- Calculated FCS = %04X)\n", FCScalculated);
	//}; // end else - if
}
