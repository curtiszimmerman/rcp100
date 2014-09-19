#ifndef LIBRCP_SNMP_H
#define LIBRCP_SNMP_H

#ifdef __cplusplus
extern "C" {
#endif

#define RCPSNMP_STATS_FILE "/opt/rcp/var/transport/snmpstats"

typedef struct {
	// in/out pkts
	unsigned inpkts;
	unsigned ingetrq;
	unsigned innextrq;
	unsigned insetrq;
	unsigned inerrbadver;
	unsigned inerrbadcomname;
	unsigned inerrbadcomuses;
	unsigned inerrasn;

	// output packets
	unsigned outpkts;
	unsigned outgetresp;
	unsigned outerrtoobig;
	unsigned outerrnosuchname;
	unsigned outerrbadvalue;
	unsigned outerrgenerr;
} RcpSnmpStats;


#ifdef __cplusplus
}
#endif

#endif
