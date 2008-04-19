/*
 *   hottpd - a fast, extensible, featureful http server
 *          (C) 2007-2008 hottpd development team
 *
 * Based on InspIRCd - (C) 2002-2007 InspIRCd Development Team
 *
 *    This program is free but copyrighted software; see
 *              the file COPYING for details.
 *
 */

#include "inspircd.h"

/* $Core: libhttpd_connectionmanager */


void ConnectionManager::Add(int socket, int port, int socketfamily, sockaddr *ip)
{
	Connection* New = NULL;
	New = new Connection(ServerInstance);

	ServerInstance->Log(DEBUG,"New user fd: %d", socket);

	char ipaddr[MAXBUF];
#ifdef IPV6
	if (socketfamily == AF_INET6)
		inet_ntop(AF_INET6, &((const sockaddr_in6*)ip)->sin6_addr, ipaddr, sizeof(ipaddr));
	else
#endif
	inet_ntop(AF_INET, &((const sockaddr_in*)ip)->sin_addr, ipaddr, sizeof(ipaddr));

	New->SetFd(socket);
	New->SetSockAddr(socketfamily, ipaddr, port);
	New->ip = New->GetIPString();

	ServerInstance->local_connections.push_back(New);

	if (!ServerInstance->SE->AddFd(New))
	{
		ServerInstance->Log(DEBUG,"Internal error on new connection");
		this->Delete(New);
		return;
	}

	FOREACH_MOD(I_OnConnectionConnect, OnConnectionConnect(New));
}

void ConnectionManager::Delete(Connection *c)
{
	ServerInstance->GlobalCulls.AddItem(c);
	c->quitting = true;
	c->State = HTTP_FINISHED;
}


