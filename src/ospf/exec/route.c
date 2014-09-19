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
#include "route.h"

//TODO: handle PTNs with different type of routes


// redistribute route into RIP
void redistribute_route(int add, uint32_t ip, uint32_t mask, uint32_t gw) {
	uint32_t destination = 0;
	
	// send rcp route message to rip
	if (rcpRipRedistOspf())
		destination |= RCP_PROC_RIP;

	// send rcp route message to clients
	if (muxsock && destination) {
		RcpPkt pkt;
		memset(&pkt, 0, sizeof(pkt));
		pkt.destination = destination;
		
		if (!add)
			pkt.type = RCP_PKT_TYPE_DELROUTE;
		else
			pkt.type = RCP_PKT_TYPE_ADDROUTE;

		pkt.pkt.route.type = RCP_ROUTE_OSPF;
		pkt.pkt.route.ip = ip;
		pkt.pkt.route.mask = mask;
		pkt.pkt.route.gw = gw;
		
		errno = 0;
		int n = send(muxsock, &pkt, sizeof(pkt), 0);
		if (n < 1) {
			close(muxsock);
			muxsock = 0;
//			printf("Error: process %s, muxsocket send %d, errno %d, disconnecting...\n",
//				rcpGetProcName(), n, errno);
		}
	}	
}

//****************************************************
// ECMP routes interface to Linux kernel
//****************************************************
void ecmp_del(PTN *ptn) {
	ASSERT(ptn != NULL);
	if (ptn->rt[0].type == RT_CONNECTED || ptn->rt[0].type == RT_STATIC)
		return;
	
	if (ptn->rt[0].type == RT_OSPF_BLACKHOLE) {
		char cmd[100];
		sprintf(cmd, "ip route del blackhole %d.%d.%d.%d/%d protocol %d",
			RCP_PRINT_IP(ptn->rt[0].ip), mask2bits(ptn->rt[0].mask), RCP_OSPF_KERNEL_PROTO);
		int v = system(cmd);
		if (v == -1)
			ASSERT(0);
		// blackhole routes are not redistributed into rip
		return;
	}
		
	// count ecmp routes
	int cnt = 0;
	int i;
	RT *found = NULL;
	for (i = 0; i < RCP_OSPF_ECMP_LIMIT; i++) {
		if (ptn->rt[i].type == RT_NONE)
			break;
		cnt++;
		found = &ptn->rt[i];
	}

	// nothing to delete
	if (cnt == 0)
		return;
	
	// single route
	else if (cnt == 1) {
		ASSERT(found);
		rcpDelRoute(muxsock, found->ip, found->mask, found->gw);
		redistribute_route(0, found->ip, found->mask, found->gw);
		return;
	}
	
	// prefer ospf intra-area over inter-area over e1 over e2
	// add/remove to the routing table only the proffered routes
	uint8_t smallest_type = RT_TYPE_MAX;
	for (i = 0; i < RCP_OSPF_ECMP_LIMIT; i++) {
		if (ptn->rt[i].type == RT_NONE)
			break;
		smallest_type = (ptn->rt[i].type < smallest_type)? ptn->rt[i].type: smallest_type;
	}
	if (smallest_type == RT_TYPE_MAX) {
		ASSERT(0);
		return;
	}
	
	// ecmp routes
	uint32_t gw[cnt];
	int gw_cnt = 0;
	for (i = 0; i < RCP_OSPF_ECMP_LIMIT; i++) {
		if (ptn->rt[i].type == RT_NONE)
			break;
		if (ptn->rt[i].type != smallest_type)
			continue;
			
	     	gw[gw_cnt] = ptn->rt[i].gw;
		redistribute_route(0, found->ip, found->mask, ptn->rt[i].gw);
	     	gw_cnt++;
	}
	rcpDelECMPRoute(muxsock,  smallest_type, found->ip, found->mask, gw_cnt, gw);
}

