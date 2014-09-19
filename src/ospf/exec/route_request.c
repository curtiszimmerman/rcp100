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

//*******************************************
// callbacks
//*******************************************
// set del_flag in every route; this is done at the beginning of SPF cycle
static int callback_rrq(PTN *ptn, void *arg) {
	ASSERT(ptn != NULL);
	
	int i;
	for (i = 0; i < RCP_OSPF_ECMP_LIMIT; i++) {
		if (ptn->rt[i].type == RT_NONE ||
		     ptn->rt[i].type == RT_CONNECTED ||
		     ptn->rt[i].type == RT_OSPF_BLACKHOLE)
			break;
			
		redistribute_route(1, ptn->rt[i].ip, ptn->rt[i].mask, ptn->rt[i].gw);
	}
	
	return 0;

}


void route_request(void) {
	// set delete flag in all routes
	route_walk(callback_rrq, NULL);
}
