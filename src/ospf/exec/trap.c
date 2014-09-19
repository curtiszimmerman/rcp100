#include "ospf.h"

// RFC4750
// - implementing some of the optional traps; window of 10 seconds with 7 notifications pre window
static time_t start_time = 0;	//ospf start time - wait for 2 dead intervals after startup before enabling traps
static time_t wtime = 0;	// current window start time
#define WINDOW 10
static int wcount = 0;	// current window count
#define MAX_WCOUNT 7
static char msg[2048];


void trap_init(void) {
	start_time = time(NULL);
}

static int cannot_send(void) {
	time_t now = time(NULL);
	if ((now - start_time) < (RCP_OSPF_IF_DEAD_DEFAULT * 2))
		return 1;

	if (wcount == 0 || (now - wtime) > WINDOW) {
		wtime = now;
		wcount = 1;
		return 0;
	}
	
	++wcount;
	if (wcount >= MAX_WCOUNT)
		return 1;
	return 0;
}

#if 0
 ospfNbrStateChange NOTIFICATION-TYPE .1.3.6.1.2.1.14.16.2.2 
       OBJECTS {
          ospfRouterId (IpAddress) .1.3.6.1.2.1.14.1.1 
          ospfNbrIpAddr (IpAddress) .1.3.6.1.2.1.14.10.1.1
          ospfNbrAddressLessIndex (INTEGER 0) .1.3.6.1.2.1.14.10.1.2
          ospfNbrRtrId, (IpAddress) .1.3.6.1.2.1.14.10.1.3
          ospfNbrState  (INTEGER down (1), attempt (2), init (3), twoWay (4), exchangeStart (5), exchange (6), loading (7), full (8))
          			.1.3.6.1.2.1.14.10.1.6 
       }
#endif

void trap_NbrStateChange(OspfNeighbor *nbr) {
	// if traps disabled do nothing
	if (!shm->config.snmp.traps_enabled)
		return;
	if (cannot_send())
		return;
		
	char rtr_id[20];
	sprintf(rtr_id, "%d.%d.%d.%d", RCP_PRINT_IP(shm->config.ospf_router_id));

	char nb_rtr_id[20];
	sprintf(nb_rtr_id, "%d.%d.%d.%d", RCP_PRINT_IP(nbr->router_id));

	char nb_ip[20];
	sprintf(nb_ip, "%d.%d.%d.%d", RCP_PRINT_IP(nbr->ip));

	sprintf(msg, ".1.3.6.1.2.1.14.16.2.2 " 	//ospfNbrStateChange
		".1.3.6.1.2.1.14.1.1 a %s " 	//ospfRouterId
		".1.3.6.1.2.1.14.10.1.1 a %s " //ospfNbrIpAddr
		".1.3.6.1.2.1.14.10.1.2 i 0 "	//ospfNbrAddressLessIndex
		".1.3.6.1.2.1.14.10.1.3 a %s "	//ospfNbrRtrId
		".1.3.6.1.2.1.14.10.1.6 i %d",
		
		rtr_id,
		nb_ip,
		nb_rtr_id,
		nbr->state + 1);
	
	// send trap
	rcpSnmpSendTrap(msg);
}


#if 0
  ospfIfAuthFailure NOTIFICATION-TYPE .1.3.6.1.2.1.14.16.2.6 
       OBJECTS {
          ospfRouterId (IpAddress) .1.3.6.1.2.1.14.1.1 
          ospfIfIpAddress (IpAddress) .1.3.6.1.2.1.14.7.1.1
          ospfAddressLessIf (INTEGER 0) .1.3.6.1.2.1.14.7.1.2
          ospfPacketSrc (IpAddress) 1.3.6.1.2.1.14.16.1.4.0
          ospfConfigErrorType (INTEGER) .1.3.6.1.2.1.14.16.1.2.0, -- authTypeMismatch or authFailure
                     badVersion (1),
                       areaMismatch (2),
                       unknownNbmaNbr (3), -- Router is DR eligible
                       unknownVirtualNbr (4),
                       authTypeMismatch(5),
                       authFailure (6),
                       netMaskMismatch (7),
                       helloIntervalMismatch (8),
                       deadIntervalMismatch (9),
                       optionMismatch (10),
                       mtuMismatch (11),
                       duplicateRouterId (12),
                       noError (13) }
          ospfPacketType (INTEGER) .1.3.6.1.2.1.14.16.1.3.0
                       hello (1),
                       dbDescript (2),
                       lsReq (3),
                       lsUpdate (4),
                       lsAck (5),
                       nullPacket (6)
          }
