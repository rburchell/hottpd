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
#include "modes/cmode_s.h"

ModeChannelSecret::ModeChannelSecret(InspIRCd* Instance) : ModeHandler(Instance, 's', 0, 0, false, MODETYPE_CHANNEL, false)
{
}

ModeAction ModeChannelSecret::OnModeChange(User*, User*, Channel* channel, std::string&, bool adding)
{
	if (channel->modes[CM_SECRET] != adding)
	{
		channel->modes[CM_SECRET] = adding;
		return MODEACTION_ALLOW;
	}
	else
	{
		return MODEACTION_DENY;
	}
}
