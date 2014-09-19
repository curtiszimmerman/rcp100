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
#include "dns.h"

// read a 16 bit value in network format
static inline uint16_t read_16(unsigned char *ptr) {
	uint16_t retval = *ptr++;
	retval <<= 8;
	retval += *ptr;
	return retval;
}

// write a 16 bit value in network format
static inline void write_16(unsigned char *ptr, uint16_t data) {
	data = htons(data);
	memcpy(ptr, &data, 2);
}


// read a 32 bit value in network format
static inline uint32_t read_32(unsigned char *ptr) {
	uint32_t retval = *ptr++;
	retval <<= 8;
	retval += *ptr++;
	retval <<= 8;
	retval += *ptr++;
	retval <<= 8;
	retval += *ptr;
	return retval;
}

// write a 32 bit value in network format
static inline void write_32(unsigned char *ptr, uint32_t data) {
	data = htonl(data);
	memcpy(ptr, &data, 4);
}


// build a dns packet in pkt, with information from cache and tid
int dnspkt_build(DnsPkt *pkt, DnsCache *dc, uint32_t tid) {
	if (dc->dns_cname) {
		memcpy(pkt, dc->dns_cname, sizeof(DnsPkt));
		pkt->tid = htons(tid);
		return pkt->len;
	}

	// set header
	pkt->tid = htons(tid);
	pkt->flags = htons(dc->dns_flags);
	pkt->queries = htons(1);
	pkt->answers = htons(1);
	pkt->authority = 0;
	pkt->additional = 0;

	// set the query
	unsigned char *ptr = (unsigned char *) pkt + 12;
	int rv = 12;
	// name
	int len = strlen(dc->dns_name) + 1;
	memcpy(ptr, dc->dns_name, len);
	ptr += len;
	// type
	write_16(ptr, dc->type);
	ptr += 2;
	// class
	write_16(ptr, 1);
	ptr += 2;
	rv += len + 4;

	// set the answer
	// name
	write_16(ptr, 0xc00c);
	ptr += 2;
	// type
	write_16(ptr, dc->type);
	ptr += 2;
	// class
	write_16(ptr, 1);
	ptr += 2;
	// ttl
	write_32(ptr, dc->dns_ttl);
	ptr += 4;
	if (dc->type == 1) {
		// data len
		write_16(ptr, 4);
		ptr += 2;
		// address
		write_32(ptr, dc->dns_ip);
		ptr += 4;
		rv += 16;
	}
	else if (dc->type == 28) {
		// data len
		write_16(ptr, 16);
		ptr += 2;
		// address
		memcpy(ptr, dc->dns_ipv6, 16);
		ptr += 16;
		rv += 16 + 12;
	}
	
	return rv;
}


static void mark_len(char *n) {
	char *ptr = n + 1;
	int len = 0;
	while (*ptr != '.' && *ptr != '\0') {
		len++;
		ptr++;
	}
	
	n[0] = len;
	if (*ptr == '\0')
		return;
	mark_len(ptr);
}

// convert a domain name from human format to packet format
const char *dnspkt_build_name(char *ptr) {
	static char n[CLI_MAX_DOMAIN_NAME + 1];
	strncpy(ptr, n + 1, CLI_MAX_DOMAIN_NAME);
	n[CLI_MAX_DOMAIN_NAME] = '\0';
	
	mark_len(n);
	return n;
}

// convert a domain name from packet format to human format
const char *dnspkt_get_real_name(unsigned char *ptr) {
	static char n[CLI_MAX_DOMAIN_NAME + 1];
	
	n[0] = '\0';
	char *out = n;
	int total = 0;	// name length protection
	
	while (*ptr != '\0') {
		int len = *ptr;
		total += len;
		if (total >= CLI_MAX_DOMAIN_NAME)
			return NULL;
		memcpy(out, ptr + 1, len);
		ptr  += len + 1;
		out += len;
		*out++ = '.';
	}
	
	// remove the extra '.'
	out--;
	*out = '\0';
	return n;
}


// check current packet length
// return 1 if error
static inline int chk_pkt_len(unsigned char *ptr, DnsPkt *pkt, int len) {
	int newlen = (unsigned) (ptr - (unsigned char *) pkt);
	if (newlen > len)
		return 1;
	return 0;
}

