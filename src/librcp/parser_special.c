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
#include "librcp.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>

// return 1 if error
static int chk_existing_file(const char *token, const char* str);
static int chk_tftp_file(const char *token, const char* str);
static int chk_ftp_file(const char *token, const char* str);
static int chk_sftp_file(const char *token, const char* str);
static int chk_file(const char *token, const char *str);
static int chk_administrator(const char *token, const char *str);
static int chk_password(const char *token, const char *str);
static int chk_tun_password(const char *token, const char *str);
static int chk_rip_password(const char *token, const char *str);
static int chk_ospf_password(const char *token, const char *str);
static int chk_ospf_md5_password(const char *token, const char *str);
static int chk_snmp_password(const char *token, const char *str);
static int chk_snmp_community(const char *token, const char *str);
static int chk_hostname(const char *token, const char *str);
static int chk_tunnel_name(const char *token, const char *str);
static int chk_domain_name(const char *token, const char *str);
static int chk_ip(const char *token, const char *str);
static int chk_mac(const char *token, const char *str);
static int chk_int2(const char *token, const char *str);
static void gen_int2(const char *token);
static int chk_uint2(const char *token, const char *str);
static void gen_uint2(const char *token);
static int chk_string2(const char *token, const char *str);
static void generr_string2(const char *token);
static int chk_interface(const char *token, const char *str);
static void generr_interface(const char *token);
static int chk_mask(const char *token, const char *str);
static int chk_cidr(const char *token, const char *str);
static int chk_cidrloopback(const char *token, const char *str);
static int chk_cidrnet(const char *token, const char *str);
static int chk_userathost(const char *token, const char *str);
static int chk_port(const char *token, const char* str);
static int chk_word(const char *token, const char *str);
static int chk_constate(const char *token, const char *str);
static int chk_icmp_type(const char *token, const char *str);

#define GENHELP_STR_LEN 4096
static char genhelpstr[GENHELP_STR_LEN + 1];
static char generrstr[GENHELP_STR_LEN + 1];

typedef struct {
	// parsing
	const char *format;	// stncmp string
	int cnt;	// count for strncmp
	
	// check function
	int (*function)(const char *token, const char *str); // check function
	
	// help
	const char *help;	// left column of help - static
	void (*gen_help)(const char *token);	// left column of help - dynamic
	
	// error
	const char *error_string; // error string - static
	void (*gen_error)(const char *token);	// error string - dynamic
} CHK;

