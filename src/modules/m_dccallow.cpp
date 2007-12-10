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

/* $ModDesc: Povides support for the /DCCALLOW command */

static ConfigReader *Conf;

class BannedFileList
{
 public:
	std::string filemask;
	std::string action;
};

class DCCAllow
{
 public:
	std::string nickname;
	std::string hostmask;
	time_t set_on;
	long length;

	DCCAllow() { }

	DCCAllow(const std::string &nick, const std::string &hm, const time_t so, const long ln) : nickname(nick), hostmask(hm), set_on(so), length(ln) { }
};

typedef std::vector<User *> userlist;
userlist ul;
typedef std::vector<DCCAllow> dccallowlist;
dccallowlist* dl;
typedef std::vector<BannedFileList> bannedfilelist;
bannedfilelist bfl;

class CommandDccallow : public Command
{
 public:
	CommandDccallow(InspIRCd* Me) : Command(Me, "DCCALLOW", 0, 0)
	{
		this->source = "m_dccallow.so";
		syntax = "{[+|-]<nick> <time>|HELP|LIST}";
		/* XXX we need to fix this so it can work with translation stuff (i.e. move +- into a seperate param */
	}

	CmdResult Handle(const char **parameters, int pcnt, User *user)
	{
		/* syntax: DCCALLOW [+|-]<nick> (<time>) */
		if (!pcnt)
		{
			// display current DCCALLOW list
			DisplayDCCAllowList(user);
			return CMD_FAILURE;
		}
		else if (pcnt > 0)
		{
			char action = *parameters[0];
		
			// if they didn't specify an action, this is probably a command
			if (action != '+' && action != '-')
			{
				if (!strcasecmp(parameters[0], "LIST"))
				{
					// list current DCCALLOW list
					DisplayDCCAllowList(user);
					return CMD_FAILURE;
				} 
				else if (!strcasecmp(parameters[0], "HELP"))
				{
					// display help
					DisplayHelp(user);
					return CMD_FAILURE;
				}
			}
			
			std::string nick = parameters[0] + 1;
			User *target = ServerInstance->FindNick(nick);
	
			if (target)
			{
				
				if (action == '-')
				{
					user->GetExt("dccallow_list", dl);
					// check if it contains any entries
					if (dl)
					{
						if (dl->size())
						{
							for (dccallowlist::iterator i = dl->begin(); i != dl->end(); ++i)
							{
								// search through list
								if (i->nickname == target->nick)
								{
									dl->erase(i);
									user->WriteServ("995 %s %s :Removed %s from your DCCALLOW list", user->nick, user->nick, target->nick);
									break;
								}
							}
						}
					}
					else
					{
						delete  dl;
						user->Shrink("dccallow_list");
				
						// remove from userlist
						for (userlist::iterator j = ul.begin(); j != ul.end(); ++j)
						{
							User* u = (User*)(*j);
							if (u == user)
							{
								ul.erase(j);
								break;
							}
						}
					}
				}
				else if (action == '+')
				{
					// fetch current DCCALLOW list
					user->GetExt("dccallow_list", dl);
					// they don't have one, create it
					if (!dl)
					{
						dl = new dccallowlist;
						user->Extend("dccallow_list", dl);
						// add this user to the userlist
						ul.push_back(user);
					}
					for (dccallowlist::const_iterator k = dl->begin(); k != dl->end(); ++k)
					{
						if (k->nickname == target->nick)
						{
							user->WriteServ("996 %s %s :%s is already on your DCCALLOW list", user->nick, user->nick, target->nick);
							return CMD_FAILURE;
						}
						else if (ServerInstance->MatchText(user->GetFullHost(), k->hostmask))
						{
							user->WriteServ("996 %s %s :You cannot add yourself to your own DCCALLOW list!", user->nick, user->nick);
							return CMD_FAILURE;
						}
					}
				
					std::string mask = std::string(target->nick)+"!"+std::string(target->ident)+"@"+std::string(target->dhost);
					std::string default_length = Conf->ReadValue("dccallow", "length", 0);
		
					long length;
					if (pcnt < 2)
					{
						length = ServerInstance->Duration(default_length);
					} 
					else if (!atoi(parameters[1]))
					{
						length = 0;
					}
					else
					{
						length = ServerInstance->Duration(parameters[1]);
					}
	
					if (!ServerInstance->IsValidMask(mask.c_str()))
					{
						return CMD_FAILURE;
					}
			
					dl->push_back(DCCAllow(target->nick, mask, ServerInstance->Time(), length));
			
					if (length > 0)
					{
						user->WriteServ("993 %s %s :Added %s to DCCALLOW list for %d seconds", user->nick, user->nick, target->nick, length);
					}
					else
					{
						user->WriteServ("994 %s %s :Added %s to DCCALLOW list for this session", user->nick, user->nick, target->nick);
					}

					/* route it. */
					return CMD_SUCCESS;
				}
			}
			else
			{
				// nick doesn't exist
				user->WriteServ("401 %s %s :No such nick/channel", user->nick, nick.c_str());
				return CMD_FAILURE;
			}
		}
		return CMD_FAILURE;
	}