// get the length of a dns name from the packet
// return 0 if error
int get_name_len(unsigned char *ptr, DnsPkt *pkt, int len) {
	ASSERT(ptr != NULL);
	ASSERT(pkt != NULL);
	int rv = 0;
	
	while (*ptr != '\0') {
		// test pointer
		if (*ptr & 0xc0) {
			rcpLog(muxsock, RCP_PROC_DNS, RLOG_DEBUG, RLOG_FC_DNS,
				"parser: pointer not implemented");
			return 0;
		}
		else {
			rv += *ptr + 1;
			ptr += *ptr + 1;
			if (chk_pkt_len(ptr, pkt, len))
				return 0;
		}
	}
	rv++; // include '\0'	
	if (rv > CLI_MAX_DOMAIN_NAME) {
		rcpLog(muxsock, RCP_PROC_DNS, RLOG_DEBUG, RLOG_FC_DNS,
			"parser: name too long");
		return 0;
	}
	return rv;
}


static inline void dnspkt_update_RR_stats(DnsPkt *pkt) {
	switch (pkt->dns_type) {
		case 1:
			shm->stats.dns_a_answers++;
			break;
		case 5:
			shm->stats.dns_cname_answers++;
			break;
		case 28:
			shm->stats.dns_aaaa_answers++;
			break;
		case 12:
			shm->stats.dns_ptr_answers++;
			break;
		case 15:
			shm->stats.dns_mx_answers++;
		default:
printf("unsupported DNS RR type %u\n", pkt->dns_type);
	}		
}


static int parse_cnames(DnsPkt *pkt, int len, int acount,int *ttl) {
	if (len <= 12)
		return 1;

	*ttl = DNS_MAX_TTL;
	unsigned char *ptr = (unsigned char *) pkt;
	ptr += 12; // jump over the header
	int ptrlen = 12;

	// jump over the query
	{
		// process char
		int name_len = get_name_len(ptr, pkt, len);
		ptr += name_len + 4;
		ptrlen += name_len + 4;
		if (ptrlen > len) {
			return 1;
		}
	}
	
	int i;
	for (i = 0; i < acount; i++) {
		// over here we should have a pointer
		if ((*ptr & 0xc0) != 0xc0) {
			return 0;
		}		
		ptr += 2;

		// get type and class		
		uint16_t type = read_16(ptr);
		ptr += 2;
		uint16_t dclass = read_16(ptr);
		ptr += 2;

		if (dclass != 1) {
			return 0;
		}			

		// ttl
		uint16_t ttl_tmp = read_32(ptr);
		if (ttl_tmp < *ttl)
			*ttl = ttl_tmp;
		ptr += 4;


		// check packet length so far
		ptrlen += 10;
		if (ptrlen > len) {
			return 1;
		}

		// read the rest of RR
		if (type == 1) {
			// read address
			uint16_t addr_len = read_16(ptr);
			ptr += 2;
			if (addr_len != 4) {
				return 1;
			}
			
			ptr += 4;
			
			// check packet length so far
			ptrlen += 6;
			if (ptrlen > len) {
				return 1;
			}
		}
		// read the rest of RR
		if (type == 28) {
			// read address
			uint16_t addr_len = read_16(ptr);
			ptr += 2;
			if (addr_len != 16) {
				return 1;
			}
			
			ptr += 16;
			
			// check packet length so far
			ptrlen += 18;
			if (ptrlen > len) {
				return 1;
			}
		}
		else if (type == 5) {
			// read another cname
			uint16_t name2_len = read_16(ptr);
			ptr += 2;
			ptr += name2_len;

			// check packet length so far
			ptrlen += 2 + name2_len;
			if (ptrlen > len) {
				return 1;
			}
		}
	}
	
	return 0;
}