static CHK chk[] = {
// format: {parsing string, parsing cnt, check function, help static, help dynamic, error static, error dynamic}
	// ranges: integer, unsigned, string,
	// word: string without range
	{"<i, numbers here...>", 3, chk_int2, NULL, gen_int2, "invalid value", NULL},
	{"<u, numbers here...>", 3, chk_uint2, NULL, gen_uint2, "invalid value", NULL},
	{"<s, numbers here...>", 3, chk_string2, "WORD", NULL, NULL, generr_string2},
	{ "<word>", 6, chk_word, "WORD", NULL, NULL, NULL},

	// files
	{ "<existing-file>", 15, chk_existing_file, "FILE", NULL, "invalid existing file", NULL},
	{ "<tftp-file>", 11, chk_tftp_file, "TFTP FILE", NULL, "invalid TFTP file", NULL},
	{ "<ftp-file>", 10, chk_ftp_file, "FTP FILE", NULL, "invalid FTP file", NULL},
	{ "<sftp-file>", 11, chk_sftp_file, "SFTP FILE", NULL, "invalid SFTP file", NULL},
	{ "<file>", 6, chk_file, "FILE", NULL, "invalid file", NULL},

	// system
	{"<interface>", 11, chk_interface, "WORD", NULL, NULL, generr_interface},
	{"<interface2>", 11, chk_interface, "WORD", NULL, NULL, generr_interface},
	{ "<hostname>", 10, chk_hostname,"WORD", NULL, "invalid hostname, maximum 16 characters expected", NULL},
	{ "<tunnel-name>", 13, chk_tunnel_name, "WORD", NULL, "invalid tunnel name, maximum 15 characters expected", NULL},
	{ "<domain>", 8, chk_domain_name, "WORD", NULL, "invalid domain name, maximum 63 characters expected", NULL},
	{"<userathost>", 12, chk_userathost, "user@host", NULL, "invalid user@host", NULL},
	{ "<administrator>", 15, chk_administrator, "WORD", NULL, "invalid aministrator, maximum 31 characters expected", NULL},
	
	// passwords
	{ "<password>", 10, chk_password, "WORD", NULL, "invalid password, maximum 128 characters expected", NULL},
	{ "<tun-password>", 14, chk_tun_password,"WORD", NULL, "invalid password, maximum 24 characters expected", NULL},
	{ "<rip-password>", 14, chk_rip_password, "WORD", NULL, "invalid password, maximum 16 characters expected", NULL},
	{ "<ospf-password>", 15, chk_ospf_password, "WORD", NULL, "invalid password, maximum 8 characters expected", NULL},
	{ "<ospf-md5-password>", 19, chk_ospf_md5_password, "WORD", NULL, "invalid password, maximum 16 characters expected", NULL},
	{ "<snmp-password>", 15, chk_snmp_password, "WORD", NULL, "invalid password, 8 to 64 characters expected", NULL},
	{ "<snmp-community>", 16, chk_snmp_community, "WORD", NULL, "invalid password,  maximum 16 characters expected", NULL},
	
	// MAC & IP addresses
	{ "<mac>", 5, chk_mac, "XX:XX:XX:XX:XX:XX", NULL, "invalid MAC address", NULL},
	{ "<ip>", 4, chk_ip, "A.B.C.D", NULL, "invalid IP address", NULL},
	{ "<ip1>", 4, chk_ip, "A.B.C.D", NULL, "invalid IP address", NULL},	// same as <ip>, used to print different help
	{ "<ip2>", 4, chk_ip, "A.B.C.D", NULL, "invalid IP address", NULL},	// same as <ip>, used to print different help
	{"<mask>", 6, chk_mask,"A.B.C.D",  NULL, "invalid IP mask", NULL},
	{ "<ipnexthop>", 11,  chk_ip, "A.B.C.D",NULL, "invalid next hop address", NULL},
	
	// CIDR
	// any address in cidr format - e.g. 1.2.3.4/24 is a valid address (used as interface address)
	{"<cidr>", 6, chk_cidr, "A.B.C.D/0..32", NULL, "invalid CIDR address", NULL},
	// loopback address, it can be a host (/32) address
	{"<cidrloopback>", 6, chk_cidrloopback, "A.B.C.D/0..32", NULL, "invalid loopback address", NULL},
	// network address in cidr format - e.g. 1.2.3.4/24 is invalid
	{"<cidrnet>", 9, chk_cidrnet, "A.B.C.D/0..32", NULL, "invalid network address", NULL},
	// alias
	{"<cidrnet-destination>", 21, chk_cidrnet, "A.B.C.D/0..32", NULL, "invalid network address", NULL},

	// TCP/UDP port number
	{"<port>", 6, chk_port, "1..65535", NULL, "invalid port number", NULL},
	// alias
	{"<port-destination>", 18, chk_port, "1..65535", NULL, "invalid port number", NULL},

	// FIREWALL
	{"<connection-state>", 18, chk_constate, "WORD LIST", NULL, "invalid comma-separated connection states", NULL},
	{"<icmp-type>", 11, chk_icmp_type, "0..255", NULL, "invalid ICMP type number", NULL},
	
	{NULL, 0, NULL, NULL} // null terminated
};

int cliCheckSpecial(const char*token, const char*str, CliParserOp *op) {
	rcpDebug("parser: special token check #%s#, #%s#\n", token, str);
	int i = 0;

	while (chk[i].format != NULL) {
		if (strncmp(token, chk[i].format, chk[i].cnt) == 0) {
			// if str is exactly the same as the token, accept it, it might come from init
			if (strncmp(token, str, chk[i].cnt) == 0) {
				// token matching the format; we might have multiple tokens for the same format
				// i.e. <1,1,99> and <i,100,199>
				if (strcmp(token, str) == 0)
					return 0;
			}		
			
			ASSERT( chk[i].function != NULL);
			int rv = chk[i].function(token, str);
			
			// add the error string if necessary
			if (rv != 0 && chk[i].error_string != NULL)
				op->error_string = chk[i].error_string;
			else if (rv != 0 && chk[i].gen_error != NULL) {
				chk[i].gen_error(token);
				op->error_string = generrstr;
			}
			return rv;
		}
		i++;
	}
	rcpDebug("parser: special token check - not found\n", token, str);
	return RCPERR;
}