#endif

void trap_IfAuthFailure(uint32_t ifindex, uint32_t source_ip, int mismatch, uint8_t pkt_type) {
	// if traps disabled do nothing
	if (!shm->config.snmp.traps_enabled)
		return;
	if (cannot_send())
		return;

	// find the interface address based on the kernel interface index
	RcpInterface *intf = rcpFindInterfaceByKIndex(shm, ifindex);
	if (intf == NULL) {
		ASSERT(0);
		return;
	}

	char rtr_id[20];
	sprintf(rtr_id, "%d.%d.%d.%d", RCP_PRINT_IP(shm->config.ospf_router_id));

	char ifip[20];
	sprintf(ifip, "%d.%d.%d.%d", RCP_PRINT_IP(intf->ip));

	char sip[20];
	sprintf(sip, "%d.%d.%d.%d", RCP_PRINT_IP(source_ip));
	
	int type = (mismatch)? 5: 6;
	
	sprintf(msg, ".1.3.6.1.2.1.14.16.2.6 " 	//ospfNbrStateChange
		".1.3.6.1.2.1.14.1.1 a %s " 	//ospfRouterId
		".1.3.6.1.2.1.14.7.1.1 a %s " //ospfIfIpAddress
		".1.3.6.1.2.1.14.7.1.2 i 0 "	//ospfAddressLessIf
		".1.3.6.1.2.1.14.16.1.4.0 a %s "	//ospfPacketSrc
		".1.3.6.1.2.1.14.16.1.2.0 i %d "	//ospfConfigErrorType
		".1.3.6.1.2.1.14.16.1.3.0 i %d",	//ospfPacketType		
		rtr_id,
		ifip,
		sip,
		type,
		pkt_type);
	
	// send trap
	rcpSnmpSendTrap(msg);
}

#if 0
  ospfIfConfigError NOTIFICATION-TYPE .1.3.6.1.2.1.14.16.2.4
       OBJECTS {

          ospfRouterId (IpAddress) .1.3.6.1.2.1.14.1.1 
          ospfIfIpAddress (IpAddress) .1.3.6.1.2.1.14.7.1.1
          ospfAddressLessIf (INTEGER 0) .1.3.6.1.2.1.14.7.1.2
          ospfPacketSrc (IpAddress) 1.3.6.1.2.1.14.16.1.4.0
          ospfConfigErrorType (INTEGER) .1.3.6.1.2.1.14.16.1.2.0
                     badVersion (1),
                       areaMismatch (2),
                       unknownNbmaNbr (3), -- Router is DR eligible
                       unknownVirtualNbr (4),
                       authTypeMismatch(5),
                       authFailure (6),
                       netMaskMismatch (7),
                       helloIntervalMismatch (8),
                       deadIntervalMismatch (9),
                       optionMismatch (10),
                       mtuMismatch (11),
                       duplicateRouterId (12),
                       noError (13) }
          ospfPacketType (INTEGER) .1.3.6.1.2.1.14.16.1.3.0
                       hello (1),
                       dbDescript (2),
                       lsReq (3),
                       lsUpdate (4),
                       lsAck (5),
                       nullPacket (6)
          }
 #endif

