/**
 * Copyright (c) 2023 Paul-Louis Ageneau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "conn_user.h"
#include "agent.h"
#include "log.h"
#include "socket.h"
#include "udp.h"

#include <stdint.h>

#define BUFFER_SIZE 4096
#define MAX_AGENT (BUFFER_SIZE/sizeof(struct pollfd))

typedef enum conn_state { CONN_STATE_NEW = 0, CONN_STATE_READY, CONN_STATE_FINISHED } conn_state_t;

typedef struct conn_impl {
	conn_state_t state;
	socket_t sock;
	int send_ds;
	timestamp_t next_timestamp;
} conn_impl_t;

static inline int conn_user_process(juice_agent_t **agents, int agents_count, uint32_t *has_packets_pending, char *buffer);
static inline int conn_user_recv(socket_t sock, char *buffer, size_t size, addr_record_t *src);

JUICE_EXPORT int juice_user_poll(juice_agent_t **agents, int agents_count, int timeout) {
	JLOG_VERBOSE("Setting up poll for %d agents", agents_count);

	if (agents_count == 0)
		return JUICE_ERR_SUCCESS;

	if (!agents)
		return JUICE_ERR_INVALID;

	if (agents_count > MAX_AGENT) {
		JLOG_ERROR("agent_count > %d", MAX_AGENT);
		return JUICE_ERR_INVALID;
	}

	// For explicit memory reuse
	union {
		struct pollfd pfds[MAX_AGENT];
		char buffer[BUFFER_SIZE];
	} u;
	struct pollfd *pfds = u.pfds;

	uint32_t has_packets_pending[(MAX_AGENT-1)/32+1];

	int status = JUICE_ERR_SUCCESS;
	for(int i = 0; i < agents_count; ++i) {
		struct pollfd *pfd = pfds + i;

		if (!agents[i]) {
			JLOG_ERROR("agents[%d] is NULL", i);
			status = JUICE_ERR_INVALID;
			continue;
		}

		conn_impl_t *conn_impl = agents[i]->conn_impl;
		// @todo This might be a hack I was getting warnings after I removed it though. I think at worst it just performs extra work until we connect
		if (agents[i]->state != JUICE_STATE_COMPLETED) {
			if (agent_conn_update(agents[i], &conn_impl->next_timestamp) != 0) {
				JLOG_WARN("Agent update failed");
				conn_impl->state = CONN_STATE_FINISHED;
				continue;
			}
		}

		if (agents[i]->config.concurrency_mode != JUICE_CONCURRENCY_MODE_USER) {
			JLOG_ERROR("agents[%d].config.concurrency_mode=%d Only JUICE_CONCURRENCY_MODE_USER (%d) is supported", 
			            i, agents[i]->config.concurrency_mode, JUICE_CONCURRENCY_MODE_USER);
			status = JUICE_ERR_INVALID;
			continue;
		}

		if (conn_impl->state != CONN_STATE_NEW && conn_impl->state != CONN_STATE_READY) {
			pfd->fd = INVALID_SOCKET;
			pfd->events = 0;
			continue;
		}

		if (conn_impl->state == CONN_STATE_NEW)
			conn_impl->state = CONN_STATE_READY;

		pfd->fd = conn_impl->sock;
		pfd->events = POLLIN;
	}

	if (status != JUICE_ERR_SUCCESS)
		return status;

	JLOG_VERBOSE("Entering poll on %d sockets", agents_count);
	while ((poll(pfds, agents_count, timeout)) < 0) {
		// POSIX allows kernels to set these two errors unconditionally when calling poll
		// In this case looping until poll succeeds is standard practice
		if (sockerrno == SEINTR || sockerrno == SEAGAIN) {
			JLOG_VERBOSE("poll interrupted");
		} else {
			JLOG_FATAL("poll failed, error=%d", sockerrno);
		}
	}
	JLOG_VERBOSE("Leaving poll");

	for (int i = 0; i < agents_count; ++i) {
		struct pollfd *pfd = pfds + i;

		if (pfd->revents & POLLNVAL || pfd->revents & POLLERR) {
			JLOG_WARN("Error when polling socket on agent[%d]", i);
			agent_conn_fail(agents[i]);
			conn_impl_t *conn_impl = agents[i]->conn_impl;
			conn_impl->state = CONN_STATE_FINISHED;
			continue;
		}

		uint32_t pending = (pfd->revents & POLLIN) > 0;
		has_packets_pending[i/32] |= pending << (i%32);
	}

	// This subroutine protects from accidently accessing pfds
	return conn_user_process(agents, agents_count, has_packets_pending, u.buffer);
}

static inline int conn_user_process(juice_agent_t **agents, int agents_count, uint32_t *has_packets_pending, char *buffer) {
	for (int i = 0; i < agents_count; ++i) {
		juice_agent_t *agent = agents[i];

		conn_impl_t *conn_impl = agent->conn_impl;
		if (!conn_impl || conn_impl->state != CONN_STATE_READY)
			continue;

		if (has_packets_pending[i/32] & (1 << (i%32))) {
			addr_record_t src;
			int ret = 0;
			while ((ret = conn_user_recv(conn_impl->sock, buffer, BUFFER_SIZE, &src)) > 0) {
				if (agent_conn_recv(agent, buffer, (size_t)ret, &src) != 0) {
					JLOG_WARN("Agent receive failed");
					conn_impl->state = CONN_STATE_FINISHED;
					break;
				}
			}
			if (conn_impl->state == CONN_STATE_FINISHED)
				continue;

			if (ret < 0) {
				agent_conn_fail(agent);
				conn_impl->state = CONN_STATE_FINISHED;
				continue;
			}

			if (agent_conn_update(agent, &conn_impl->next_timestamp) != 0) {
				JLOG_WARN("Agent update failed");
				conn_impl->state = CONN_STATE_FINISHED;
				continue;
			}

		} else if (conn_impl->next_timestamp <= current_timestamp()) {
			if (agent_conn_update(agent, &conn_impl->next_timestamp) != 0) {
				JLOG_WARN("Agent update failed");
				conn_impl->state = CONN_STATE_FINISHED;
				continue;
			}
		}
	}

	return JUICE_ERR_SUCCESS;
}

static inline int conn_user_recv(socket_t sock, char *buffer, size_t size, addr_record_t *src) {
	JLOG_VERBOSE("Receiving datagram");
	int len;
	while ((len = udp_recvfrom(sock, buffer, size, src)) == 0) {
		// Empty datagram, ignore
	}

	if (len < 0) {
		if (sockerrno == SEAGAIN || sockerrno == SEWOULDBLOCK) {
			JLOG_VERBOSE("No more datagrams to receive");
			return 0;
		}
		JLOG_ERROR("recvfrom failed, errno=%d", sockerrno);
		return -1;
	}

	addr_unmap_inet6_v4mapped((struct sockaddr *)&src->addr, &src->len);
	return len; // len > 0
}

int conn_user_init(juice_agent_t *agent, conn_registry_t *registry, udp_socket_config_t *config) {
	(void)registry;

	conn_impl_t *conn_impl = calloc(1, sizeof(conn_impl_t));
	if (!conn_impl) {
		JLOG_FATAL("Memory allocation failed for connection impl");
		return -1;
	}

	conn_impl->sock = udp_create_socket(config);
	if (conn_impl->sock == INVALID_SOCKET) {
		JLOG_ERROR("UDP socket creation failed");
		free(conn_impl);
		return -1;
	}

	agent->conn_impl = conn_impl;

	return JUICE_ERR_SUCCESS;
}

void conn_user_cleanup(juice_agent_t *agent) {
	conn_impl_t *conn_impl = agent->conn_impl;

	closesocket(conn_impl->sock);
	free(agent->conn_impl);
	agent->conn_impl = NULL;
}

void conn_user_lock(juice_agent_t *agent) {
}

void conn_user_unlock(juice_agent_t *agent) {
}

int conn_user_interrupt(juice_agent_t *agent) {
	conn_impl_t *conn_impl = agent->conn_impl;
	conn_impl->next_timestamp = current_timestamp();
	return JUICE_ERR_SUCCESS;
}

int conn_user_send(juice_agent_t *agent, const addr_record_t *dst, const char *data, size_t size,
                   int ds) {
	conn_impl_t *conn_impl = agent->conn_impl;

	if (conn_impl->send_ds >= 0 && conn_impl->send_ds != ds) {
		JLOG_VERBOSE("Setting Differentiated Services field to 0x%X", ds);
		if (udp_set_diffserv(conn_impl->sock, ds) == 0)
			conn_impl->send_ds = ds;
		else
			conn_impl->send_ds = -1; // disable for next time
	}

	JLOG_VERBOSE("Sending datagram, size=%d", size);

	int ret = udp_sendto(conn_impl->sock, data, size, dst);
	if (ret < 0) {
		if (sockerrno == SEAGAIN || sockerrno == SEWOULDBLOCK)
			JLOG_INFO("Send failed, buffer is full");
		else if (sockerrno == SEMSGSIZE)
			JLOG_WARN("Send failed, datagram is too large");
		else
			JLOG_WARN("Send failed, errno=%d", sockerrno);
	}

	return ret;
}

int conn_user_get_addrs(juice_agent_t *agent, addr_record_t *records, size_t size) {
	conn_impl_t *conn_impl = agent->conn_impl;

	return udp_get_addrs(conn_impl->sock, records, size);
}