const char* cliHelpSpecial(const char*token) {
	int i = 0;
	while (chk[i].format != NULL) {
		if (strncmp(token, chk[i].format, chk[i].cnt) == 0) {
			if (chk[i].gen_help != NULL) {
				chk[i].gen_help(token);
				return genhelpstr;
			}
			else
				return chk[i].help;
		}
		i++;
	}

	return NULL;
}


//***********************************************************
// input sanitization
//***********************************************************
// accepted characters for strings that end up in a system() call
static char ok_chars[] = "abcdefghijklmnopqrstuvwxyz"
                         "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                         "1234567890_-.@";

// return 1 if we have any character in the string not from the list above
int rcpInputNotClean(const char *str) {
	const char *end = str  + strlen(str);
	for (str += strspn(str, ok_chars); str != end; str += strspn(str, ok_chars)) {
		return 1;
	}
	return 0;
}


//***********************************************************
// input check functions
//***********************************************************
static int chk_word(const char *token, const char* str) {
	rcpDebug("parser: special check word\n");
	return 0;
}

static int chk_tun_password(const char *token, const char* str) {
	rcpDebug("parser: special check tunnel password\n");
	if (strlen(str) > CLI_TUN_PASSWD)
		return RCPERR;
	
	return 0;
}

static int chk_rip_password(const char *token, const char* str) {
	rcpDebug("parser: special check rip password\n");
	if (strlen(str) > CLI_RIP_PASSWD)
		return RCPERR;
	
	return 0;
}

static int chk_ospf_password(const char *token, const char* str) {
	rcpDebug("parser: special check ospf password\n");
	if (strlen(str) > CLI_OSPF_PASSWD)
		return RCPERR;
	
	return 0;
}

static int chk_ospf_md5_password(const char *token, const char* str) {
	rcpDebug("parser: special check ospf md5 password\n");
	if (strlen(str) > CLI_OSPF_MD5_PASSWD)
		return RCPERR;
	
	return 0;
}

static int chk_snmp_password(const char *token, const char* str) {
	rcpDebug("parser: special check snmp password\n");
	int len = strlen(str);
	if (len < CLI_SNMP_MIN_PASSWD || len > CLI_SNMP_MAX_PASSWD)
		return RCPERR;
	
	return 0;
}
static int chk_snmp_community(const char *token, const char* str) {
	rcpDebug("parser: special check snmp community password\n");
	int len = strlen(str);
	if (len > CLI_SNMP_MAX_COMMUNITY)
		return RCPERR;
	
	return 0;
}

static int chk_administrator(const char *token, const char* str) {
	rcpDebug("parser: special check administrator\n");
	if (strlen(str) > CLI_MAX_ADMIN_NAME)
		return RCPERR;
	
	return 0;
}

static int chk_password(const char *token, const char* str) {
	rcpDebug("parser: special check password\n");
	if (strlen(str) > CLI_MAX_PASSWD)
		return RCPERR;
	
	return 0;
}

static int chk_hostname(const char *token, const char* str) {
	rcpDebug("parser: special check hostname\n");
	if (strlen(str) > CLI_MAX_HOSTNAME)
		return RCPERR;

	if (rcpInputNotClean(str))
		return RCPERR;
		
	return 0;
}

static int chk_tunnel_name(const char *token, const char* str) {
	rcpDebug("parser: special check tunnel name\n");
	if (strlen(str) > CLI_MAX_TUNNEL_NAME)
		return RCPERR;

	if (rcpInputNotClean(str))
		return RCPERR;
		
	return 0;
}

