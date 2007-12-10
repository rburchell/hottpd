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
#include "commands/cmd_pass.h"

extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandPass(Instance);
}

CmdResult CommandPass::Handle (const char** parameters, int, User *user)
{
	// Check to make sure they havnt registered -- Fix by FCS
	if (user->registered == REG_ALL)
	{
		user->WriteServ("462 %s :You may not reregister",user->nick);
		return CMD_FAILURE;
	}
	ConnectClass* a = user->GetClass();
	if (!a)
		return CMD_FAILURE;

	strlcpy(user->password,parameters[0],63);
	if (a->GetPass() == parameters[0])
	{
		user->haspassed = true;
	}

	return CMD_SUCCESS;
}