// verify a dns packet and extract the information form the packet
// return 0 if the packet is OK, 1 if it is bad
int dnspkt_verify(DnsPkt *pkt, int len) {
	pkt->len = len;
	pkt->dns_error_code = 0;
	pkt->dns_pkt_type = 0;
	pkt->dns_query_name[0] = '\0';
	pkt->dns_type = 0;
	pkt->dns_class = 0;
	pkt->dns_ttl = 0;
	pkt->dns_address = 0;
	
	if (len < 12)	// at least we should have a header
		return 1;
	
	// extract flags
	uint16_t flags =  ntohs(pkt->flags);
	if (flags & 0x8000)
		pkt->dns_pkt_type = 1;
	if (flags & 0x000f)
		pkt->dns_error_code = flags & 0x000f;
	pkt->dns_flags = flags;
	
	// parse queries
	unsigned char *ptr = (unsigned char *) pkt + 12;
	int qcount = ntohs(pkt->queries);
	if (qcount != 1) {
		rcpLog(muxsock, RCP_PROC_DNS, RLOG_DEBUG, RLOG_FC_DNS,
			"parser: query count %d not implemented", qcount);
		return chk_pkt_len(ptr, pkt, len);
	}
	
	{ // read one query		
		// process char
		int name_len = get_name_len(ptr, pkt, len);
		if (name_len == 0) {
			rcpLog(muxsock, RCP_PROC_DNS, RLOG_DEBUG, RLOG_FC_DNS,
				"parser: query name length problem");
			return chk_pkt_len(ptr, pkt, len);
		}
		memcpy(pkt->dns_query_name, ptr, name_len);
		ptr += name_len;
		pkt->dns_query_type = read_16(ptr);
		ptr += 4;
	}

	// parse answers
	int acount = ntohs(pkt->answers);
	if (acount == 0)
		return chk_pkt_len(ptr, pkt, len);
		
	{ // read one answer
		// accept here only a pointer to the name in query
		if (*ptr == 0xc0 && *(ptr + 1) == 0x0c);
		else {
			rcpLog(muxsock, RCP_PROC_DNS, RLOG_DEBUG, RLOG_FC_DNS,
				"parser: expecting a pointer in answer");
			return chk_pkt_len(ptr, pkt, len);
		}
		ptr += 2;

		// type and class
		pkt->dns_type = read_16(ptr);
		ptr += 2;
		pkt->dns_class = read_16(ptr);
		ptr += 2;


		 if (pkt->dns_class != 1) {
			rcpLog(muxsock, RCP_PROC_DNS, RLOG_DEBUG, RLOG_FC_DNS,
				"parser: class %u not implemented", pkt->dns_class);
			return chk_pkt_len(ptr, pkt, len);
		}

		
		dnspkt_update_RR_stats(pkt);
		
		if (pkt->dns_type == 5) {	// cname
			// parse cnames
			if (parse_cnames(pkt, len, acount, &pkt->dns_ttl))
				return 1;	// packet error
			rcpLog(muxsock, RCP_PROC_DNS, RLOG_DEBUG, RLOG_FC_DNS,
				"parser: CNAME, ttl %d", pkt->dns_ttl);		
			return chk_pkt_len(ptr, pkt, len);
		}
		
		else if (pkt->dns_type == 1) {	// ipv4 address
			// ttl
			pkt->dns_ttl = read_32(ptr);
			ptr += 4;
			
			// read address
			uint16_t addr_len = read_16(ptr);
			ptr += 2;
			if (addr_len != 4) {
				rcpLog(muxsock, RCP_PROC_DNS, RLOG_DEBUG, RLOG_FC_DNS,
					"parser: wrong address length %u for DNS type 1", addr_len);
				return chk_pkt_len(ptr, pkt, len);
			}
			pkt->dns_address = read_32(ptr);
			ptr += 4;
		}
		
		else if (pkt->dns_type == 28) {	// ipv6 address
			// ttl
			pkt->dns_ttl = read_32(ptr);
			ptr += 4;
			
			// read address
			uint16_t addr_len = read_16(ptr);
			ptr += 2;
			if (addr_len != 16) {
				rcpLog(muxsock, RCP_PROC_DNS, RLOG_DEBUG, RLOG_FC_DNS,
					"parser: wrong address length %u for DNS type 28", addr_len);
				return chk_pkt_len(ptr, pkt, len);
			}

			memcpy(pkt->dns_address_v6, ptr, 16);
			ptr += 16;
		}
		
		else {
			rcpLog(muxsock, RCP_PROC_DNS, RLOG_DEBUG, RLOG_FC_DNS,
				"parser: type %u, class %u not implemented", pkt->dns_type, pkt->dns_class);
			return chk_pkt_len(ptr, pkt, len);
		}
	}

	return chk_pkt_len(ptr, pkt, len);
}