static int chk_domain_name(const char *token, const char* str) {
	rcpDebug("parser: special check hostname\n");
	if (strlen(str) > CLI_MAX_DOMAIN_NAME)
		return RCPERR;

	if (rcpInputNotClean(str))
		return RCPERR;
	
	// char '@' is permitted by rcpInputNotClean, but not for a domain
	const char *ptr = str;
	while (*ptr != '\0') {
		if (*ptr == '@')
			return RCPERR;
		ptr++;	
	}
	
	return 0;
}

// regular file name check
// allows : and @, in use for regular system files and all tftp, ftp and sftp URLs
static int chk_file(const char *token, const char *str) {
	rcpDebug("parser: special check file\n");
	ASSERT(str != NULL);
	size_t len = strlen(str);
	ASSERT(len != 0);
	ASSERT(strstr(str, " ") == NULL);
	
	// some characters just don't appear in filenames
	if (strcspn(str, "\\*&!?\"'<>%^(){}[];, ") != len)
		return RCPERR;
	
	// no hidden files allowed
	if (*str == '.')
		return RCPERR;
	
	return 0;
}

// existing file check
static int chk_existing_file(const char *token, const char *str) {
	rcpDebug("parser: special check existing file\n");
	ASSERT(str != NULL);
	int rv;	
	if ((rv = chk_file(token, str)) != 0)
		return rv;
	
	struct stat s;
	if (stat(str, &s))
		return RCPERR;
	
	if (!S_ISREG(s.st_mode))
		return RCPERR;
	
	return 0;
}

// tftp file check
// the format is tftp://hostname/filename
static int chk_tftp_file(const char *token, const char *str) {
	rcpDebug("parser: special check tftp file\n");
	ASSERT(str != NULL);

	// file starts with tftp://
	if (strncmp(str, "tftp://", 7) != 0)
		return RCPERR;

	// no funny characters
	int rv;	
	if ((rv = chk_file(token, str)) != 0)
		return rv;
	
	// at least one / separating hostname from the file name
	if (strchr(str + 7, '/') == NULL)
		return RCPERR;

	return 0;
}

// ftp file check
// the format is tftp://user:password@hostname/filename
static int chk_ftp_file(const char *token, const char *str) {
	rcpDebug("parser: special check ftp file\n");
	ASSERT(str != NULL);

	// file starts with ftp://
	if (strncmp(str, "ftp://", 6) != 0)
		return RCPERR;

	// no funny characters
	int rv;	
	if ((rv = chk_file(token, str)) != 0)
		return rv;
	
	// at least one : separating the user name from the rest
	const char *ptr = str + 6;
	if ((ptr = strchr(ptr, ':')) == NULL)
		return RCPERR;

	// at least one @ separating the user name and password from the rest
	if ((ptr = strchr(ptr, '@')) == NULL)
		return RCPERR;

	// at least one / separating the user name, password and hostname from the file name
	if ((ptr = strchr(ptr, '/')) == NULL)
		return RCPERR;

	// a single @
	ptr = str + 6;
	if (strchr(ptr, '@') != strrchr(ptr, '@'))
		return RCPERR;
		
	// a single :
	if (strchr(ptr, ':') != strrchr(ptr, ':'))
		return RCPERR;
	
	return 0;
}


// sftp file check
// the format is tftp://user:password@hostname/filename
static int chk_sftp_file(const char *token, const char *str) {
	rcpDebug("parser: special check sftp file\n");
	ASSERT(str != NULL);

	// file starts with ftp://
	if (strncmp(str, "sftp://", 7) != 0)
		return RCPERR;

	// no funny characters
	int rv;	
	if ((rv = chk_file(token, str)) != 0)
		return rv;
	
	// at least one : separating the user name from the rest
	const char *ptr = str + 7;
	if ((ptr = strchr(ptr, ':')) == NULL)
		return RCPERR;

	// at least one @ separating the user name and password from the rest
	if ((ptr = strchr(ptr, '@')) == NULL)
		return RCPERR;

	// at least one / separating the user name, password and hostname from the file name
	if ((ptr = strchr(ptr, '/')) == NULL)
		return RCPERR;

	// a single @
	ptr = str + 7;
	if (strchr(ptr, '@') != strrchr(ptr, '@'))
		return RCPERR;
		
	// a single :
	if (strchr(ptr, ':') != strrchr(ptr, ':'))
		return RCPERR;
	
	return 0;
}


