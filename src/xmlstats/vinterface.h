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
#ifndef VINTERFACE_H
#define VINTERFACE_H
#include "tinyxml2.h"
extern int debug;
class VInterface: public XMLVisitor {
	int interval_;
	int interval_delta_;
	const XMLElement *if_;
	const char *ifname_;
	int rx_;
	int tx_;
	
public:
	VInterface(int interval, int interval_delta):
		XMLVisitor(), interval_(interval), interval_delta_(interval_delta),
			if_(NULL), ifname_(NULL), rx_(0), tx_(0) {}
	~VInterface() {}
	
	// Visit a document.
	virtual bool VisitEnter( const XMLDocument& doc) {
		return true;
	}
	// Visit a document.
	virtual bool VisitExit( const XMLDocument& doc ) {
		return true;
	}

	// Visit an element.
	virtual bool VisitEnter(const XMLElement& element, const XMLAttribute* attribute) {
		const char *name = element.Name();
		if (strcmp(name, "interface") == 0) {
			ASSERT(if_ == NULL);
			ASSERT(ifname_ == NULL);
			
			while (attribute) {
				if (strcmp(attribute->Name(), "name") == 0)
					ifname_ = attribute->Value();
				attribute = attribute->Next();
			}
			if (ifname_)
				if_ = &element;
		}
		else if (strcmp(name, "rx_kbps") == 0 && ifname_) {
			rx_ = 1;
			tx_ = 0;
		}
		else if (strcmp(name, "tx_kbps") == 0 && ifname_) {
			tx_ = 1;
			rx_ = 0;
		}
		return true;
	}
	// Visit an element.
	virtual bool VisitExit( const XMLElement& element) {
		if (&element == if_) {
			if_ = NULL;
			ifname_ = NULL;
			rx_ = 0;
			tx_ = 0;
		}
		return true;
	}

	// Visit a declaration.
	virtual bool Visit( const XMLDeclaration& declaration) {
		return true;
	}
	
	// Visit a text node.
	virtual bool Visit( const XMLText& text) {
		unsigned *target_ptr = NULL;
		RcpInterface *intf = NULL;
		if (if_ && ifname_)
			intf = rcpFindInterfaceByName(shm, ifname_);
		
		if (intf && rx_)
			target_ptr = intf->stats.rx_kbps;
		else if (intf && tx_)
			target_ptr = intf->stats.tx_kbps;

		if (target_ptr) {
			if (debug)
				printf("\nloading interface %s %s, interval %d, delta interval %d\n",
					intf->name, (rx_)? "rx": "tx", interval_, interval_delta_);
			// skip interval_delta
			int index = interval_ - interval_delta_;
			if (index < 0)
				index = RCP_MAX_STATS + index;
			
			ASSERT(index >= 0);
			ASSERT(index < RCP_MAX_STATS);

			// read values
			const char *ptr = text.Value();
			int i;
			int cnt;
			for (i = index, cnt = 0; cnt < (RCP_MAX_STATS - interval_delta_); i--, cnt++) {
				unsigned val;
				sscanf(ptr, "%u", &val);
				if (debug)
					printf("(%d %u/%u) ", i, val, target_ptr[i]);
				target_ptr[i] = val;
				
				while (*ptr != ',' && *ptr != '\0') 
					ptr++;
				if (*ptr == '\0') {
					ASSERT(0);
					return false;
				}
				ptr++;
				
				if (i <= 0) {
					i = RCP_MAX_STATS - 1;
					i++;	// end of for loop adjustment
				}			
			}
		} // if active			
		return true;
	}
	
	// Visit a comment node.
	virtual bool Visit( const XMLComment& comment) {
		return true;
	}
	
	// Visit an unknown node.
	virtual bool Visit( const XMLUnknown& unknown) {
		return true;
	}

};
#endif
