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

//A.3.1 The OSPF packet header
//
//    Every OSPF packet starts with a standard 24	byte header.  This
//    header contains all	the information	necessary to determine whether
//    the	packet should be accepted for further processing.  This
//    determination is described in Section 8.2 of the specification.
//
//
//	0		    1			2		    3
//	0 1 2 3	4 5 6 7	8 9 0 1	2 3 4 5	6 7 8 9	0 1 2 3	4 5 6 7	8 9 0 1
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       |   Version #   |     Type      |	 Packet	length	       |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       |			  Router ID			       |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       |			   Area	ID			       |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       |	   Checksum	       |	     AuType	       |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       |		       Authentication			       |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       |		       Authentication			       |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
//
//
//    Version #
//	The OSPF version number.  This specification documents version 2
//	of the protocol.
//
//    Type
//	The OSPF packet	types are as follows. See Sections A.3.2 through
//	A.3.6 for details.
//
//			  Type	 Description
//			  ________________________________
//			  1	 Hello
//			  2	 Database Description
//			  3	 Link State Request
//			  4	 Link State Update
//			  5	 Link State Acknowledgment
//
//    Packet length
//	The length of the OSPF protocol	packet in bytes.  This length
//	includes the standard OSPF header.
//
//    Router ID
//	The Router ID of the packet's source.
//
//    Area ID
//	A 32 bit number	identifying the	area that this packet belongs
//	to.  All OSPF packets are associated with a single area.  Most
//	travel a single	hop only.  Packets traveling over a virtual
//	link are labeled with the backbone Area ID of 0.0.0.0.
//
//    Checksum
//	The standard IP	checksum of the	entire contents	of the packet,
//	starting with the OSPF packet header but excluding the 64-bit
//	authentication field.  This checksum is	calculated as the 16-bit
//	one's complement of the	one's complement sum of	all the	16-bit
//	words in the packet, excepting the authentication field.  If the
//	packet's length	is not an integral number of 16-bit words, the
//	packet is padded with a	byte of	zero before checksumming.  The
//	checksum is considered to be part of the packet	authentication
//	procedure; for some authentication types the checksum
//	calculation is omitted.
//
//    AuType
//	Identifies the authentication procedure	to be used for the
//	packet.	 Authentication	is discussed in	Appendix D of the
//	specification.	Consult	Appendix D for a list of the currently
//	defined	authentication types.
//
//    Authentication
//	A 64-bit field for use by the authentication scheme. See
//	Appendix D for details.
//

// extract header data; return 1 if error, 0 if ok
int headerExtract(OspfPacket *pkt, OspfPacketDesc *desc) {
	TRACE_FUNCTION();
	ASSERT(pkt != NULL);

//	Next, the OSPF packet header is	verified.  The fields specified
//	in the header must match those configured for the receiving
//	interface.  If they do not, the	packet should be discarded:
//
//	o   The	version	number field must specify protocol version 2.
	if (pkt->header.version != 2) {
		trap_IfConfigError(desc->if_index, desc->ip_source, 1 /*badVersion*/, pkt->header.type);
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT, "invalid header version");
		return 1;
	}

//	o   The	Area ID	found in the OSPF header must be verified.  If
//	    both of the	following cases	fail, the packet should	be
//	    discarded.	The Area ID specified in the header must either:
	desc->area_id = ntohl(pkt->header.area);
//
//	    (1)	Match the Area ID of the receiving interface.  In this
//		case, the packet has been sent over a single hop.
//		Therefore, the packet's	IP source address is required to
//		be on the same network as the receiving	interface.  This
//		can be verified	by comparing the packet's IP source
//		address	to the interface's IP address, after masking
//		both addresses with the	interface mask.
	if (areaFind(desc->area_id) == NULL) {
		trap_IfConfigError(desc->if_index, desc->ip_source, 2 /*areaMismatch*/, pkt->header.type);
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT, "invalid area id");
		return 1;
	}
	
	if (networkFindIp(desc->area_id, desc->ip_source) == NULL) {
		trap_IfConfigError(desc->if_index, desc->ip_source, 2 /*areaMismatch*/, pkt->header.type);
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT, "invalid network");
		return 1;
	}
//	                                                                                              This comparison
//		should not be performed	on point-to-point networks. On
//		point-to-point networks, the interface addresses of each
//		end of the link	are assigned independently, if they are
//		assigned at all. - todo


//	    (2)	Indicate the backbone.	In this	case, the packet has
//		been sent over a virtual link.	The receiving router
//		must be	an area	border router, and the Router ID
//		specified in the packet	(the source router) must be the
//		other end of a configured virtual link.	 The receiving
//		interface must also attach to the virtual link's
//		configured Transit area.  If all of these checks
//		succeed, the packet is accepted	and is from now	on
//		associated with	the virtual link (and the backbone
//		area). - todo


//	o   Packets whose IP destination is AllDRouters	should only be
//	    accepted if	the state of the receiving interface is	DR or
//	    Backup (see	Section	9.1). - todo

//	o   The	AuType specified in the	packet must match the AuType
//	    specified for the associated area. - already done

//	o   The	packet must be authenticated.  The authentication
//	    procedure is indicated by the setting of AuType (see
//	    Appendix D).  The authentication procedure may use one or
//	    more Authentication	keys, which can	be configured on a per-
//	    interface basis.  The authentication procedure may also
//	    verify the checksum	field in the OSPF packet header	(which,
//	    when used, is set to the standard IP 16-bit	one's complement
//	    checksum of	the OSPF packet's contents after excluding the
//	    64-bit authentication field).  If the authentication
//	    procedure fails, the packet	should be discarded. - todo

	desc->router_id = ntohl(pkt->header.router_id);
	return 0;
}

// build header
void headerBuild(OspfPacket *pkt, OspfNetwork *net, uint8_t type, int length) {
	TRACE_FUNCTION();
	ASSERT(pkt != NULL);
	ASSERT(net != NULL);
	memset(&pkt->header, 0, sizeof(OspfHeader));
	
	pkt->header.version = 2;
	pkt->header.type = type;
	pkt->header.router_id = htonl(net->router_id);
	pkt->header.area = htonl(net->area_id);
	pkt->header.length = htons(length);
	pkt->header.auth_type = htons(net->auth_type);
	if (net->auth_type != 2)
		pkt->header.checksum = calculate_checksum((uint16_t *) &pkt->header, length);
	else
		pkt->header.checksum = 0;
	
	// add authentication
	auth_tx(pkt, net);
}