int is_cidr_format(const char *str) {
	rcpDebug("parser: special check cidr format\n");
	ASSERT(str != NULL);

	const char *ptr = str;
	int pcnt = 0;
	int scnt = 0;
	while (*ptr != '\0') {
		if (*ptr == '.')
			pcnt++;
		else if (*ptr == '/')
			scnt++;
		else if (isdigit(*ptr))
			;
		else
			return 0;
		ptr++;
	}
	if (pcnt != 3)
		return 0;
	if (scnt != 1)
		return 0;

	unsigned a, b, c, d, e;
	int rv = sscanf(str, "%d.%d.%d.%d/%d", &a, &b, &c, &d, &e);
	if (rv != 5)
		return 0;
	return 1;
}

static int chk_ip(const char *token, const char* str) {
	rcpDebug("parser: special check ip\n");
	ASSERT(str != NULL);
	
	if (is_cidr_format(str))
		return RCPERR;

	const char *ptr = str;
	int pcnt = 0;
	while (*ptr != '\0') {
		if (*ptr == '.')
			pcnt++;
		else if (isdigit(*ptr))
			;
		else
			return RCPERR;
		ptr++;
	}
	if (pcnt != 3)
		return RCPERR;

	int a, b, c, d;
	int rv = sscanf(str, "%d.%d.%d.%d", &a, &b, &c, &d);
	if (a < 0 || b < 0 || c < 0 || d < 0 || rv != 4)
		return RCPERR;
	
	if (a > 255 || b > 255 || c > 255 || d > 255)
		return RCPERR;
	
	// no multicast and no broadcast
	uint32_t ip = 0;
	atoip(str, &ip);
	if (isMulticast(ip) || isBroadcast(ip))
		return RCPERR;
			
	return 0;
}

static int chk_mac(const char *token, const char* str) {
	rcpDebug("parser: special check mac\n");
	ASSERT(str != NULL);
	unsigned a, b, c, d, e, f;
	
	int rv = sscanf(str, "%x:%x:%x:%x:%x:%x", &a, &b, &c, &d, &e, &f);
	
	if (a > 255 || b > 255 || c > 255 || d > 255 || e > 255 || f > 255 || rv != 6)
		return RCPERR;
		
	return 0;
}

// check if ptr is an integer number
static int isnum(const char *ptr) {
	if (*ptr == '\0')
		return 0;
	
	while (*ptr != '\0') {
		if (!isdigit((int) *ptr))
			return 0;
		ptr++;
	}
	return 1;
}


static int chk_int2(const char *token, const char* str) {
	rcpDebug("parser: special check int2\n");
	ASSERT(str != NULL);
	ASSERT(token != NULL);
	
	// parse the token
	int a, b;
	int rv = sscanf(token, "<i,%d,%d>", &a, &b);
	if (rv != 2)
		return RCPERR;
		
	// parse the string
	if (!isnum(str))
		return RCPERR;
	int val = atoi(str);
	if (val < a || val > b)
		return RCPERR;
	return 0;
}


static void gen_int2(const char *token) {
	ASSERT(token != NULL);
	int a, b;
	int rv = sscanf(token, "<i,%d,%d>", &a, &b);
	ASSERT(rv == 2);

	snprintf(genhelpstr, GENHELP_STR_LEN, "%d..%d", a, b);
}

static int chk_uint2(const char *token, const char* str) {
	rcpDebug("parser: special check uint2\n");
	ASSERT(str != NULL);
	ASSERT(token != NULL);

	// parse the token
	unsigned a, b;
	int rv = sscanf(token, "<u,%u,%u>", &a, &b);
	if (rv != 2)
		return RCPERR;
		
	// parse the string
	if (!isnum(str))
		return RCPERR;

	unsigned val;
	sscanf(str, "%u", &val);
	if (val < a || val > b)
		return RCPERR;

	return 0;
}

static void gen_uint2(const char *token) {
	ASSERT(token != NULL);
	unsigned a, b;
	int rv = sscanf(token, "<u,%u,%u>", &a, &b);
	ASSERT(rv == 2);

	snprintf(genhelpstr, GENHELP_STR_LEN, "%u..%u", a, b);
}



