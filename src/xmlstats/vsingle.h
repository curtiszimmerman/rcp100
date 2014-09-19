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
#ifndef VSINGLE_H
#define VSINGLE_H
#include "tinyxml2.h"

extern int debug;
class VSingle: public XMLVisitor {
	int active_;
	int interval_;
	int interval_delta_;
	const char *target_name_;
	unsigned *target_ptr_;
public:
	VSingle(int interval, int interval_delta, const char *target_name, unsigned *target_ptr):
		XMLVisitor(), active_(0), interval_(interval), interval_delta_(interval_delta),
		target_name_(target_name), target_ptr_(target_ptr) {}
	~VSingle() {}
	
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
		if (strcmp(name, target_name_) == 0)
			active_ = 1;
		return true;
	}
	// Visit an element.
	virtual bool VisitExit( const XMLElement& element) {
		active_ = 0;
		return true;
	}

	// Visit a declaration.
	virtual bool Visit( const XMLDeclaration& declaration) {
		return true;
	}
	
	// Visit a text node.
	virtual bool Visit( const XMLText& text) {
		if (active_) {
			if (debug)
				printf("\nloading %s, interval %d, interval delta %d\n",
					target_name_, interval_, interval_delta_);
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
					printf("(%d %u/%u) ", i, val, target_ptr_[i]);
				target_ptr_[i] = val;
				
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

			// break the walk
			return false;
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
