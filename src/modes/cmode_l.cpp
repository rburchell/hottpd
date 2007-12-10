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
#include "modes/cmode_l.h"

ModeChannelLimit::ModeChannelLimit(InspIRCd* Instance) : ModeHandler(Instance, 'l', 1, 0, false, MODETYPE_CHANNEL, false)
{
}

ModePair ModeChannelLimit::ModeSet(User*, User*, Channel* channel, const std::string &parameter)
{
	if (channel->limit)
	{
		return std::make_pair(true, ConvToStr(channel->limit));
	}
	else
	{
		return std::make_pair(false, parameter);
	}
}

bool ModeChannelLimit::CheckTimeStamp(time_t, time_t, const std::string &their_param, const std::string &our_param, Channel*)
{
	/* When TS is equal, the higher channel limit wins */
	return (atoi(their_param.c_str()) < atoi(our_param.c_str()));
}

ModeAction ModeChannelLimit::OnModeChange(User*, User*, Channel* channel, std::string &parameter, bool adding)
{
	if (adding)
	{
		/* Setting a new limit, sanity check */
		long limit = atoi(parameter.c_str());

		/* Wrap low values at 32768 */
		if (limit < 0)
			limit = 0x7FFF;

		/* If the new limit is the same as the old limit,
		 * and the old limit isnt 0, disallow */
		if ((limit == channel->limit) && (channel->limit > 0))
		{
			parameter = "";
			return MODEACTION_DENY;
		}

		/* They must have specified an invalid number.
		 * Dont allow +l 0.
		 */
		if (!limit)
		{
			parameter = "";
			return MODEACTION_DENY;
		}

		parameter = ConvToStr(limit);

		/* Set new limit */
		channel->limit = limit;
		channel->modes[CM_LIMIT] = 1;

		return MODEACTION_ALLOW;
	}
	else
	{
		/* Check if theres a limit here to remove.
		 * If there isnt, dont allow the -l
		 */
		if (!channel->limit)
		{
			parameter = "";
			return MODEACTION_DENY;
		}

		/* Removing old limit, no checks here */
		channel->limit = 0;
		channel->modes[CM_LIMIT] = 0;

		return MODEACTION_ALLOW;
	}

	return MODEACTION_DENY;
}
