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

#include "inspircd.h"
#include "commands/cmd_kick.h"

extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandKick(Instance);
}

/** Handle /KICK
 */
CmdResult CommandKick::Handle (const char** parameters, int pcnt, User *user)
{
	char reason[MAXKICK];
	Channel* c = ServerInstance->FindChan(parameters[0]);
	User* u = ServerInstance->FindNick(parameters[1]);

	if (!u || !c)
	{
		user->WriteServ( "401 %s %s :No such nick/channel", user->nick, u ? parameters[0] : parameters[1]);
		return CMD_FAILURE;
	}

	if ((IS_LOCAL(user)) && (!c->HasUser(user)) && (!ServerInstance->ULine(user->server)))
	{
		user->WriteServ( "442 %s %s :You're not on that channel!", user->nick, parameters[0]);
		return CMD_FAILURE;
	}

	if (pcnt > 2)
	{
		strlcpy(reason, parameters[2], MAXKICK - 1);
	}
	else
	{
		strlcpy(reason, user->nick, MAXKICK - 1);
	}

	if (!c->KickUser(user, u, reason))
		/* Nobody left here, delete the Channel */
		delete c;

	return CMD_SUCCESS;
}