static int chk_string2(const char *token, const char* str) {
	rcpDebug("parser: special check uint2\n");
	ASSERT(str != NULL);
	ASSERT(token != NULL);

	// parse the token
	unsigned a, b;
	int rv = sscanf(token, "<s,%u,%u>", &a, &b);
	if (rv != 2)
		return RCPERR;
		
	int len = strlen(str);
	if (len < a || len > b)
		return RCPERR;

	return 0;
}

static void generr_string2(const char *token) {
	ASSERT(token != NULL);
	unsigned a, b;
	int rv = sscanf(token, "<s,%u,%u>", &a, &b);
	ASSERT(rv == 2);

	snprintf(generrstr, GENHELP_STR_LEN, "invalid value, %u to %u characters expected", a, b);
}

static int chk_interface(const char *token, const char* str) {
	rcpDebug("parser: special check interface\n");
	ASSERT(str != NULL);
	ASSERT(token != NULL);
	if (strlen(str) > RCP_MAX_IF_NAME)
		return RCPERR;
		
	// find interfaces
	RcpShm *shm = rcpGetShm();
	int i;
	for (i = 0; i < RCP_INTERFACE_LIMITS; i++) {
		RcpInterface *intf = &shm->config.intf[i];
		if (*intf->name == '\0')
			continue;
		if (rcpInterfaceVirtual(intf->name))
			continue;
		if (strcmp(str, intf->name) == 0)
			return 0;
	}
	
	return RCPERR;
}

static void generr_interface(const char *token) {
	char *ptr = generrstr;
	sprintf(ptr, "invalid interface name.\nAvailable interfaces: ");
	ptr += strlen(ptr);

	// check total string length
	int len = strlen(generrstr);
	int i;
	RcpShm *shm = rcpGetShm();
	for (i = 0; i < RCP_INTERFACE_LIMITS; i++) {
		RcpInterface *intf = &shm->config.intf[i];
		if (*intf->name == '\0')
			continue;
		if (rcpInterfaceVirtual(intf->name))
			continue;
		len += strlen(intf->name + 2);
	}
	len += 2;
	if (len > GENHELP_STR_LEN) {
		sprintf(generrstr, "invalid interface name");
		return;
	}
	
	// list interfaces
	for (i = 0; i < RCP_INTERFACE_LIMITS; i++) {
		RcpInterface *intf = &shm->config.intf[i];
		if (*intf->name == '\0')
			continue;
		sprintf(ptr, "%s, ", intf->name);
		ptr += strlen(ptr);
	}
	*ptr++ = '\n';
	*ptr = '\0';
}

static int chk_mask(const char *token, const char* str) {
	rcpDebug("parser: special check mask\n");
	ASSERT(str != NULL);
	if (is_cidr_format(str))
		return RCPERR;

	const char *ptr = str;
	int pcnt = 0;
	while (*ptr != '\0') {
		if (*ptr == '.')
			pcnt++;
		else if (isdigit(*ptr))
			;
		else
			return RCPERR;
		ptr++;
	}
	if (pcnt != 3)
		return RCPERR;		

	unsigned a, b, c, d;
	int rv = sscanf(str, "%d.%d.%d.%d", &a, &b, &c, &d);
	if (a > 255 || b > 255 || c > 255 || d > 255 || rv != 4)
		return RCPERR;

	uint32_t mask = (a << 24) | (b << 16) | (c << 8) | d;
	uint32_t m = 0x80000000;
	int zero;
	int i;
	for (i = 0, zero = 0; i < 32; i++, m >>= 1) {
		if (zero == 1 && (m & mask) == 0)
			return RCPERR;
		if ((m & mask) == 1)
			zero = 1;
	}
	
	return 0;
}

static int chk_cidr(const char *token, const char* str) {
	rcpDebug("parser: special check cidr\n");
	ASSERT(str != NULL);
	
	if (is_cidr_format(str) != 1)
		return RCPERR;
		
	unsigned a, b, c, d, e;
	int rv = sscanf(str, "%d.%d.%d.%d/%d", &a, &b, &c, &d, &e);
	if (a > 255 || b > 255 || c > 255 || d > 255 || e > 32 || rv != 5)
		return RCPERR;

	// no multicast and no broadcast for prefix
	uint32_t ip = 0;
	uint32_t mask = 0;
	atocidr(str, &ip, &mask);
	if (isMulticast(ip) || isBroadcast(ip))
		return RCPERR;


	return 0;
}