void ecmp_add(PTN *ptn) {
	ASSERT(ptn != NULL);

	if (ptn->rt[0].type == RT_CONNECTED || ptn->rt[0].type == RT_STATIC)
		return;

	if (ptn->rt[0].type == RT_OSPF_BLACKHOLE) {
		char cmd[100];
		sprintf(cmd, "ip route add blackhole %d.%d.%d.%d/%d protocol %d",
			RCP_PRINT_IP(ptn->rt[0].ip), mask2bits(ptn->rt[0].mask), RCP_OSPF_KERNEL_PROTO);
		int v = system(cmd);
		if (v == -1)
			ASSERT(0);
		// blackhole routes are not redistributed into rip
		return;
	}

	// count ecmp routes
	int cnt = 0;
	int i;
	RT *found = NULL;
	for (i = 0; i < RCP_OSPF_ECMP_LIMIT; i++) {
		if (ptn->rt[i].type == RT_NONE)
			break;
		cnt++;
		found = &ptn->rt[i];
	}

	// nothing to add
	if (cnt == 0)
		return;
	
	// single route
	else if (cnt == 1) {
		ASSERT(found);
		if (found->type == RCP_ROUTE_OSPF_E2) {
			rcpAddRoute(muxsock, found->type, found->ip, found->mask, found->gw, found->e2_cost);
		}
		else {
			rcpAddRoute(muxsock, found->type, found->ip, found->mask, found->gw, found->cost);
		}
		redistribute_route(1, found->ip, found->mask, found->gw);
		return;
	}
	
	// prefer ospf intra-area over inter-area over e1 over e2
	// add/remove to the routing table only the proffered routes
	uint8_t smallest_type = RT_TYPE_MAX;
	for (i = 0; i < RCP_OSPF_ECMP_LIMIT; i++) {
		if (ptn->rt[i].type == RT_NONE)
			break;
		smallest_type = (ptn->rt[i].type < smallest_type)? ptn->rt[i].type: smallest_type;
	}
	if (smallest_type == RT_TYPE_MAX) {
		ASSERT(0);
		return;
	}
	
	// ecmp routes
	uint32_t gw[cnt];
	int gw_cnt = 0;
	for (i = 0; i < RCP_OSPF_ECMP_LIMIT; i++) {
		if (ptn->rt[i].type == RT_NONE)
			break;
		if (ptn->rt[i].type != smallest_type)
			continue;
			
	     	gw[gw_cnt] = ptn->rt[i].gw;
		redistribute_route(1, found->ip, found->mask, ptn->rt[i].gw);
	     	gw_cnt++;
	}

	if ( smallest_type == RCP_ROUTE_OSPF_E2)
		rcpAddECMPRoute(muxsock,  smallest_type, found->ip, found->mask, found->e2_cost, gw_cnt, gw);
	else
		rcpAddECMPRoute(muxsock,  smallest_type, found->ip, found->mask, found->cost, gw_cnt, gw);
}

//****************************************************
// Patricia Tree
//****************************************************
static PTN *ptree_root = NULL;

// check the bit in the key at position specified by len
static inline uint32_t CHECK_BIT(uint32_t key, int len) {
	return (key & (0x80000000 >> (len - 1)))? 1: 0;
}

static inline int MATCH_KEY(uint32_t k1, uint32_t k2, uint32_t mask) {
	return  ((k1 & mask) == (k2 & mask));
}

static inline void *ptn_malloc(size_t size) {
	void *rv = malloc(size);
	return rv;
}

static inline void ptn_free(void *ptr) {
	free(ptr);
}

static void root_init(void) {
	ptree_root = ptn_malloc(sizeof(PTN));
	if (ptree_root == NULL) {
		printf("cannot allocate memory\n");
		exit(1);
	}
	memset(ptree_root, 0, sizeof(PTN));
}

static void update_masklen(uint32_t *mask, int *len) {
	ASSERT(mask);
	ASSERT(len);
	
	if (*mask != 0) {
		*len = mask2bits(*mask);
	}
	else if (len != 0) {
		*mask = 0;
		uint8_t bitmask[] = {0x00, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff};
		int div1 = *len / 8;
		*mask = bitmask[*len % 8];
		*mask <<= 24;
		while (div1 > 0) {
			*mask >>= 8;
			*mask |= 0xff000000;
			div1--;
		}
	}
	else
		ASSERT(0);
}

