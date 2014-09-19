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
#ifndef OSPF_H
#define OSPF_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <signal.h>
#include <sys/times.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <net/ethernet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netpacket/packet.h>
#include <netinet/ip.h>
#include "librcp.h"

// global debug
//#define TRACE_FUNCTION() printf("%s\n", __FUNCTION__);
#define TRACE_FUNCTION() ;
static inline void PRINT_EVENT(void) {
	time_t pt = time(NULL);
	printf("\n%s", ctime(&pt));
}
// global debug end

// protocol globals
#define IP_PROT_OSPF 89
#define AllSPFRouters_GROUP  "224.0.0.5"
#define AllSPFRouters_GROUP_IP 0xe0000005
#define AllDRouters_GROUP  "224.0.0.6"
#define AllDRouters_GROUP_IP 0xe0000006
#define MaxAge 3600	// 60 minutes
#define MaxAgeDiff 900	// 15 minutes
#define LSRefreshTime 1800 // 30 minutes
#define CheckAge 300	// 5 minutes

#define SPF_DELAY 5
#define OSPF_MAX_COST 0xffffffff
				
#include "packet.h"
#include "area.h"

// process statistics
extern uint32_t systic_delta_pkt;
extern uint32_t systic_spf_delta;
extern uint32_t systic_delta_lsa;
extern uint32_t systic_delta_network;
extern uint32_t systic_delta_integrity;
extern uint32_t route_add_cnt;
extern uint32_t route_del_cnt;
extern uint32_t route_replaced_cnt;


// cheksum.c
int validate_checksum(uint16_t *ptr, int size);
uint16_t calculate_checksum(uint16_t *ptr, int size);

// tx.c
void txpacket(OspfPacket *pkt, OspfNetwork *net);
void txpacket_unicast(OspfPacket *pkt, OspfNetwork *net, uint32_t ip);

// rx.c
void rxpacket(int sock);
// connect OSPF socket; returns socket number or 0 if error
int rxConnect(void);
// join multicast group for all networks
void rxMultiJoinAll(int sock);
void rxMultiJoinNetwork(int sock, OspfNetwork *net);
void rxMultiDropNetwork(int sock, OspfNetwork *net);

// join router group; returns 1 if error, 0 if ok
int rxMultiJoin(int sock, uint32_t ifaddress, uint32_t group);
// leaves router group; returns 1 if error, 0 if ok
int rxMultiDrop(int sock, uint32_t ifaddress, uint32_t group);

// header.c
// extract header data; return 1 if error, 0 if ok
int headerExtract(OspfPacket *pkt, OspfPacketDesc *desc);
void headerBuild(OspfPacket *pkt, OspfNetwork *net, uint8_t type, int length);

// hello.c
int helloRx(OspfPacket *pkt, OspfPacketDesc *desc);
void helloBuildPkt(OspfNetwork *net, OspfPacket *pkt);

// dbdesc.c
int dbdescRx(OspfPacket *pkt, OspfPacketDesc *desc);
void dbdescBuildPkt(OspfNetwork *net, OspfNeighbor *nb, OspfPacket *pkt, uint8_t fields);

// lsrequest.c
int lsrequestRx(OspfPacket *pkt, OspfPacketDesc *desc);
void lsrequestBuildPkt(OspfNetwork *net, OspfNeighbor *nb, OspfPacket *pkt);

// lsupdate.c
int lsupdateRx(OspfPacket *pkt, OspfPacketDesc *desc);
// return 0 if packet build, 1 if error
int lsupdateBuildPkt(OspfNetwork *net, OspfPacket *pkt, OspfLsa *lsa);
// return 0 if packet build, or the last lsa not sent
OspfLsa *lsupdateBuildMultiplePkt(OspfNetwork *net, OspfPacket *pkt, OspfLsa *lsa, int trap);

// lsack.c
int lsackRx(OspfPacket *pkt, OspfPacketDesc *desc);
void lsackBuildPkt(OspfNetwork *net, OspfNeighbor *nb, OspfPacket *pkt, OspfLsaHeader *lsa, int cnt);