	void DisplayHelp(User* user)
	{
		user->WriteServ("998 %s :DCCALLOW [<+|->nick [time]] [list] [help]", user->nick);
		user->WriteServ("998 %s :You may allow DCCs from specific users by specifying a", user->nick);
		user->WriteServ("998 %s :DCC allow for the user you want to receive DCCs from.", user->nick);
		user->WriteServ("998 %s :For example, to allow the user Brain to send you inspircd.exe", user->nick);
		user->WriteServ("998 %s :you would type:", user->nick);
		user->WriteServ("998 %s :/DCCALLOW +Brain", user->nick);
		user->WriteServ("998 %s :Brain would then be able to send you files. They would have to", user->nick);
		user->WriteServ("998 %s :resend the file again if the server gave them an error message", user->nick);
		user->WriteServ("998 %s :before you added them to your DCCALLOW list.", user->nick);
		user->WriteServ("998 %s :DCCALLOW entries will be temporary by default, if you want to add", user->nick);
		user->WriteServ("998 %s :them to your DCCALLOW list until you leave IRC, type:", user->nick);
		user->WriteServ("998 %s :/DCCALLOW +Brain 0", user->nick);
		user->WriteServ("998 %s :To remove the user from your DCCALLOW list, type:", user->nick);
		user->WriteServ("998 %s :/DCCALLOW -Brain", user->nick);
		user->WriteServ("998 %s :To see the users in your DCCALLOW list, type:", user->nick);
		user->WriteServ("998 %s :/DCCALLOW LIST", user->nick);
		user->WriteServ("998 %s :NOTE: If the user leaves IRC or changes their nickname", user->nick);
		user->WriteServ("998 %s :  they will be removed from your DCCALLOW list.", user->nick);
		user->WriteServ("998 %s :  your DCCALLOW list will be deleted when you leave IRC.", user->nick);
		user->WriteServ("999 %s :End of DCCALLOW HELP", user->nick);
	}
	
	void DisplayDCCAllowList(User* user)
	{
		 // display current DCCALLOW list
		user->WriteServ("990 %s :Users on your DCCALLOW list:", user->nick);
		user->GetExt("dccallow_list", dl);
		
		if (dl)
		{
			for (dccallowlist::const_iterator c = dl->begin(); c != dl->end(); ++c)
			{
				user->WriteServ("991 %s %s :%s (%s)", user->nick, user->nick, c->nickname.c_str(), c->hostmask.c_str());
			}
		}
		
		user->WriteServ("992 %s :End of DCCALLOW list", user->nick);
	}			

};
	
class ModuleDCCAllow : public Module
{
	CommandDccallow* mycommand;
 public:

	ModuleDCCAllow(InspIRCd* Me)
		: Module(Me)
	{
		Conf = new ConfigReader(ServerInstance);
		mycommand = new CommandDccallow(ServerInstance);
		ServerInstance->AddCommand(mycommand);
		ReadFileConf();
		Implementation eventlist[] = { I_OnUserPreMessage, I_OnUserPreNotice, I_OnUserQuit, I_OnUserPreNick, I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, 5);
	}


	virtual void OnRehash(User* user, const std::string &parameter)
	{
		delete Conf;
		Conf = new ConfigReader(ServerInstance);
	}

	virtual void OnUserQuit(User* user, const std::string &reason, const std::string &oper_message)
	{
		dccallowlist* dl;
	
		// remove their DCCALLOW list if they have one
		user->GetExt("dccallow_list", dl);
		if (dl)
		{
			delete dl;
			user->Shrink("dccallow_list");
			RemoveFromUserlist(user);
		}
		
		// remove them from any DCCALLOW lists
		// they are currently on
		RemoveNick(user);
	}


	virtual int OnUserPreNick(User* user, const std::string &newnick)
	{
		RemoveNick(user);
		return 0;
	}

	virtual int OnUserPreMessage(User* user, void* dest, int target_type, std::string &text, char status, CUList &exempt_list)
	{
		return OnUserPreNotice(user, dest, target_type, text, status, exempt_list);
	}

