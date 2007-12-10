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
#include <string>
#include <vector>
#include "inspircd_config.h"
#include "configreader.h"
#include "hash_map.h"
#include "mode.h"
#include "channels.h"
#include "users.h"
#include "modules.h"
#include "inspstring.h"
#include "hashcomp.h"
#include "modes/cmode_b.h"

ModeChannelBan::ModeChannelBan(InspIRCd* Instance) : ModeHandler(Instance, 'b', 1, 1, true, MODETYPE_CHANNEL, false)
{
}

ModeAction ModeChannelBan::OnModeChange(User* source, User*, Channel* channel, std::string &parameter, bool adding)
{
	int status = channel->GetStatus(source);
	/* Call the correct method depending on wether we're adding or removing the mode */
	if (adding)
	{
		parameter = this->AddBan(source, parameter, channel, status);
	}
	else
	{
		parameter = this->DelBan(source, parameter, channel, status);
	}
	/* If the method above 'ate' the parameter by reducing it to an empty string, then
	 * it won't matter wether we return ALLOW or DENY here, as an empty string overrides
	 * the return value and is always MODEACTION_DENY if the mode is supposed to have
	 * a parameter.
	 */
	return MODEACTION_ALLOW;
}

void ModeChannelBan::RemoveMode(Channel* channel)
{
	BanList copy;
	char moderemove[MAXBUF];

	for (BanList::iterator i = channel->bans.begin(); i != channel->bans.end(); i++)
	{
		copy.push_back(*i);
	}

	for (BanList::iterator i = copy.begin(); i != copy.end(); i++)
	{
		sprintf(moderemove,"-%c",this->GetModeChar());
		const char* parameters[] = { channel->name, moderemove, i->data };
		ServerInstance->SendMode(parameters, 3, ServerInstance->FakeClient);
	}
}

void ModeChannelBan::RemoveMode(User*)
{
}

void ModeChannelBan::DisplayList(User* user, Channel* channel)
{
	/* Display the channel banlist */
	for (BanList::reverse_iterator i = channel->bans.rbegin(); i != channel->bans.rend(); ++i)
	{
		user->WriteServ("367 %s %s %s %s %d",user->nick, channel->name, i->data, i->set_by, i->set_time);
	}
	user->WriteServ("368 %s %s :End of channel ban list",user->nick, channel->name);
	return;
}

void ModeChannelBan::DisplayEmptyList(User* user, Channel* channel)
{
	user->WriteServ("368 %s %s :End of channel ban list",user->nick, channel->name);
}

std::string& ModeChannelBan::AddBan(User *user, std::string &dest, Channel *chan, int)
{
	if ((!user) || (!chan))
	{
		ServerInstance->Log(DEFAULT,"*** BUG *** AddBan was given an invalid parameter");
		dest = "";
		return dest;
	}

	/* Attempt to tidy the mask */
	ModeParser::CleanMask(dest);
	/* If the mask was invalid, we exit */
	if (dest == "")
		return dest;

	long maxbans = chan->GetMaxBans();
	if ((unsigned)chan->bans.size() > (unsigned)maxbans)
	{
		user->WriteServ("478 %s %s :Channel ban list for %s is full (maximum entries for this channel is %d)",user->nick, chan->name,chan->name,maxbans);
		dest = "";
		return dest;
	}

	int MOD_RESULT = 0;
	FOREACH_RESULT(I_OnAddBan,OnAddBan(user,chan,dest));
	if (MOD_RESULT)
	{
		dest = "";
		return dest;
	}

	for (BanList::iterator i = chan->bans.begin(); i != chan->bans.end(); i++)
	{
		if (!strcasecmp(i->data,dest.c_str()))
		{
			/* dont allow a user to set the same ban twice */
			dest = "";
			return dest;
		}
	}

	b.set_time = ServerInstance->Time();
	strlcpy(b.data,dest.c_str(),MAXBUF);
	if (*user->nick)
	{
		strlcpy(b.set_by,user->nick,NICKMAX-1);
	}
	else
	{
		strlcpy(b.set_by,ServerInstance->Config->ServerName,NICKMAX-1);
	}
	chan->bans.push_back(b);
	return dest;
}

ModePair ModeChannelBan::ModeSet(User*, User*, Channel* channel, const std::string &parameter)
{
	for (BanList::iterator i = channel->bans.begin(); i != channel->bans.end(); i++)
	{
		if (!strcasecmp(i->data,parameter.c_str()))
		{
			return std::make_pair(true, i->data);
		}
	}
        return std::make_pair(false, parameter);
}

std::string& ModeChannelBan::DelBan(User *user, std::string& dest, Channel *chan, int)
{
	if ((!user) || (!chan))
	{
		ServerInstance->Log(DEFAULT,"*** BUG *** TakeBan was given an invalid parameter");
		dest = "";
		return dest;
	}

	/* 'Clean' the mask, e.g. nick -> nick!*@* */
	ModeParser::CleanMask(dest);

	for (BanList::iterator i = chan->bans.begin(); i != chan->bans.end(); i++)
	{
		if (!strcasecmp(i->data,dest.c_str()))
		{
			int MOD_RESULT = 0;
			FOREACH_RESULT(I_OnDelBan,OnDelBan(user,chan,dest));
			if (MOD_RESULT)
			{
				dest = "";
				return dest;
			}
			chan->bans.erase(i);
			return dest;
		}
	}
	dest = "";
	return dest;
}

