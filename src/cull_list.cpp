/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* $Core: libIRCDcull_list */

#include "inspircd.h"
#include "cull_list.h"

CullList::CullList(InspIRCd* Instance) : ServerInstance(Instance)
{
	list.clear();
}

void CullList::AddItem(Connection *c)
{
	if (c->quitting) // don't quit them twice
		return;

	list.push_back(c);
	c->quitting = true;
}

int CullList::Apply()
{
	int n = list.size();
	while (list.size())
	{
		std::vector<Connection *>::iterator a = list.begin();

		Connection *c = (*a);

		FOREACH_MOD_I(ServerInstance,I_OnConnectionDisconnect, OnConnectionDisconnect(c));

		ServerInstance->SE->DelFd(c);
		c->CloseSocket();

		std::vector<Connection*>::iterator x = find(ServerInstance->local_connections.begin(),ServerInstance->local_connections.end(),c);
		if (x != ServerInstance->local_connections.end())
			ServerInstance->local_connections.erase(x);

		delete c;
		list.erase(list.begin());
	}

	return n;
}