void trap_IfConfigError(uint32_t ifindex, uint32_t source_ip, int cfg_type, uint8_t pkt_type) {
	// if traps disabled do nothing
	if (!shm->config.snmp.traps_enabled)
		return;
	if (cannot_send())
		return;

	// find the interface address based on the kernel interface index
	RcpInterface *intf = rcpFindInterfaceByKIndex(shm, ifindex);
	if (intf == NULL) {
		ASSERT(0);
		return;
	}

	char rtr_id[20];
	sprintf(rtr_id, "%d.%d.%d.%d", RCP_PRINT_IP(shm->config.ospf_router_id));

	char ifip[20];
	sprintf(ifip, "%d.%d.%d.%d", RCP_PRINT_IP(intf->ip));

	char sip[20];
	sprintf(sip, "%d.%d.%d.%d", RCP_PRINT_IP(source_ip));
	
	sprintf(msg, ".1.3.6.1.2.1.14.16.2.4 " 	//ospfIfConfigError
		".1.3.6.1.2.1.14.1.1 a %s " 	//ospfRouterId
		".1.3.6.1.2.1.14.7.1.1 a %s " //ospfIfIpAddress
		".1.3.6.1.2.1.14.7.1.2 i 0 "	//ospfAddressLessIf
		".1.3.6.1.2.1.14.16.1.4.0 a %s "	//ospfPacketSrc
		".1.3.6.1.2.1.14.16.1.2.0 i %d "	//ospfConfigErrorType
		".1.3.6.1.2.1.14.16.1.3.0 i %d",	//ospfPacketType		
		rtr_id,
		ifip,
		sip,
		cfg_type,
		pkt_type);
	
	// send trap
	rcpSnmpSendTrap(msg);
}
 
 #if 0
   ospfIfRxBadPacket NOTIFICATION-TYPE .1.3.6.1.2.1.14.16.2.8
       OBJECTS { 
          ospfRouterId (IpAddress) .1.3.6.1.2.1.14.1.1 
          ospfIfIpAddress (IpAddress) .1.3.6.1.2.1.14.7.1.1
          ospfAddressLessIf (INTEGER 0) .1.3.6.1.2.1.14.7.1.2
          ospfPacketSrc (IpAddress) 1.3.6.1.2.1.14.16.1.4.0
          ospfPacketType (INTEGER) .1.3.6.1.2.1.14.16.1.3.0
                       hello (1),
                       dbDescript (2),
                       lsReq (3),
                       lsUpdate (4),
                       lsAck (5),
                       nullPacket (6)
          }
#endif
void trap_IfRxBadPacket(uint32_t ifindex, uint32_t source_ip, uint8_t pkt_type) {
	// if traps disabled do nothing
	if (!shm->config.snmp.traps_enabled)
		return;
	if (cannot_send())
		return;

	// find the interface address based on the kernel interface index
	RcpInterface *intf = rcpFindInterfaceByKIndex(shm, ifindex);
	if (intf == NULL) {
		ASSERT(0);
		return;
	}

	char rtr_id[20];
	sprintf(rtr_id, "%d.%d.%d.%d", RCP_PRINT_IP(shm->config.ospf_router_id));

	char ifip[20];
	sprintf(ifip, "%d.%d.%d.%d", RCP_PRINT_IP(intf->ip));

	char sip[20];
	sprintf(sip, "%d.%d.%d.%d", RCP_PRINT_IP(source_ip));
	
	sprintf(msg, ".1.3.6.1.2.1.14.16.2.8 " 	//ospfIfConfigError
		".1.3.6.1.2.1.14.1.1 a %s " 	//ospfRouterId
		".1.3.6.1.2.1.14.7.1.1 a %s " //ospfIfIpAddress
		".1.3.6.1.2.1.14.7.1.2 i 0 "	//ospfAddressLessIf
		".1.3.6.1.2.1.14.16.1.4.0 a %s "	//ospfPacketSrc
		".1.3.6.1.2.1.14.16.1.3.0 i %d",	//ospfPacketType		
		rtr_id,
		ifip,
		sip,
		pkt_type);
	
	// send trap
	rcpSnmpSendTrap(msg);
}

#if 0
  ospfIfStateChange NOTIFICATION-TYPE .1.3.6.1.2.1.14.16.2.16
       OBJECTS {
          ospfRouterId (IpAddress) .1.3.6.1.2.1.14.1.1 
          ospfIfIpAddress (IpAddress) .1.3.6.1.2.1.14.7.1.1
          ospfAddressLessIf (INTEGER 0) .1.3.6.1.2.1.14.7.1.2
          ospfIfState   -- The new state .1.3.6.1.2.1.14.7.1.12
                      down (1),
                       loopback (2),
                       waiting (3),
                       pointToPoint (4),
                       designatedRouter (5),
                       backupDesignatedRouter (6),
                       otherDesignatedRouter (7)
          }
