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

/* $ModDesc: Provides support for the /SILENCE command */

// This typedef holds a silence list. Each user may or may not have a
// silencelist, if a silence list is empty for a user, he/she does not
// have one of these structures associated with their user record.
typedef std::map<irc::string, time_t> silencelist;

class CommandSilence : public Command
{
	unsigned int& maxsilence;
 public:
	CommandSilence (InspIRCd* Instance, unsigned int &max) : Command(Instance,"SILENCE", 0, 0), maxsilence(max)
	{
		this->source = "m_silence.so";
		syntax = "{[+|-]<mask>}";
		TRANSLATE2(TR_TEXT, TR_END);
	}

	CmdResult Handle (const char** parameters, int pcnt, User *user)
	{
		if (!pcnt)
		{
			// no parameters, show the current silence list.
			// Use Extensible::GetExt to fetch the silence list
			silencelist* sl;
			user->GetExt("silence_list", sl);
			// if the user has a silence list associated with their user record, show it
			if (sl)
			{
				for (silencelist::const_iterator c = sl->begin(); c != sl->end(); c++)
				{
					user->WriteServ("271 %s %s %s :%lu",user->nick, user->nick, c->first.c_str(), (unsigned long)c->second);
				}
			}
			user->WriteServ("272 %s :End of Silence List",user->nick);

			return CMD_SUCCESS;
		}
		else if (pcnt > 0)
		{
			// one or more parameters, add or delete entry from the list (only the first parameter is used)
			std::string mask = parameters[0] + 1;
			char action = *parameters[0];
			
			if (!mask.length())
			{
				// 'SILENCE +' or 'SILENCE -', assume *!*@*
				mask = "*!*@*";
			}
			
			ModeParser::CleanMask(mask);

			if (action == '-')
			{
				// fetch their silence list
				silencelist* sl;
				user->GetExt("silence_list", sl);
				// does it contain any entries and does it exist?
				if (sl)
				{
					silencelist::iterator i = sl->find(mask.c_str());
					if (i != sl->end())
					{
						sl->erase(i);
						user->WriteServ("950 %s %s :Removed %s from silence list",user->nick, user->nick, mask.c_str());
						if (!sl->size())
						{
							// tidy up -- if a user's list is empty, theres no use having it
							// hanging around in the user record.
							delete sl;
							user->Shrink("silence_list");
						}
					}
					else
						user->WriteServ("952 %s %s :%s does not exist on your silence list",user->nick, user->nick, mask.c_str());
				}
			}
			else if (action == '+')
			{
				// fetch the user's current silence list
				silencelist* sl;
				user->GetExt("silence_list", sl);
				// what, they dont have one??? WE'RE ALL GONNA DIE! ...no, we just create an empty one.
				if (!sl)
				{
					sl = new silencelist;
					user->Extend("silence_list", sl);
				}
				silencelist::iterator n = sl->find(mask.c_str());
				if (n != sl->end())
				{
					user->WriteServ("952 %s %s :%s is already on your silence list",user->nick, user->nick, mask.c_str());
					return CMD_FAILURE;
				}
				if (sl->size() >= maxsilence)
				{
					user->WriteServ("952 %s %s :Your silence list is full",user->nick, user->nick, mask.c_str());
					return CMD_FAILURE;
				}
				sl->insert(std::make_pair<irc::string, time_t>(mask.c_str(), ServerInstance->Time()));
				user->WriteServ("951 %s %s :Added %s to silence list",user->nick, user->nick, mask.c_str());
				return CMD_SUCCESS;
			}
		}
		return CMD_SUCCESS;
	}
};

class ModuleSilence : public Module
{
	
	CommandSilence* mycommand;
	unsigned int maxsilence;
 public:
 
	ModuleSilence(InspIRCd* Me)
		: Module(Me), maxsilence(32)
	{
		OnRehash(NULL, "");
		mycommand = new CommandSilence(ServerInstance, maxsilence);
		ServerInstance->AddCommand(mycommand);
		Implementation eventlist[] = { I_OnRehash, I_OnUserQuit, I_On005Numeric, I_OnUserPreNotice, I_OnUserPreMessage };
		ServerInstance->Modules->Attach(eventlist, this, 5);
	}


	virtual void OnRehash(User* user, const std::string &parameter)
	{
		ConfigReader Conf(ServerInstance);
		maxsilence = Conf.ReadInteger("silence", "maxentries", 0, true);
		if (!maxsilence)
			maxsilence = 32;
	}

	virtual void OnUserQuit(User* user, const std::string &reason, const std::string &oper_message)
	{
		// when the user quits tidy up any silence list they might have just to keep things tidy
		// and to prevent a HONKING BIG MEMORY LEAK!
		silencelist* sl;
		user->GetExt("silence_list", sl);
		if (sl)
		{
			delete sl;
			user->Shrink("silence_list");
		}
	}

	virtual void On005Numeric(std::string &output)
	{
		// we don't really have a limit...
		output = output + " SILENCE=" + ConvToStr(maxsilence);
	}
	
	virtual int OnUserPreNotice(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		// im not sure how unreal's silence operates but ours is sensible. It blocks notices and
		// privmsgs from people on the silence list, directed privately at the user.
		// channel messages are unaffected (ever tried to follow the flow of conversation in
		// a channel when you've set an ignore on the two most talkative people?)
		if ((target_type == TYPE_USER) && (IS_LOCAL(user)))
		{
			User* u = (User*)dest;
			silencelist* sl;
			u->GetExt("silence_list", sl);
			if (sl)
			{
				for (silencelist::const_iterator c = sl->begin(); c != sl->end(); c++)
				{
					if (match(user->GetFullHost(), c->first.c_str()))
					{
						return 1;
					}
				}
			}
		}
		return 0;
	}

	virtual int OnUserPreMessage(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		return OnUserPreNotice(user,dest,target_type,text,status,exempt_list);
	}

	virtual ~ModuleSilence()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1, 1, 0, 0, VF_COMMON | VF_VENDOR, API_VERSION);
	}
};

MODULE_INIT(ModuleSilence)
