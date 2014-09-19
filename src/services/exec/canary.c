#include "services.h"

void check_canary(void) {

	// memory monitoring using canary
	ASSERT(shm->stats.can1 ==  RCP_CANARY);
	ASSERT(shm->stats.can2 ==  RCP_CANARY);
	ASSERT(shm->stats.can3 ==  RCP_CANARY);
	ASSERT(shm->stats.can4 ==  RCP_CANARY);
	ASSERT(shm->stats.can5 ==  RCP_CANARY);
	ASSERT(shm->stats.can6 ==  RCP_CANARY);
	
	int i;
	for (i = 0; i <RCP_INTERFACE_LIMITS; i++) {
		if (shm->config.intf[i].name[0] == '\0')
			continue;
			
		ASSERT(shm->config.intf[i].stats.can1 == RCP_CANARY);	
		ASSERT(shm->config.intf[i].stats.can2 == RCP_CANARY);	
	}
	
	for (i = 0; i < RCP_NETMON_LIMIT; i++) {
		if (shm->config.netmon_host[i].valid == 0)
			continue;
			
		ASSERT(shm->config.netmon_host[i].can1 == RCP_CANARY);
		ASSERT(shm->config.netmon_host[i].can2 == RCP_CANARY);
		ASSERT(shm->config.netmon_host[i].can3 == RCP_CANARY);
	}
}