static void ptn_print(PTN *ptn, FILE *fp) {
	ASSERT(ptn);
	fprintf(fp, "%p(%p/%p, %p): %d.%d.%d.%d/%d, type %d, ip %d.%d.%d.%d/%d, gw %d.%d.%d.%d, cost %d\n",
		ptn, ptn->parent, ptn->link[0], ptn->link[1],
		RCP_PRINT_IP(ptn->key), ptn->len,
		ptn->rt[0].type,
		RCP_PRINT_IP(ptn->rt[0].ip),
		mask2bits(ptn->rt[0].mask),
		RCP_PRINT_IP(ptn->rt[0].gw),
		ptn->rt[0].cost);
}

void ptn_print_all(PTN *ptn, FILE *fp) {
	ASSERT(ptn);
	ptn_print(ptn, fp);
		
	if (ptn->link[0])
		ptn_print_all(ptn->link[0], fp);
		
	if (ptn->link[1])
		ptn_print_all(ptn->link[1], fp);
}


// recursive search
static PTN* ptn_find(PTN *ptn, uint32_t key, int len) {
	ASSERT(ptn);
//printf("%s %d.%d.%d.%d/%d\n", __FUNCTION__, RCP_PRINT_IP(key), len);
//ptn_print(ptn);
	
	if (ptn->key == key && ptn->len == len)
		return ptn;
	
	PTN *n = ptn->link[CHECK_BIT(key, ptn->len + 1)];
	if (n == NULL)
		return NULL;
	if (n->len > len)
		return NULL;
		
	return ptn_find(n, key, len);
}




// recursive search
static PTN* ptn_find_lpm(PTN *ptn, uint32_t key, int len) {
	ASSERT(ptn);
	
	if (ptn->key == key && ptn->len == len)
		return ptn;

	if (ptn->len > len) {
		return ptn->parent;
	}
		
	if (!MATCH_KEY(key, ptn->key, ptn->mask)) {
		return ptn->parent;
	}
	
	PTN *n = ptn->link[CHECK_BIT(key, ptn->len + 1)];
	if (n == NULL) {
		return ptn;
	}
	
	return ptn_find_lpm(n, key, len);
}

static PTN *split(PTN *ptn1, PTN *ptn2) {
	ASSERT(ptn1);
	ASSERT(ptn2);
	
	// create the new node
	PTN *n = ptn_malloc(sizeof(PTN));
	if (n == NULL) {
		printf("cannot allocate memory\n");
		exit(1);
	}
	memset(n, 0, sizeof(PTN));

	// look for the common bits in key
	int len = 1;
	uint32_t mask = 0x80000000;
	for (; len < 32; len++) {
		if ((mask & ptn1->key) != (mask & ptn2->key))
			break;
		mask >>= 1;
		mask |= 0x80000000;
	}
	
	mask <<= 1;
	len--;
	n->len = len;
	n->key = ptn1->key & mask;
	n->mask = mask;
	return n;
}

static void ptn_set_child(PTN *ptn, PTN *child) {
	int bit = CHECK_BIT(child->key, ptn->len + 1);

	
	if (ptn->link[bit] == NULL) {
		// add
		ptn->link[bit] = child;
		child->parent = ptn;
	}
	else {
		// insert
		if (MATCH_KEY(child->key, ptn->link[bit]->key, child->mask)) {
			child->link[bit] = ptn->link[bit];
			child->link[bit]->parent = child;
			ptn->link[bit] = child;
			child->parent = ptn;
		}
		else {
			PTN *existent = ptn->link[bit];
			PTN *n = split(child, existent);
			n->parent = ptn;
			child->parent = n;
			existent->parent = n;
			ptn->link[bit] = n;
			
			// recalculate bit
			bit = CHECK_BIT(existent->key, n->len + 1);
			n->link[bit] = existent;
			bit = (bit)? 0: 1;
			n->link[bit] = child;
			
			
		}
			
	}		
}


