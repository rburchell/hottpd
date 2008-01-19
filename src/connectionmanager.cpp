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

	if ((ServerInstance->local_connections.size() > ServerInstance->Config->SoftLimit) || (ServerInstance->local_connections.size() >= MAXCLIENTS))
	{
		this->Delete(New);
		return;
	}

	/*
	 * XXX -
	 * this is done as a safety check to keep the file descriptors within range of fd_ref_table.
	 * its a pretty big but for the moment valid assumption:
	 * file descriptors are handed out starting at 0, and are recycled as theyre freed.
	 * therefore if there is ever an fd over 65535, 65536 clients must be connected to the
	 * irc server at once (or the irc server otherwise initiating this many connections, files etc)
	 * which for the time being is a physical impossibility (even the largest networks dont have more
	 * than about 10,000 users on ONE server!)
	 */
#ifndef WINDOWS
	if ((unsigned int)socket >= MAX_DESCRIPTORS)
	{
		this->Delete(New);
		return;
	}
#endif

	if (!ServerInstance->SE->AddFd(New))
	{
		ServerInstance->Log(DEBUG,"Internal error on new connection");
		this->Delete(New);
		return;
	}

	New->FullConnect();
}

void ConnectionManager::Delete(Connection *c)
{
	ServerInstance->GlobalCulls.AddItem(c);
	c->quitting = true;
	c->State = HTTP_FINISHED;
}


