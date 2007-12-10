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
#include "modes/cmode_k.h"

ModeChannelKey::ModeChannelKey(InspIRCd* Instance) : ModeHandler(Instance, 'k', 1, 1, false, MODETYPE_CHANNEL, false)
{
}

ModePair ModeChannelKey::ModeSet(User*, User*, Channel* channel, const std::string &parameter)
{       
        if (channel->modes[CM_KEY])
        {
                return std::make_pair(true, channel->key);
        }
        else
        {
                return std::make_pair(false, parameter);
        }
}

void ModeChannelKey::RemoveMode(Channel* channel)
{
	/** +k needs a parameter when being removed,
	 * so we have a special-case RemoveMode here for it
	 */
	char moderemove[MAXBUF];
	const char* parameters[] = { channel->name, moderemove, channel->key };

	if (channel->IsModeSet(this->GetModeChar()))
	{
		sprintf(moderemove,"-%c",this->GetModeChar());
		ServerInstance->SendMode(parameters, 3, ServerInstance->FakeClient);
	}
}

void ModeChannelKey::RemoveMode(User*)
{
}

bool ModeChannelKey::CheckTimeStamp(time_t, time_t, const std::string &their_param, const std::string &our_param, Channel*)
{
	/* When TS is equal, the alphabetically later channel key wins */
	return (their_param < our_param);
}

ModeAction ModeChannelKey::OnModeChange(User* source, User*, Channel* channel, std::string &parameter, bool adding)
{
	if ((channel->modes[CM_KEY] != adding) || (!IS_LOCAL(source)))
	{
		if (((channel->modes[CM_KEY]) && (strcasecmp(parameter.c_str(),channel->key))) && (IS_LOCAL(source)))
		{
			/* Key is currently set and the correct key wasnt given */
			return MODEACTION_DENY;
		}
		else if ((!channel->modes[CM_KEY]) || ((adding) && (!IS_LOCAL(source))))
		{
			/* Key isnt currently set */
			if ((parameter.length()) && (parameter.rfind(' ') == std::string::npos))
			{
				strlcpy(channel->key,parameter.c_str(),32);
				channel->modes[CM_KEY] = adding;
				parameter = channel->key;
				return MODEACTION_ALLOW;
			}
			else
				return MODEACTION_DENY;
		}
		else if (((channel->modes[CM_KEY]) && (!strcasecmp(parameter.c_str(),channel->key))) || ((!adding) && (!IS_LOCAL(source))))
		{
			/* Key is currently set, and correct key was given */
			*channel->key = 0;
			channel->modes[CM_KEY] = adding;
			return MODEACTION_ALLOW;
		}
		return MODEACTION_DENY;
	}
	else
	{
		return MODEACTION_DENY;
	}
}