static int ptn_walk(PTN *ptn, int (*f)(PTN *, void *), void *arg) {
	ASSERT(ptn);
	ASSERT(f);
	
	if (ptn->rt[0].type != RT_NONE) {
		if (f(ptn, arg))
			return 1;
	}
	
	if (ptn->link[0])
		ptn_walk(ptn->link[0], f, arg);
		
	if (ptn->link[1])
		ptn_walk(ptn->link[1], f, arg);
	return 0;

}

static void ptn_unlink(PTN *ptn) {
	ASSERT(ptn);
	PTN *parent = ptn->parent;
	if (parent == NULL)
		return;
		
	if (parent->link[0] == ptn)
		parent->link[0] = NULL;
	if (parent->link[1] == ptn)
		parent->link[1] = NULL;
}

#if 0 // not used
static void ptn_delete(PTN *ptn) {
	ASSERT(ptn);
	PTN *parent = ptn->parent;
	
	if (ptn->link[0] == NULL && ptn->link[1] == NULL) {
		// ptn_free memory and attempt to delete the parent
		ptn_unlink(ptn);
		ptn_free(ptn);
		if (parent != NULL &&
		     parent->link[0] == NULL && parent->link[1] == NULL && parent->rt.type == RT_NONE)
			ptn_delete(parent);
	}
	else
		// remove route data
		memset(&ptn->rt, 0, sizeof(ptn->rt));
}
#endif

//**************************************************************
// Route functions
//**************************************************************
RT* route_find(uint32_t ip, uint32_t mask, int len) {
	if (ptree_root == NULL)
		return NULL;	// empty tree
	
	update_masklen(&mask, &len);
	
	PTN *ptn = ptn_find(ptree_root, ip, len);
	if (ptn == NULL || ptn->rt[0].type == RT_NONE)
		return NULL;
	return &ptn->rt[0];
}

RT* route_find_lpm(uint32_t ip, uint32_t mask, int len) {
	if (ptree_root == NULL)
		return NULL;	// empty tree
	
	update_masklen(&mask, &len);
	
	PTN *n = ptn_find_lpm(ptree_root, ip, len);
	ASSERT(n != NULL);
	
	if (n->rt[0].type != RT_NONE)
		return &n->rt[0];
	return NULL;
}

#if 0 // not used
void route_delete(uint32_t ip, uint32_t mask, int len) {
	if (ptree_root == NULL)
		return;	// empty tree
	
	update_masklen(&mask, &len);
	
	PTN *ptn = ptn_find(ptree_root, ip, len);
	if (ptn == NULL)
		return;
		
	if (ptn->rt.type != RT_CONNECTED && ptn->rt.type != RT_STATIC)
		rcpDelRoute(muxsock, ptn->rt.ip, ptn->rt.mask, ptn->rt.gw);
	rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_SPF,
		"route %d.%d.%d.%d/%d deleted", RCP_PRINT_IP(ptn->rt.ip), mask2bits(ptn->rt.mask));
	ptn_delete(ptn);		
}

void route_delete_ptn(PTN *ptn)  {
	ASSERT(ptn);
	if (ptn->rt.type != RT_CONNECTED && ptn->rt.type != RT_STATIC)
		rcpDelRoute(muxsock, ptn->rt.ip, ptn->rt.mask, ptn->rt.gw);
	rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_SPF,
		"route %d.%d.%d.%d/%d deleted", RCP_PRINT_IP(ptn->rt.ip), mask2bits(ptn->rt.mask));
	ptn_delete(ptn);		
}
#endif

