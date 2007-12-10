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
#include "mode.h"
#include "channels.h"
#include "users.h"
#include "modes/umode_o.h"

ModeUserOperator::ModeUserOperator(InspIRCd* Instance) : ModeHandler(Instance, 'o', 0, 0, false, MODETYPE_USER, true)
{
}

ModeAction ModeUserOperator::OnModeChange(User* source, User* dest, Channel*, std::string&, bool adding)
{
	/* Only opers can execute this class at all */
	if (!*source->oper)
		return MODEACTION_DENY;

	/* Not even opers can GIVE the +o mode, only take it away */
	if (adding)
		return MODEACTION_DENY;

	/* Set the bitfields.
	 * Note that oper status is only given in cmd_oper.cpp
	 * NOT here. It is impossible to directly set +o without
	 * verifying as an oper and getting an opertype assigned
	 * to your User!
	 */
	ServerInstance->SNO->WriteToSnoMask('o', "User %s de-opered (by %s)", dest->nick, source->nick);
	dest->UnOper();

	return MODEACTION_ALLOW;
}

unsigned int ModeUserOperator::GetCount()
{
	return ServerInstance->all_opers.size();
}
