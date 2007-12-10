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
#include "wildcard.h"

/* $ModDesc: Provides support for unreal-style oper-override */

typedef std::map<std::string,std::string> override_t;

class ModuleOverride : public Module
{
	
	override_t overrides;
	bool NoisyOverride;
	bool OverriddenMode;
	int OverOps, OverDeops, OverVoices, OverDevoices, OverHalfops, OverDehalfops;

 public:
 
	ModuleOverride(InspIRCd* Me)
		: Module(Me)
	{		
		// read our config options (main config file)
		OnRehash(NULL,"");
		ServerInstance->SNO->EnableSnomask('O',"OVERRIDE");
		OverriddenMode = false;
		OverOps = OverDeops = OverVoices = OverDevoices = OverHalfops = OverDehalfops = 0;
		Implementation eventlist[] = { I_OnRehash, I_OnAccessCheck, I_On005Numeric, I_OnUserPreJoin, I_OnUserPreKick, I_OnPostCommand };
		ServerInstance->Modules->Attach(eventlist, this, 6);
	}
	
	virtual void OnRehash(User* user, const std::string &parameter)
	{
		// on a rehash we delete our classes for good measure and create them again.
		ConfigReader* Conf = new ConfigReader(ServerInstance);
		
		// re-read our config options on a rehash
		NoisyOverride = Conf->ReadFlag("override","noisy",0);
		overrides.clear();
		for (int j =0; j < Conf->Enumerate("type"); j++)
		{
			std::string typen = Conf->ReadValue("type","name",j);
			std::string tokenlist = Conf->ReadValue("type","override",j);
			overrides[typen] = tokenlist;
		}
		
		delete Conf;
	}


	virtual void OnPostCommand(const std::string &command, const char** parameters, int pcnt, User *user, CmdResult result, const std::string &original_line)
	{
		if ((NoisyOverride) && (OverriddenMode) && (irc::string(command.c_str()) == "MODE") && (result == CMD_SUCCESS))
		{
			int Total = OverOps + OverDeops + OverVoices + OverDevoices + OverHalfops + OverDehalfops;

			ServerInstance->SNO->WriteToSnoMask('O',std::string(user->nick)+" Overriding modes: "+ServerInstance->Modes->GetLastParse()+" "+(Total ? "[Detail: " : "")+
					(OverOps ? ConvToStr(OverOps)+" op"+(OverOps != 1 ? "s" : "")+" " : "")+
					(OverDeops ? ConvToStr(OverDeops)+" deop"+(OverDeops != 1 ? "s" : "")+" " : "")+
					(OverVoices ? ConvToStr(OverVoices)+" voice"+(OverVoices != 1 ? "s" : "")+" " : "")+
					(OverDevoices ? ConvToStr(OverDevoices)+" devoice"+(OverDevoices != 1 ? "s" : "")+" " : "")+
					(OverHalfops ? ConvToStr(OverHalfops)+" halfop"+(OverHalfops != 1 ? "s" : "")+" " : "")+
					(OverDehalfops ? ConvToStr(OverDehalfops)+" dehalfop"+(OverDehalfops != 1 ? "s" : "") : "")
					+(Total ? "]" : ""));

			OverriddenMode = false;
			OverOps = OverDeops = OverVoices = OverDevoices = OverHalfops = OverDehalfops = 0;
		}
	}

	virtual void On005Numeric(std::string &output)
	{
		output.append(" OVERRIDE");
	}

	virtual bool CanOverride(User* source, const char* token)
	{
		// checks to see if the oper's type has <type:override>
		override_t::iterator j = overrides.find(source->oper);

		if (j != overrides.end())
		{
			// its defined or * is set, return its value as a boolean for if the token is set
			return ((j->second.find(token, 0) != std::string::npos) || (j->second.find("*", 0) != std::string::npos));
		}

		// its not defined at all, count as false
		return false;
	}

	virtual int OnUserPreKick(User* source, User* user, Channel* chan, const std::string &reason)
	{
		if (IS_OPER(source) && CanOverride(source,"KICK"))
		{
			if (((chan->GetStatus(source) == STATUS_HOP) && (chan->GetStatus(user) == STATUS_OP)) || (chan->GetStatus(source) < STATUS_VOICE))
			{
				ServerInstance->SNO->WriteToSnoMask('O',std::string(source->nick)+" Override-Kicked "+std::string(user->nick)+" on "+std::string(chan->name)+" ("+reason+")");
			}
			/* Returning -1 explicitly allows the kick */
			return -1;
		}
		return 0;
	}
	
