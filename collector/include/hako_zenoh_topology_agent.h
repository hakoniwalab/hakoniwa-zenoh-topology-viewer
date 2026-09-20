#pragma once

#include "zenoh.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Observes the borrowed session without taking ownership of it.
 * Configuration is read from HAKO_TOPOLOGY_ENDPOINT_CONFIG and the optional
 * HAKO_TOPOLOGY_INTERVAL_MS environment variable.
 */
int hako_topology_agent_attach(const z_loaned_session_t* session, const char* node_name);

/* Must be called before the owning application drops the Zenoh session. */
void hako_topology_agent_detach(void);

#ifdef __cplusplus
}
#endif