static int chk_cidrloopback(const char *token, const char* str) {
	rcpDebug("parser: special check cidrloopback\n");
	ASSERT(str != NULL);
	
	if (is_cidr_format(str) != 1)
		return RCPERR;
		
	unsigned a, b, c, d, e;
	int rv = sscanf(str, "%d.%d.%d.%d/%d", &a, &b, &c, &d, &e);
	if (a > 255 || b > 255 || c > 255 || d > 255 || e > 32 || rv != 5)
		return RCPERR;

	// no multicast and no broadcast for prefix
	uint32_t ip;
	uint32_t mask;
	atocidr(str, &ip, &mask);

	return 0;
}

static int chk_cidrnet(const char *token, const char* str) {
	rcpDebug("parser: special check cidrnet\n");
	ASSERT(str != NULL);
	
	if (is_cidr_format(str) != 1)
		return RCPERR;
		
	unsigned a, b, c, d, e;
	int rv = sscanf(str, "%d.%d.%d.%d/%d", &a, &b, &c, &d, &e);
	if (a > 255 || b > 255 || c > 255 || d > 255 || e > 32 || rv != 5)
		return RCPERR;
	
	uint32_t ip;
	uint32_t mask;
	if (atocidr(str, &ip, &mask))
		return RCPERR;
	if (ip & ~mask)
		return RCPERR;
	
	// no multicast prefix
	if (isMulticast(ip))
		return RCPERR;
	
	return 0;
}

static int chk_userathost(const char *token, const char* str) {
	rcpDebug("parser: special check string\n");
	ASSERT(str != NULL);
	ASSERT(token != NULL);

	// fail empty string
	if (strlen(str) == 0)
		return RCPERR;

	// fail unsanitized input
	if (rcpInputNotClean(str))
		return RCPERR;
	
	// user missing
	if (*str == '@')
		return RCPERR;

	// @ missing
	char *ptr =strchr(str, '@');
	if (ptr == NULL)	
		return RCPERR;
	
	// host missing
	if (*(ptr + 1) == '\0')
		return RCPERR;
		
	return 0;
}

static int chk_port(const char *token, const char* str) {
	rcpDebug("parser: special check port\n");
	ASSERT(str != NULL);
	int port;

	const char *ptr = str;
	while (*ptr != '\0') {
		if (isdigit((int)*ptr) == 0)
			return RCPERR;
		ptr++;
	}
	
	int rv = sscanf(str, "%d", &port);
	if (rv != 1 || port < 1 || port > 65535)
		return RCPERR;
		
	return 0;
}

static int chk_constate(const char *token, const char* str) {
	rcpDebug("parser: special check connection state\n");
	ASSERT(str != NULL);

	// preserve the original string
	int len = strlen(str);
	char storage[len + 1];
	strcpy(storage, str);

	// parsing
	char *ptr = strtok((char *)str, ",");
	while(ptr != NULL) {
		if (strcmp(ptr, "new") == 0)
			;
		else if (strcmp(ptr, "established") == 0)
			;
		else if (strcmp(ptr, "related") == 0)
			;
		else if (strcmp(ptr, "invalid") == 0)
			;
		else {
			// restore string
			strcpy((char *) str, storage);
			return RCPERR;
		}
		
		ptr = strtok(NULL, ",");
	}
		
	// restore string
	strcpy((char *) str, storage);
	return 0;
}

static int chk_icmp_type(const char *token, const char* str) {
	rcpDebug("parser: special check icmp type\n");
	ASSERT(str != NULL);
	int type;

	const char *ptr = str;
	while (*ptr != '\0') {
		if (isdigit((int)*ptr) == 0)
			return RCPERR;
		ptr++;
	}
	
	int rv = sscanf(str, "%d", &type);
	if (rv != 1 || type < 0 || type > 255)
		return RCPERR;
		
	return 0;
}