	virtual int OnAccessCheck(User* source,User* dest,Channel* channel,int access_type)
	{
		if (IS_OPER(source))
		{
			if (source && channel)
			{
				// Fix by brain - allow the change if they arent on channel - rely on boolean short-circuit
				// to not check the other items in the statement if they arent on the channel
				int mode = channel->GetStatus(source);
				switch (access_type)
				{
					case AC_DEOP:
						if (CanOverride(source,"MODEDEOP"))
						{
							if (NoisyOverride)
							if ((!channel->HasUser(source)) || (mode < STATUS_OP))
								OverDeops++;
							return ACR_ALLOW;
						}
						else
						{
							return ACR_DEFAULT;
						}
					break;
					case AC_OP:
						if (CanOverride(source,"MODEOP"))
						{
							if (NoisyOverride)
							if ((!channel->HasUser(source)) || (mode < STATUS_OP))
								OverOps++;
							return ACR_ALLOW;
						}
						else
						{
							return ACR_DEFAULT;
						}
					break;
					case AC_VOICE:
						if (CanOverride(source,"MODEVOICE"))
						{
							if (NoisyOverride)
							if ((!channel->HasUser(source)) || (mode < STATUS_HOP))
								OverVoices++;
							return ACR_ALLOW;
						}
						else
						{
							return ACR_DEFAULT;
						}
					break;
					case AC_DEVOICE:
						if (CanOverride(source,"MODEDEVOICE"))
						{
							if (NoisyOverride)
							if ((!channel->HasUser(source)) || (mode < STATUS_HOP))
								OverDevoices++;
							return ACR_ALLOW;
						}
						else
						{
							return ACR_DEFAULT;
						}
					break;
					case AC_HALFOP:
						if (CanOverride(source,"MODEHALFOP"))
						{
							if (NoisyOverride)
							if ((!channel->HasUser(source)) || (mode < STATUS_OP))
								OverHalfops++;
							return ACR_ALLOW;
						}
						else
						{
							return ACR_DEFAULT;
						}
					break;
					case AC_DEHALFOP:
						if (CanOverride(source,"MODEDEHALFOP"))
						{
							if (NoisyOverride)
							if ((!channel->HasUser(source)) || (mode < STATUS_OP))
								OverDehalfops++;
							return ACR_ALLOW;
						}
						else
						{
							return ACR_DEFAULT;
						}
					break;
				}
			
				if (CanOverride(source,"OTHERMODE"))
				{
					if (NoisyOverride)
					if ((!channel->HasUser(source)) || (mode < STATUS_OP))
					{
						OverriddenMode = true;
						OverOps = OverDeops = OverVoices = OverDevoices = OverHalfops = OverDehalfops = 0;
					}
					return ACR_ALLOW;
				}
				else
				{
					return ACR_DEFAULT;
				}
			}
		}

		return ACR_DEFAULT;
	}
	
	virtual int OnUserPreJoin(User* user, Channel* chan, const char* cname, std::string &privs)
	{
		if (IS_OPER(user))
		{
			if (chan)
			{
				if ((chan->modes[CM_INVITEONLY]) && (CanOverride(user,"INVITE")))
				{
					irc::string x = chan->name;
					if (!user->IsInvited(x))
					{
						/* XXX - Ugly cast for a parameter that isn't used? :< - Om */
						if (NoisyOverride)
							chan->WriteChannelWithServ(ServerInstance->Config->ServerName, "NOTICE %s :%s used oper-override to bypass invite-only", cname, user->nick);
						ServerInstance->SNO->WriteToSnoMask('O',std::string(user->nick)+" used operoverride to bypass +i on "+std::string(cname));
					}
					return -1;
				}
				
				if ((*chan->key) && (CanOverride(user,"KEY")))
				{
					if (NoisyOverride)
						chan->WriteChannelWithServ(ServerInstance->Config->ServerName, "NOTICE %s :%s used oper-override to bypass the channel key", cname, user->nick);
					ServerInstance->SNO->WriteToSnoMask('O',std::string(user->nick)+" used operoverride to bypass +k on "+std::string(cname));
					return -1;
				}
					
				if ((chan->limit > 0) && (chan->GetUserCounter() >=  chan->limit) && (CanOverride(user,"LIMIT")))
				{
					if (NoisyOverride)
						chan->WriteChannelWithServ(ServerInstance->Config->ServerName, "NOTICE %s :%s used oper-override to bypass the channel limit", cname, user->nick);
					ServerInstance->SNO->WriteToSnoMask('O',std::string(user->nick)+" used operoverride to bypass +l on "+std::string(cname));
					return -1;
				}

				if (CanOverride(user,"BANWALK"))
				{
					if (chan->IsBanned(user))
					{
						if (NoisyOverride)
							chan->WriteChannelWithServ(ServerInstance->Config->ServerName, "NOTICE %s :%s used oper-override to bypass channel ban", cname, user->nick);
						ServerInstance->SNO->WriteToSnoMask('O',"%s used oper-override to bypass channel ban on %s", user->nick, cname);
					}
					return -1;
				}
			}
		}
		return 0;
	}
	
	virtual ~ModuleOverride()
	{
		ServerInstance->SNO->DisableSnomask('O');
	}
	
	virtual Version GetVersion()
	{
		return Version(1,1,0,1,VF_VENDOR,API_VERSION);
	}
};

MODULE_INIT(ModuleOverride)
