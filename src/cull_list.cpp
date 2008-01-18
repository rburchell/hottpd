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

void CullList::AddItem(User *c)
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
		std::vector<User *>::iterator a = list.begin();

		User *c = (*a);

		FOREACH_MOD_I(ServerInstance,I_OnUserDisconnect,OnUserDisconnect(c));

		if (IS_LOCAL(c))
		{
			ServerInstance->SE->DelFd(c);
			c->CloseSocket();
		}

		if (IS_LOCAL(c))
		{
			std::vector<User*>::iterator x = find(ServerInstance->local_users.begin(),ServerInstance->local_users.end(),c);
			if (x != ServerInstance->local_users.end())
				ServerInstance->local_users.erase(x);
		}

		delete c;
		list.erase(list.begin());
	}

	return n;
}