static uint32_t e2_cost;
RT* route_replace(PTN *ptn, uint32_t ip, uint32_t mask, int len, uint32_t gw, uint32_t cost, uint8_t type, uint32_t area_id, uint32_t ifip) {
	if (ptree_root == NULL)
		 root_init();

	update_masklen(&mask, &len);

	// not yet configured
	if (ptn->rt[0].type == RT_NONE) {
		ptn->rt[0].ip = ip;
		ptn->rt[0].mask = mask;
		ptn->rt[0].gw = gw;
		ptn->rt[0].cost = cost;
		ptn->rt[0].e2_cost = e2_cost;
		ptn->rt[0].type = type;
		ptn->rt[0].del_flag = 0;
		ptn->rt[0].area_id = area_id;
		ptn->rt[0].ifip = ifip;

		// add the route to the kernel
		ecmp_add(ptn);
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_SPF,
			"route %d.%d.%d.%d/%d added", RCP_PRINT_IP(ptn->rt[0].ip), len);
		route_add_cnt++;
		return &ptn->rt[0];
	}
	
	// nothing changed?
	int i;
	int none_visited = 1;
	for (i = 0; i < RCP_OSPF_ECMP_LIMIT; i++) {
		if (ptn->rt[i].type == RT_NONE)
			break;
			
		if (ptn->rt[i].cost == cost && 
		     ptn->rt[i].type == type &&
		     ptn->rt[i].gw == gw) {
			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_SPF,
				"route %d.%d.%d.%d/%d already installed", RCP_PRINT_IP(ptn->rt[i].ip), len);
			ptn->rt[i].del_flag = 0;
			return &ptn->rt[i];
		}
		
		if (ptn->rt[i].del_flag == 0)
			none_visited = 0;
	}

	// if the route was not visited in this spf cyle, replace it regardless the cost
	if (none_visited) {
		// delete route
		ecmp_del(ptn);
		
		// clean route memory
		memset(&ptn->rt[0], 0, RCP_OSPF_ECMP_LIMIT * sizeof(RT));
		
		// set the new route
		ptn->rt[0].cost = cost;
		ptn->rt[0].e2_cost = e2_cost;
		ptn->rt[0].type = type;
		ptn->rt[0].gw = gw;
		ptn->rt[0].ip = ip;
		ptn->rt[0].mask = mask;
		ptn->rt[0].del_flag = 0;
		ptn->rt[0].area_id = area_id;
		ptn->rt[0].ifip = ifip;
		
		// add the route
		ecmp_add(ptn);
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_SPF,
			"route %d.%d.%d.%d/%d replaced", RCP_PRINT_IP(ptn->rt[0].ip), len);
		route_replaced_cnt++;
	}
	// the route was already visited, check the cost
	else if (ptn->rt[0].cost == cost) {
		// is there any room left for another ECMP route?
		for (i = 0; i < RCP_OSPF_ECMP_LIMIT; i++) {
			if (ptn->rt[i].type == RT_NONE)
				break;
		}
		if (i >= RCP_OSPF_ECMP_LIMIT) {
			// ECMP support to come here
			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_SPF,
				"route %d.%d.%d.%d/%d not added, ECMP limit reached", RCP_PRINT_IP(ptn->rt[0].ip), len);
			ptn->retry_ecmp = 1;
			return NULL;
		}

		// delete the old route in the kernel
		ecmp_del(ptn);

		// add the equal cost route
		ptn->rt[i].ip = ip;
		ptn->rt[i].mask = mask;
		ptn->rt[i].cost = cost;
		ptn->rt[i].e2_cost = e2_cost;
		ptn->rt[i].type = type;
		ptn->rt[i].gw = gw;
		ptn->rt[i].del_flag = 0;
		ptn->rt[i].area_id = area_id;
		ptn->rt[i].ifip = ifip;
		
		// add the route back
		ecmp_add(ptn);
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_SPF,
			"route %d.%d.%d.%d/%d added", RCP_PRINT_IP(ptn->rt[i].ip), len);
		route_add_cnt++;
		return &ptn->rt[i];
	}
	// better cost route coming in, replace it
	else if (ptn->rt[0].cost > cost) {
		// delete old route
		ecmp_del(ptn);

		// clear rt memory
		memset(&ptn->rt[0], 0, RCP_OSPF_ECMP_LIMIT * sizeof(RT));
		
		// set new route
		ptn->rt[0].cost = cost;
		ptn->rt[0].e2_cost = e2_cost;
		ptn->rt[0].type = type;
		ptn->rt[0].gw = gw;
		ptn->rt[0].ip = ip;
		ptn->rt[0].mask = mask;
		ptn->rt[0].del_flag = 0;
		ptn->rt[0].area_id = area_id;
		ptn->rt[0].ifip = ifip;
		
		// add new route
		ecmp_add(ptn);
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_SPF,
			"route %d.%d.%d.%d/%d replaced", RCP_PRINT_IP(ptn->rt[0].ip), len);
		route_replaced_cnt++;
	}

	return &ptn->rt[0];
}

