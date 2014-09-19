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
#ifndef OSPF_PACKET_H
#define OSPF_PACKET_H

#include <netinet/ip.h>

// implementation globals
#define OSPF_MAX_PAYLOAD (65535 - 64)	// ospf payload under heavy fragmentation				
#define OSPF_DD_MAX  70		// maximum database descriptions in a DD packet
#define OSPF_LS_REQ_MAX 250   // maximum lsa requested in a LS REQ packet
#define OSPF_FLOOD_MAX 32	// maximum lsa flooded as a single packet in one second				
#define OSPF_TIMEOUT_FLOOD_MAX 1000	// maximum lsa flooded in one second	timeout function
#define OSPF_NB_ADJ_MAX	4 // maximum number of neighbors establishing adjacencies			

#include "lsa.h"  // bring in OspfLsaHeader

extern uint8_t last_pkt_type;
extern uint32_t last_pkt_size;

typedef struct ospf_header_t {
	uint8_t version;	// OSPF version
	uint8_t type;	// message type
	uint16_t length;	// packet length
	uint32_t router_id;	// router id
	uint32_t area;	// area
	uint16_t checksum;
	uint16_t auth_type;
	uint8_t auth_data[8];
} OspfHeader;

// md5 structure mapped into auth_data above
typedef struct ospf_md5_t {
	uint16_t null;
	uint8_t key_id;
	uint8_t auth_data_len;
	uint32_t seq;
} OspfMd5;


typedef struct ospf_hello_t {
	uint32_t mask;
	uint16_t hello_interval;
	uint8_t options;
	uint8_t router_priority;
	uint32_t dead_interval;
	uint32_t designated_router;
	uint32_t backup_router;
} OspfHello;

typedef struct ospf_dbd_t { // database description
	uint16_t mtu;
	uint8_t options;
	uint8_t fields;
	uint32_t seq;
} OspfDbd;


typedef struct ospf_ls_request_t {
	uint32_t type;
	uint32_t id;
	uint32_t adv_router;
} OspfLsRequest;

typedef struct ospf_ls_update_t {
	uint32_t count;	// number of lsa
} OspfLsUpdate;

typedef struct ospf_packet_t {
	struct iphdr ip_header;
	OspfHeader header;
	union {
		OspfHello hello;	// type 1, followed by a list of neighbors (uin32_t each)
		OspfDbd dbd;	// type 2, followed by a list of OspfLsaHeader structures
		OspfLsRequest req;	 // type 3, followed by more OspfLsRequest structures
		OspfLsUpdate update; // type 4, followed by a list of lsa (OspfLsaHeader + data)
				// type 5, empty followed by a list of OspfLsaHeader structures
	} u;	// the structure has 64 bytes up to this point
	
	uint8_t data[OSPF_MAX_PAYLOAD];	// the structure has 65535 bytes up to this point 
					// this is the maximum IP packet size
	uint8_t padding;		// the compiler will include one byte of padding
} OspfPacket;
#define OSPF_PKT_HDR_LEN (sizeof(OspfPacket) - OSPF_MAX_PAYLOAD)


typedef struct ospf_packet_desc_t {
	int rx_size;	// received size

	// extracted data
	uint32_t ip_source;
	uint32_t ip_dest;
	unsigned if_index;
	
	// header
	uint32_t router_id;
	uint32_t area_id;
} OspfPacketDesc;


#endif