// lsa.c
OspfLsa *lsadbGetList(uint32_t area_id, uint8_t type);
OspfLsa **lsadbGetListHead(uint32_t area_id, uint8_t type);
void lsadbAdd(uint32_t area_id, OspfLsa *lsa);
OspfLsa *lsadbFind(uint32_t area_id, uint8_t type, uint32_t link_state_id, uint32_t adv_router);
OspfLsa *lsadbFind2(uint32_t area_id, uint8_t type, uint32_t link_state_id);
OspfLsa *lsadbFindHeader(uint32_t area_id, OspfLsaHeader *lsa);
OspfLsa *lsadbFindRequest(uint32_t area_id, OspfLsRequest *lsrq);
void lsadbReplace(uint32_t area_id, OspfLsa *oldlsa, OspfLsa *newlsa);
void lsadbRemove(uint32_t area_id, OspfLsa *lsa);
void lsadbTimeout(void);
void lsadbRemoveArea(uint32_t area_id);
void lsadbAgeArea(uint32_t area_id);
void lsadbFlood(OspfLsa *lsa, uint32_t area_id);
void lsadbAgeRemoveNeighbor(uint32_t nb_router_id, uint32_t area_id);
void lsadbAgeRemoveNeighborNetwork(uint32_t nb_router_id, OspfNetwork *net);

// lsa_originate_*.c
OspfLsa *lsa_originate_rtr(uint32_t area_id);
void lsa_originate_rtr_update(void);
OspfLsa *lsa_originate_net(OspfNetwork *net);
void lsa_originate_net_update(void);
OspfLsa *lsa_originate_ext(uint32_t ip, uint32_t mask, uint32_t gw, int type, uint32_t metric, uint32_t tag);
OspfLsa *lsa_originate_sumdefault(uint32_t area_id, uint32_t cost);
void lsa_originate_summary_update(void);

// nexthop.c
void nexthop_external(uint32_t area_id);
void nexthop_summary_net(uint32_t area_id);
void nexthop_summary_router(uint32_t area_id);
void nexthop(uint32_t area_id);

// checksum.c
int validate_checksum(uint16_t *ptr, int size);
uint16_t calculate_checksum(uint16_t *ptr, int size);
uint16_t fletcher16(uint8_t *msg, int mlen, int coffset);
void testfletcher(void);

// spf.c
extern uint32_t spf_calculations;
void djikstra_result(uint32_t area_id);
void init_costs(uint32_t area_id);
void spfTrigger(void);
void spfTriggerNow(void);
void spfTimeout(void);

// show.c
void showOspfDatabase(FILE *fp);
void showOspfDatabaseDetail(FILE *fp);
void showOspfAreaDatabase(FILE *fp, uint32_t area_id);
void showOspfAreaDatabaseDetail(FILE *fp, uint32_t area_id);

void showOspfDatabaseNetwork(FILE *fp);
void showOspfDatabaseNetworkDetail(FILE *fp);
void showOspfAreaDatabaseNetwork(FILE *fp, uint32_t area_id);
void showOspfAreaDatabaseNetworkDetail(FILE *fp, uint32_t area_id);

void showOspfDatabaseRouter(FILE *fp);
void showOspfDatabaseRouterDetail(FILE *fp);
void showOspfAreaDatabaseRouter(FILE *fp, uint32_t area_id);
void showOspfAreaDatabaseRouterDetail(FILE *fp, uint32_t area_id);

void showOspfDatabaseSummary(FILE *fp);
void showOspfDatabaseSummaryDetail(FILE *fp);
void showOspfAreaDatabaseSummary(FILE *fp, uint32_t area_id);
void showOspfAreaDatabaseSummaryDetail(FILE *fp, uint32_t area_id);

void showOspfDatabaseAsbrSummary(FILE *fp);
void showOspfDatabaseAsbrSummaryDetail(FILE *fp);
void showOspfAreaDatabaseAsbrSummary(FILE *fp, uint32_t area_id);
void showOspfAreaDatabaseAsbrSummaryDetail(FILE *fp, uint32_t area_id);

void showOspfDatabaseSelfOriginateDetail(FILE *fp);
void showOspfAreaDatabaseSelfOriginateDetail(FILE *fp, uint32_t area_id);