RT* route_add_e2(uint32_t ip, uint32_t mask, int len, uint32_t gw, uint32_t cost, uint8_t type, uint32_t e2, uint32_t area_id, uint32_t ifip) {
//rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_SPF,
//printf(
//"add route e2 %d.%d.%d.%d/%d gw %d.%d.%d.%d cost %u, e2 cost %u\n",
//RCP_PRINT_IP(ip), mask2bits(mask),
//RCP_PRINT_IP(gw),
//cost, e2);

	e2_cost = e2;
	return route_add(ip, mask, len, gw, cost, type, area_id, ifip);
}


// only one of mask and len needs to be specified
RT* route_add(uint32_t ip, uint32_t mask, int len, uint32_t gw, uint32_t cost, uint8_t type, uint32_t area_id, uint32_t ifip) {
	ASSERT(type != RT_NONE);
	ASSERT(type < RT_TYPE_MAX);
//rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_SPF,
//printf(
//"add route %d.%d.%d.%d/%d gw %d.%d.%d.%d cost %u\n",
//RCP_PRINT_IP(ip), mask2bits(mask),
//RCP_PRINT_IP(gw),
//cost);

	if (ptree_root == NULL)
		 root_init();

	update_masklen(&mask, &len);
	
	// if the node already exists, add it only if the cost is better
	PTN *ptn = ptn_find(ptree_root, ip, len);
	if (ptn != NULL) {
		return route_replace(ptn, ip, mask, len, gw, cost, type, area_id, ifip);
	}
	
	// create the new node
	PTN *n = ptn_malloc(sizeof(PTN));
	if (n == NULL) {
		printf("cannot allocate memory\n");
		exit(1);
	}
	memset(n, 0, sizeof(PTN));
	// set route
	n->rt[0].ip = ip;
	n->rt[0].mask = mask;
	n->rt[0].gw =gw;
	n->rt[0].cost = cost;
	n->rt[0].e2_cost = e2_cost;
	n->rt[0].type = type;
	n->rt[0].del_flag = 0;
	n->rt[0].area_id = area_id;
	n->rt[0].ifip = ifip;
	// set ptn
	n->key = ip;
	n->len = len;
	n->mask = mask;

	ptn = ptn_find_lpm(ptree_root, ip, len);
	if (ptn == NULL)
		ptn = ptree_root;
	ptn_set_child(ptn, n);
	
	// add the new route
	ecmp_add(n);
	rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_SPF,
		"route %d.%d.%d.%d/%d added", RCP_PRINT_IP(ip), len);

	route_add_cnt++;
	
	return &n->rt[0];
}


void route_print_debug(FILE *fp) {
	if (ptree_root != NULL) {
		fprintf(fp, "node(parent/link0,link1): key/len, data...\n");
		ptn_print_all(ptree_root, fp);
	}
}

int route_walk(int (*f)(PTN *, void *), void *arg) {
	if (ptree_root == NULL)
		return 0;
	return ptn_walk(ptree_root, f, arg);
}


static void ptn_purge(PTN *ptn) {
	ASSERT(ptn);
			
	if (ptn->link[0])
		ptn_purge(ptn->link[0]);
		
	if (ptn->link[1])
		ptn_purge(ptn->link[1]);

	// the root node is never removed
	if (ptn != ptree_root && ptn->link[0] == NULL && ptn->link[1] == NULL && ptn->rt[0].type == RT_NONE) {
		// remove the node
		ptn_unlink(ptn);
		ptn_free(ptn);
	}		
}


// remove unused nodes in the tree
void route_purge(void) {
	if (ptree_root == NULL)
		return;
	
	ptn_purge(ptree_root);
}
