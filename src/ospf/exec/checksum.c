/*
 * Copyright (C) 2012-2013 RCP100 Team (rcpteam@yahoo.com)
 *
 * This file is part of RCP100 project
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
#include "ospf.h"

// validate an existing checksum
// ptr - pointer to start of data
// size - size of data in bytes
int validate_checksum(uint16_t *ptr, int size) {
	TRACE_FUNCTION();
	uint32_t sum = 0;
	int odd = 0;
	if(size & 1)
		odd = 1;
	int i;

	for (i = 0; i < size / 2; i++, ptr++)
		sum += *ptr;
	if(odd)
		sum += *((uint8_t *)ptr) << 8;

	while (sum >> 16)
		sum = (sum & 0xffff) + (sum >> 16);

	if (sum != 0xffff)
		return 0;
	return 1;				  // checksum ok
}


// calculate checksum
// ptr - pointer to start of data
// size - size of data in bytes
uint16_t calculate_checksum(uint16_t *ptr, int size) {
	TRACE_FUNCTION();
	uint32_t sum = 0;
	int odd = 0;
	if(size & 1)
		odd = 1;
	int i;

	for (i = 0; i < size / 2; i++, ptr++)
		sum += *ptr;
	if(odd)
		sum += *((uint8_t *)ptr) << 8;

	while (sum >> 16)
		sum = (sum & 0xffff) + (sum >> 16);

	return ~sum;
}


//        12.1.7.  LS checksum
//
//            This field is the checksum of the complete contents of the
//            LSA, excepting the LS age field.  The LS age field is
//            excepted so that an LSA's age can be incremented without
//            updating the checksum.  The checksum used is the same that
//            is used for ISO connectionless datagrams; it is commonly
//            referred to as the Fletcher checksum.  It is documented in
//            Annex B of [Ref6].  The LSA header also contains the length
//            of the LSA in bytes; subtracting the size of the LS age
//            field (two bytes) yields the amount of data to checksum.
//
//            The checksum is used to detect data corruption of an LSA.
//            This corruption can occur while an LSA is being flooded, or
//            while it is being held in a router's memory.  The LS
//            checksum field cannot take on the value of zero; the
//            occurrence of such a value should be considered a checksum
//            failure.  In other words, calculation of the checksum is not
//            optional.
//
//            The checksum of an LSA is verified in two cases:  a) when it
//            is received in a Link State Update Packet and b) at times
//            during the aging of the link state database.  The detection
//            of a checksum failure leads to separate actions in each
//            case.  See Sections 13 and 14 for more details.
//
//            Whenever the LS sequence number field indicates that two
//            instances of an LSA are the same, the LS checksum field is
//            examined.  If there is a difference, the instance with the
//            larger LS checksum is considered to be most recent.[13] See
//            Section 13.1 for more details.

uint16_t fletcher16(uint8_t *msg, int mlen, int coffset) {
	// Set the 16 bit checksum to zero
	if (coffset) {
		msg[coffset-1] = 0;
		msg[coffset] = 0;
	}

	// Initialize checksum fields
	int c0 = 0;
	int c1 = 0;
	uint8_t *ptr = msg;
	uint8_t *end = msg + mlen;

	// Accumulate checksum
	while (ptr < end) {
		uint8_t *stop;
		stop = ptr + 4102;
		if (stop > end)
			stop = end;
		for (; ptr < stop; ptr++) {
			c0 += *ptr;
			c1 += c0;
		}
		// Ones complement arithmetic
		c0 = c0 % 255;
		c1 = c1 % 255;
	}

	// concatenate checksum fields
	uint16_t cksum = (c1 << 8) + c0;

	// Calculate and insert checksum field
	if (coffset) {
		int o = ((mlen - coffset)*c0 - c1) % 255;
		if (o <= 0)
			o += 255;
		msg[coffset-1] = o;

		o = (510 - c0 - o);
		if (o > 255)
			o -= 255;
		msg[coffset] = o;
	}

	return(cksum);
}

#if 0
static void print_msg(uint8_t *ptr, int len) {
	int i;
	for (i = 0; i < len; i++, ptr++)
		printf("%02x ", *ptr);
	printf("\n");
}

void testfletcher(void) {
	// msg is a router lsa of length 36
	uint8_t msg[] = {
		0, 1,
		2, 1, 0xa, 0, 0, 0xc, 0xa, 0,    0, 0xc, 0x80, 0, 0, 2, 0x83, 0x97,
		0, 0x24, 0, 0, 0, 1, 0xa, 0,     0, 0, 0xff, 0xff, 0, 0, 3, 0,
		0, 1
	};
	// age field is not included in checksum calculation
	print_msg(msg + 2, 34);
	// the checksum is 0x83, 0x97

	//rx - use an offset of 0
	uint16_t rv = fletcher16(msg + 2, 34, 0);
	printf("rv = %x\n", rv);

	// tx: use an offset of 15
	// remove checksum first
	msg[16] = 0;
	msg[17] = 0;
	print_msg(msg + 2, 34);
	fletcher16(msg + 2, 34, 15);
	print_msg(msg + 2, 34); // checksum field should be back in the message

	//rx
	rv = fletcher16(msg + 2, 34, 0);
	printf("rv = %x\n", rv);
}
#endif