	virtual int OnUserPreNotice(User* user, void* dest, int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if (!IS_LOCAL(user))
			return 0;

		if (target_type == TYPE_USER)
		{
			User* u = (User*)dest;

			/* Always allow a user to dcc themselves (although... why?) */
			if (user == u)
				return 0;
		
			if ((text.length()) && (text[0] == '\1'))
			{
				Expire();

				// :jamie!jamie@test-D4457903BA652E0F.silverdream.org PRIVMSG eimaj :DCC SEND m_dnsbl.cpp 3232235786 52650 9676
				// :jamie!jamie@test-D4457903BA652E0F.silverdream.org PRIVMSG eimaj :VERSION
					
				if (strncmp(text.c_str(), "\1DCC ", 5) == 0)
				{
					u->GetExt("dccallow_list", dl);
		
					if (dl && dl->size())
					{
						for (dccallowlist::const_iterator iter = dl->begin(); iter != dl->end(); ++iter)
							if (ServerInstance->MatchText(user->GetFullHost(), iter->hostmask))
								return 0;
					}
		
					// tokenize
					std::stringstream ss(text);
					std::string buf;
					std::vector<std::string> tokens;
		
					while (ss >> buf)
						tokens.push_back(buf);
		
					irc::string type = tokens[1].c_str();
		
					bool blockchat = Conf->ReadFlag("dccallow", "blockchat", 0);
		
					if (type == "SEND")
					{
						std::string defaultaction = Conf->ReadValue("dccallow", "action", 0);
						std::string filename = tokens[2];
					
						if (defaultaction == "allow") 
							return 0;
				
						for (unsigned int i = 0; i < bfl.size(); i++)
						{
							if (ServerInstance->MatchText(filename, bfl[i].filemask))
							{
								if (bfl[i].action == "allow")
									return 0;
							}
							else
							{
								if (defaultaction == "allow")
									return 0;
							}
							user->WriteServ("NOTICE %s :The user %s is not accepting DCC SENDs from you. Your file %s was not sent.", user->nick, u->nick, filename.c_str());
							u->WriteServ("NOTICE %s :%s (%s@%s) attempted to send you a file named %s, which was blocked.", u->nick, user->nick, user->ident, user->dhost, filename.c_str());
							u->WriteServ("NOTICE %s :If you trust %s and were expecting this, you can type /DCCALLOW HELP for information on the DCCALLOW system.", u->nick, user->nick);
							return 1;
						}
					}
					else if ((type == "CHAT") && (blockchat))
					{
						user->WriteServ("NOTICE %s :The user %s is not accepting DCC CHAT requests from you.", user->nick, u->nick);
						u->WriteServ("NOTICE %s :%s (%s@%s) attempted to initiate a DCC CHAT session, which was blocked.", u->nick, user->nick, user->ident, user->dhost);
						u->WriteServ("NOTICE %s :If you trust %s and were expecting this, you can type /DCCALLOW HELP for information on the DCCALLOW system.", u->nick, user->nick);
						return 1;
					}
				}
			}
		}
		return 0;
	}
	
	void Expire()
	{
		for (userlist::iterator iter = ul.begin(); iter != ul.end(); ++iter)
		{
			User* u = (User*)(*iter);
			u->GetExt("dccallow_list", dl);
	
			if (dl)
			{
				if (dl->size())
				{
					dccallowlist::iterator iter = dl->begin();
					while (iter != dl->end())
					{
						if ((iter->set_on + iter->length) <= ServerInstance->Time())
						{
							u->WriteServ("997 %s %s :DCCALLOW entry for %s has expired", u->nick, u->nick, iter->nickname.c_str());
							iter = dl->erase(iter);
						}
						else
						{
							++iter;
						}
					}
				}
			}
			else
			{
				RemoveFromUserlist(u);
			}
		}
	}
	
	void RemoveNick(User* user)
	{
		/* Iterate through all DCCALLOW lists and remove user */
		for (userlist::iterator iter = ul.begin(); iter != ul.end(); ++iter)
		{
			User *u = (User*)(*iter);
			u->GetExt("dccallow_list", dl);
	
			if (dl)
			{
				if (dl->size())
				{
					for (dccallowlist::iterator i = dl->begin(); i != dl->end(); ++i)
					{
						if (i->nickname == user->nick)
						{
					
							u->WriteServ("NOTICE %s :%s left the network or changed their nickname and has been removed from your DCCALLOW list", u->nick, i->nickname.c_str());
							u->WriteServ("995 %s %s :Removed %s from your DCCALLOW list", u->nick, u->nick, i->nickname.c_str());
							dl->erase(i);
							break;
						}
					}
				}
			}
			else
			{
				RemoveFromUserlist(u);
			}
		}
	}

	void RemoveFromUserlist(User *user)
	{
		// remove user from userlist
		for (userlist::iterator j = ul.begin(); j != ul.end(); ++j)
		{
			User* u = (User*)(*j);
			if (u == user)
			{
				ul.erase(j);
				break;
			}
		}
	}

	void ReadFileConf()
	{
		bfl.clear();
		for (int i = 0; i < Conf->Enumerate("banfile"); i++)
		{
			BannedFileList bf;
			std::string fileglob = Conf->ReadValue("banfile", "pattern", i);
			std::string action = Conf->ReadValue("banfile", "action", i);
			bf.filemask = fileglob;
			bf.action = action;
			bfl.push_back(bf);
		}
	
	}

	virtual ~ModuleDCCAllow()
	{
	}

	virtual Version GetVersion()
	{
		return Version(1, 1, 0, 0, VF_COMMON | VF_VENDOR, API_VERSION);
	}
};

MODULE_INIT(ModuleDCCAllow)