void showOspfDatabaseExternal(FILE *fp);
void showOspfDatabaseExternalDetail(FILE *fp);
void showOspfDatabaseExternalBrief(FILE *fp);
void showNeighbors(FILE *fp);
void showRoutes(FILE *fp);
void showRoutesEcmp(FILE *fp);
void showBorder(FILE *fp);

// lsa_pqueue.c
void lsapq_push(OspfLsa *lsa);
OspfLsa *lsapq_get(void);

// route_update.c
void update_rt_start(void);
void update_rt_finish(void);
void update_rt_print(void);
void update_rt_connected(uint32_t area_id);
void update_rt_external(uint32_t area_id);
void update_rt_summary(uint32_t area_id);

// drbdr.c
void drbdrElection(OspfNetwork *net);

// main.c
extern int rxsock;
extern int force_shutdown;
extern int request_rip;

// rcp_init.c
extern RcpShm *shm;
extern int muxsock;
extern RcpProcStats *pstats;
void rcp_init(uint32_t process);

// callbacks.c
extern int cli_html;
extern int is_abr;
extern int dbg_pkt;
extern int dbg_drbdr;
extern int dbg_lsa;
extern int dbg_spf;
int processCli(RcpPkt *pkt);
OspfArea *createOspfArea(uint32_t area_id);

// cmds.c
int module_initialize_commands(CliNode *head);

// shm.c
RcpOspfArea *get_shm_area(uint32_t area_id);
RcpOspfArea *get_shm_area_empty(void);
RcpOspfNetwork *get_shm_network(uint32_t area_id, uint32_t ip, uint32_t mask);
RcpOspfNetwork *get_shm_any_network(uint32_t ip, uint32_t mask);
RcpOspfNetwork *shm_match_network(uint32_t ip, uint32_t mask);
RcpOspfNetwork *get_shm_network_empty(uint32_t area_id);
void generate_router_id(void);

// interface.c
void ospf_update_interfaces(void);
void delete_vlan_network(uint32_t ip, uint32_t mask);

// redist.c
extern int is_asbr;
void redistStaticAdd(uint32_t ip, uint32_t mask, uint32_t gw, void *interface);
void redistStaticDel(uint32_t ip, uint32_t mask, uint32_t gw);
void redistStaticUpdate(void);
void redistConnectedDel(uint32_t ip, uint32_t mask, uint32_t gw);
void redistConnectedUpdate(void);
void redistConnectedSubnetsUpdate(void);
void redistTimeout(void);
void redistSummaryAdd(uint32_t ip, uint32_t mask);
void redistSummaryNotAdvertise(uint32_t ip, uint32_t mask);
void redistSummaryDel(uint32_t ip, uint32_t mask);
void redistClearShm(void);
void redistRipAdd(uint32_t ip, uint32_t mask, uint32_t gw, void *interface);
void redistRipDel(uint32_t ip, uint32_t mask, uint32_t gw);
void redistDeleteAllRip(void);
void rip_route_request(void);

// restart.c
void restart(void);

// summary_address.c
RcpOspfSummaryAddr *summaddr_match(uint32_t ip, uint32_t mask);
void summaddr_update(void);

// range.c
int range_is_configured(void);
RcpOspfRange *range_match(uint32_t area, uint32_t ip, uint32_t mask);

// route request.c
void route_request(void);

// trap.c
void trap_init(void);
void trap_NbrStateChange(OspfNeighbor *nbr);
void trap_IfAuthFailure(uint32_t ifindex, uint32_t source_ip, int mismatch, uint8_t pkt_type);
void trap_IfConfigError(uint32_t ifindex, uint32_t source_ip, int cfg_type, uint8_t pkt_type);
void trap_IfRxBadPacket(uint32_t ifindex, uint32_t source_ip, uint8_t pkt_type);
void trap_IfStateChange(OspfNetwork *net);
void trap_set_retransmit(OspfNetwork *net, OspfNeighbor *nb, int pkt_type);
void trap_TxRetransmit(OspfLsa *lsa);
void trap_MaxAgeLsa(OspfLsa *lsa, uint32_t area_id);
void trap_OriginateLsa(OspfLsa *lsa, uint32_t area_id);

#endif
