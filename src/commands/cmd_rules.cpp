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
#include "commands/cmd_rules.h"

extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandRules(Instance);
}

CmdResult CommandRules::Handle (const char** parameters, int pcnt, User *user)
{
	user->ShowRULES();
	return CMD_SUCCESS;
}
