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
#ifndef VHOST_H
#define VHOST_H
#include "tinyxml2.h"

extern int debug;
class VHost: public XMLVisitor {
	int interval_;
	int interval_delta_;
	const XMLElement *host_;
	const char *host_name_;
	unsigned char host_port_;
	unsigned char host_type_;
	int tx_;
	int rx_;
	int resp_;
public:
	VHost(int interval, int interval_delta):
		XMLVisitor(), interval_(interval), interval_delta_(interval_delta),
		host_(NULL), host_name_(NULL), host_port_(0), host_type_(0), tx_(0), rx_(0), resp_(0) {}
	~VHost() {}
	
	// Visit a document.
	virtual bool VisitEnter( const XMLDocument& doc) {
		return true;
	}
	// Visit a document.
	virtual bool VisitExit( const XMLDocument& doc ) {
		return true;
	}

	// Visit an element.
	virtual bool VisitEnter( const XMLElement& element, const XMLAttribute* attribute) {
		const char *name = element.Name();
		if (strcmp(name, "monitor") == 0) {
			ASSERT(host_ == NULL);
			ASSERT(host_name_ == NULL);
			
			while (attribute) {
				if (strcmp(attribute->Name(), "hostname") == 0)
					host_name_ = attribute->Value();
				else if (strcmp(attribute->Name(), "port") == 0) {
					unsigned val;
					sscanf(attribute->Value(), "%u", &val);
					host_port_ = (unsigned char) val;
				}
				else if (strcmp(attribute->Name(), "type") == 0) {
					unsigned val;
					sscanf(attribute->Value(), "%u", &val);
					host_type_ = (unsigned char) val;
				}
				
				attribute = attribute->Next();
			}
			if (host_name_)
				host_ = &element;

		}
		else if (strcmp(name, "rx_packets") == 0 && host_) {
			rx_ = 1;
			tx_ = 0;
			resp_ = 0;
		}
		else if (strcmp(name, "tx_packets") == 0 && host_) {
			tx_ = 1;
			rx_ = 0;
			resp_ = 0;
		}
		else if (strcmp(name, "response_time") == 0 && host_) {
			tx_ = 0;
			rx_ = 0;
			resp_ = 1;
		}
		if (debug)
			printf("\n\nhost_name_ %s, rx_ %d, tx_ %d, resp_ %d\n", host_name_, rx_, tx_, resp_);
		return true;
	}

	// Visit an element.
	virtual bool VisitExit( const XMLElement& element) {
		if (&element == host_) {
			host_ = NULL;
			host_name_ = NULL;
			host_port_ = 0;
			host_type_ = 0;
			tx_ = 0;
			rx_ = 0;
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
		RcpNetmonHost *host = NULL;
		if (host_ && host_name_) {
			for (int i = 0; i < RCP_NETMON_LIMIT; i++) {
				RcpNetmonHost *ptr = &shm->config.netmon_host[i];
				if (!ptr->valid)
					continue;
				if (strcmp(host_name_, ptr->hostname) == 0 &&
				    host_port_ == ptr->port &&
				    host_type_ == ptr->type) {
				    	host = ptr;
				    	break;
				}
			}
		}
		
		if (host && rx_)
			target_ptr = host->rx;
		else if (host && tx_)
			target_ptr = host->tx;
		else if (host && resp_)
			target_ptr = host->resptime;

		if (target_ptr) {
			if (debug)
				printf("loading monitor %s %s, interval %d, delta interval %d\n",
					host_name_, (rx_)? "rx": (tx_)? "tx" : "resp", interval_, interval_delta_);
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