#endif

void trap_IfStateChange(OspfNetwork *net) {
	// if traps disabled do nothing
	if (!shm->config.snmp.traps_enabled)
		return;
	if (cannot_send())
		return;

	char rtr_id[20];
	sprintf(rtr_id, "%d.%d.%d.%d", RCP_PRINT_IP(shm->config.ospf_router_id));

	char ifip[20];
	sprintf(ifip, "%d.%d.%d.%d", RCP_PRINT_IP(net->ip));

	sprintf(msg, ".1.3.6.1.2.1.14.16.2.16 " 	//ospfIfStateChange
		".1.3.6.1.2.1.14.1.1 a %s " 	//ospfRouterId
		".1.3.6.1.2.1.14.7.1.1 a %s " //ospfIfIpAddress
		".1.3.6.1.2.1.14.7.1.2 i 0 "	//ospfAddressLessIf
		".1.3.6.1.2.1.14.7.1.12 i %d",	//ospfIfState
		rtr_id,
		ifip,
		net->state + 1);
	
	// send trap
	rcpSnmpSendTrap(msg);
}


#if 0
   ospfTxRetransmit NOTIFICATION-TYPE .1.3.6.1.2.1.14.16.2.10
       OBJECTS {
          ospfRouterId (IpAddress) .1.3.6.1.2.1.14.1.1 
          ospfIfIpAddress (IpAddress) .1.3.6.1.2.1.14.7.1.1
          ospfAddressLessIf (INTEGER 0) .1.3.6.1.2.1.14.7.1.2
          ospfNbrRtrId, (IpAddress) -- Destination .1.3.6.1.2.1.14.10.1.3
          ospfPacketType (INTEGER) .1.3.6.1.2.1.14.16.1.3.0
                       hello (1),
                       dbDescript (2),
                       lsReq (3),
                       lsUpdate (4),
                       lsAck (5),
                       nullPacket (6)
           ospfLsdbType, (INTEGER) .1.3.6.1.2.1.14.4.1.2
                       routerLink (1),
               	       networkLink (2),
                       summaryLink (3),
               	       asSummaryLink (4),
                       asExternalLink (5), -- but see ospfAsLsdbTable
               	       multicastLink (6),
                       nssaExternalLink (7),
               	       areaOpaqueLink (10)
           ospfLsdbLsid,  (IpAddress) .1.3.6.1.2.1.14.4.1.3
           ospfLsdbRouterId, (IpAddress) .1.3.6.1.2.1.14.4.1.4
          }
 #endif
 
static OspfNetwork *net_retransmit = NULL;
static OspfNeighbor *nb_retransmit = NULL;
static int pkt_type_retransmit = 0;

void trap_set_retransmit(OspfNetwork *net, OspfNeighbor *nb, int pkt_type) {
 	net_retransmit = net;
 	nb_retransmit = nb;
 	pkt_type_retransmit = pkt_type;
 }
 
