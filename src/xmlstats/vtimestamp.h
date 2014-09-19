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
#ifndef VTIMESTAMP_H
#define VTIMESTAMP_H
#include "tinyxml2.h"

class VTimestamp: public XMLVisitor {
	unsigned timestamp;
public:
	VTimestamp(): XMLVisitor(), timestamp(0) {}
	~VTimestamp() {}
	
	unsigned getTimestamp() {
		return timestamp;
	}
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
		if (strcmp(name, "xmlstats") == 0) {
			while (attribute) {
				if (strcmp(attribute->Name(), "timestamp") == 0) {
					sscanf(attribute->Value(), "%u", &timestamp);
					// break the walk
					return false;
				}
				attribute = attribute->Next();
			}
		}
		return true;
	}
	// Visit an element.
	virtual bool VisitExit( const XMLElement& element) {
		return true;
	}

	// Visit a declaration.
	virtual bool Visit( const XMLDeclaration& declaration) {
		return true;
	}
	
	// Visit a text node.
	virtual bool Visit( const XMLText& text) {
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
