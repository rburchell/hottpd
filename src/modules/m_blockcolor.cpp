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

/* $ModDesc: Provides support for unreal-style channel mode +c */

/** Handles the +c channel mode
 */
class BlockColor : public ModeHandler
{
 public:
	BlockColor(InspIRCd* Instance) : ModeHandler(Instance, 'c', 0, 0, false, MODETYPE_CHANNEL, false) { }

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			if (!channel->IsModeSet('c'))
			{
				channel->SetMode('c',true);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (channel->IsModeSet('c'))
			{
				channel->SetMode('c',false);
				return MODEACTION_ALLOW;
			}
		}

		return MODEACTION_DENY;
	}
};

class ModuleBlockColour : public Module
{
	bool AllowChanOps;	
	BlockColor *bc;
 public:
 
	ModuleBlockColour(InspIRCd* Me) : Module(Me)
	{
		bc = new BlockColor(ServerInstance);
		if (!ServerInstance->AddMode(bc))
			throw ModuleException("Could not add new modes!");
		Implementation eventlist[] = { I_OnUserPreMessage, I_OnUserPreNotice };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}



	virtual int OnUserPreMessage(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if ((target_type == TYPE_CHANNEL) && (IS_LOCAL(user)))
		{
			Channel* c = (Channel*)dest;
			
			if(c->IsModeSet('c'))
			{
				if (!CHANOPS_EXEMPT(ServerInstance, 'c') || CHANOPS_EXEMPT(ServerInstance, 'c') && c->GetStatus(user) != STATUS_OP)
				{
					for (std::string::iterator i = text.begin(); i != text.end(); i++)
					{
						switch (*i)
						{
							case 2:
							case 3:
							case 15:
							case 21:
							case 22:
							case 31:
								user->WriteServ("404 %s %s :Can't send colours to channel (+c set)",user->nick, c->name);
								return 1;
							break;
						}
					}
				}
			}
		}
		return 0;
	}
	
	virtual int OnUserPreNotice(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		return OnUserPreMessage(user,dest,target_type,text,status,exempt_list);
	}

	virtual ~ModuleBlockColour()
	{
		ServerInstance->Modes->DelMode(bc);
		delete bc;
	}
	
	virtual Version GetVersion()
	{
		return Version(1,1,0,0,VF_COMMON|VF_VENDOR,API_VERSION);
	}
};

MODULE_INIT(ModuleBlockColour)