void trap_TxRetransmit(OspfLsa *lsa) {
	ASSERT(lsa);

	// if traps disabled do nothing
	if (!shm->config.snmp.traps_enabled)
		return;
	if (cannot_send())
		return;
	if (!net_retransmit || !nb_retransmit || !pkt_type_retransmit) {
		ASSERT(0);
		return;
	}

	char rtr_id[20];
	sprintf(rtr_id, "%d.%d.%d.%d", RCP_PRINT_IP(shm->config.ospf_router_id));

	char ifip[20];
	sprintf(ifip, "%d.%d.%d.%d", RCP_PRINT_IP(net_retransmit->ip));

	char nb_rtr_id[20];
	sprintf(nb_rtr_id, "%d.%d.%d.%d", RCP_PRINT_IP(nb_retransmit->router_id));

	char lsid[20];
	sprintf(lsid, "%d.%d.%d.%d", RCP_PRINT_IP(ntohl(lsa->header.link_state_id)));

	char ls_rtr_id[20];
	sprintf(ls_rtr_id, "%d.%d.%d.%d", RCP_PRINT_IP(ntohl(lsa->header.adv_router)));
	
	char ls_area_id[20];
	sprintf(ls_area_id, "%d.%d.%d.%d", RCP_PRINT_IP(net_retransmit->area_id));

#if 0
INDEX { ospfLsdbAreaId, ospfLsdbType,
               ospfLsdbLsid, ospfLsdbRouterId }	
#endif
	sprintf(msg, ".1.3.6.1.2.1.14.16.2.10 " 	//ospfTxRetransmit
		".1.3.6.1.2.1.14.1.1 a %s " 	//ospfRouterId
		".1.3.6.1.2.1.14.7.1.1 a %s " //ospfIfIpAddress
		".1.3.6.1.2.1.14.7.1.2 i 0 "	//ospfAddressLessIf
		".1.3.6.1.2.1.14.10.1.3 a %s " 	//ospfNbrRtrId
		".1.3.6.1.2.1.14.16.1.3.0 i %d " //ospfPacketType
		".1.3.6.1.2.1.14.4.1.2.%s.%d.%s.%s i %d " //ospfLsdbType
           		".1.3.6.1.2.1.14.4.1.3.%s.%d.%s.%s a %s " //ospfLsdbLsid
           		".1.3.6.1.2.1.14.4.1.4.%s.%d.%s.%s a %s", //ospfLsdbRouterId
		rtr_id,
		ifip,
		nb_rtr_id,
		pkt_type_retransmit,
		ls_area_id, lsa->header.type, lsid, ls_rtr_id, lsa->header.type,
		ls_area_id, lsa->header.type, lsid, ls_rtr_id, lsid,
		ls_area_id, lsa->header.type, lsid, ls_rtr_id, ls_rtr_id);
	
	// send trap
	rcpSnmpSendTrap(msg);
}

 
#if 0 
 ospfMaxAgeLsa NOTIFICATION-TYPE .1.3.6.1.2.1.14.16.2.13
       OBJECTS {
          ospfRouterId (IpAddress) .1.3.6.1.2.1.14.1.1 
          ospfLsdbAreaId,  (IpAddress) -- 0.0.0.0 for AS Externals .1.3.6.1.2.1.14.4.1.1
           ospfLsdbType, (INTEGER) .1.3.6.1.2.1.14.4.1.2
                       routerLink (1),
               	       networkLink (2),
                       summaryLink (3),
               	       asSummaryLink (4),
                       asExternalLink (5), -- but see ospfAsLsdbTable
               	       multicastLink (6),
                       nssaExternalLink (7),
               	       areaOpaqueLink (10)
           ospfLsdbLsid,  (IpAddress) .1.3.6.1.2.1.14.4.1.3
           ospfLsdbRouterId, (IpAddress) .1.3.6.1.2.1.14.4.1.4
          }
#endif
void trap_MaxAgeLsa(OspfLsa *lsa, uint32_t area_id) {
	ASSERT(lsa);

	// if traps disabled do nothing
	if (!shm->config.snmp.traps_enabled)
		return;
	if (cannot_send())
		return;

	char rtr_id[20];
	sprintf(rtr_id, "%d.%d.%d.%d", RCP_PRINT_IP(shm->config.ospf_router_id));

	char ls_area_id[20];
	sprintf(ls_area_id, "%d.%d.%d.%d", RCP_PRINT_IP(area_id));

	char lsid[20];
	sprintf(lsid, "%d.%d.%d.%d", RCP_PRINT_IP(ntohl(lsa->header.link_state_id)));

	char ls_rtr_id[20];
	sprintf(ls_rtr_id, "%d.%d.%d.%d", RCP_PRINT_IP(ntohl(lsa->header.adv_router)));
	
	sprintf(msg, ".1.3.6.1.2.1.14.16.2.13 " 	//ospfMaxAgeLsa
		".1.3.6.1.2.1.14.1.1 a %s " 	//ospfRouterId
		".1.3.6.1.2.1.14.4.1.1.%s.%d.%s.%s a %s " //ospfLsdbAreaId
		".1.3.6.1.2.1.14.4.1.2.%s.%d.%s.%s i %d " //ospfLsdbType
           		".1.3.6.1.2.1.14.4.1.3.%s.%d.%s.%s a %s " //ospfLsdbLsid
           		".1.3.6.1.2.1.14.4.1.4.%s.%d.%s.%s a %s", //ospfLsdbRouterId
		rtr_id,
		ls_area_id, lsa->header.type, lsid, ls_rtr_id, ls_area_id,
		ls_area_id, lsa->header.type, lsid, ls_rtr_id, lsa->header.type,
		ls_area_id, lsa->header.type, lsid, ls_rtr_id, lsid,
		ls_area_id, lsa->header.type, lsid, ls_rtr_id, ls_rtr_id);
	
	// send trap
	rcpSnmpSendTrap(msg);
}

