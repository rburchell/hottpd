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
#include "commands/cmd_away.h"

extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandAway(Instance);
}

/** Handle /AWAY
 */
CmdResult CommandAway::Handle (const char** parameters, int pcnt, User *user)
{
	if ((pcnt) && (*parameters[0]))
	{
		strlcpy(user->awaymsg,parameters[0],MAXAWAY);
		user->WriteServ("306 %s :You have been marked as being away",user->nick);
		FOREACH_MOD(I_OnSetAway,OnSetAway(user));
	}
	else
	{
		*user->awaymsg = 0;
		user->WriteServ("305 %s :You are no longer marked as being away",user->nick);
		FOREACH_MOD(I_OnCancelAway,OnCancelAway(user));
	}
	return CMD_SUCCESS;
}
