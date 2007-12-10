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
#include "modes/umode_i.h"

ModeUserInvisible::ModeUserInvisible(InspIRCd* Instance) : ModeHandler(Instance, 'i', 0, 0, false, MODETYPE_USER, false)
{
}

ModeAction ModeUserInvisible::OnModeChange(User* source, User* dest, Channel*, std::string&, bool adding)
{
	/* Only opers can change other users modes */
	if ((source != dest) && (!*source->oper))
		return MODEACTION_DENY;

	/* Set the bitfields */
	if (dest->modes[UM_INVISIBLE] != adding)
	{
		dest->modes[UM_INVISIBLE] = adding;
		return MODEACTION_ALLOW;
	}

	/* Allow the change */
	return MODEACTION_DENY;
}

unsigned int ModeUserInvisible::GetCount()
{
	return count;
}