#if 0          
  ospfOriginateLsa NOTIFICATION-TYPE .1.3.6.1.2.1.14.16.2.12 
       OBJECTS {
          ospfRouterId (IpAddress) .1.3.6.1.2.1.14.1.1 
          ospfLsdbAreaId,  (IpAddress) -- 0.0.0.0 for AS Externals .1.3.6.1.2.1.14.4.1.1
            ospfLsdbType, (INTEGER) .1.3.6.1.2.1.14.4.1.2
                       routerLink (1),
               	       networkLink (2),
                       summaryLink (3),
               	       asSummaryLink (4),
                       asExternalLink (5), -- but see ospfAsLsdbTable
               	       multicastLink (6),
                       nssaExternalLink (7),
               	       areaOpaqueLink (10)
           ospfLsdbLsid,  (IpAddress) .1.3.6.1.2.1.14.4.1.3
           ospfLsdbRouterId, (IpAddress) .1.3.6.1.2.1.14.4.1.4
          }
#endif
void trap_OriginateLsa(OspfLsa *lsa, uint32_t area_id) {
	ASSERT(lsa);

	// if traps disabled do nothing
	if (!shm->config.snmp.traps_enabled)
		return;
	if (cannot_send())
		return;

	char rtr_id[20];
	sprintf(rtr_id, "%d.%d.%d.%d", RCP_PRINT_IP(shm->config.ospf_router_id));
	
	char ls_area_id[20];
	sprintf(ls_area_id, "%d.%d.%d.%d", RCP_PRINT_IP(area_id));

	char lsid[20];
	sprintf(lsid, "%d.%d.%d.%d", RCP_PRINT_IP(ntohl(lsa->header.link_state_id)));

	char ls_rtr_id[20];
	sprintf(ls_rtr_id, "%d.%d.%d.%d", RCP_PRINT_IP(ntohl(lsa->header.adv_router)));
	
	sprintf(msg, ".1.3.6.1.2.1.14.16.2.12 " 	//ospfOriginateLsa
		".1.3.6.1.2.1.14.1.1 a %s " 	//ospfRouterId
		".1.3.6.1.2.1.14.4.1.1.%s.%d.%s.%s a %s " //ospfLsdbAreaId
		".1.3.6.1.2.1.14.4.1.2.%s.%d.%s.%s i %d " //ospfLsdbType
           		".1.3.6.1.2.1.14.4.1.3.%s.%d.%s.%s a %s " //ospfLsdbLsid
           		".1.3.6.1.2.1.14.4.1.4.%s.%d.%s.%s a %s", //ospfLsdbRouterId
		rtr_id,
		ls_area_id, lsa->header.type, lsid, ls_rtr_id, ls_area_id,
		ls_area_id, lsa->header.type, lsid, ls_rtr_id, lsa->header.type,
		ls_area_id, lsa->header.type, lsid, ls_rtr_id, lsid,
		ls_area_id, lsa->header.type, lsid, ls_rtr_id, ls_rtr_id);
	
	// send trap
	rcpSnmpSendTrap(msg);
}
