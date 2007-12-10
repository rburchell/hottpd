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

/* $ModDesc: Gives /cban, aka C:lines. Think Q:lines, for channels. */

/** Holds a CBAN item
 */
class CBan : public classbase
{
public:
	irc::string chname;
	std::string set_by;
	time_t set_on;
	long length;
	std::string reason;

	CBan()
	{
	}

	CBan(irc::string cn, std::string sb, time_t so, long ln, std::string rs) : chname(cn), set_by(sb), set_on(so), length(ln), reason(rs)
	{
	}
};

bool CBanComp(const CBan &ban1, const CBan &ban2);

typedef std::vector<CBan> cbanlist;

/* cbans is declared here, as our type is right above. Don't try move it. */
cbanlist cbans;

/** Handle /CBAN
 */
class CommandCban : public Command
{
 public:
	CommandCban(InspIRCd* Me) : Command(Me, "CBAN", 'o', 1)
	{
		this->source = "m_cban.so";
		this->syntax = "<channel> [<duration> :<reason>]";
		TRANSLATE4(TR_TEXT,TR_TEXT,TR_TEXT,TR_END);
	}

	CmdResult Handle(const char** parameters, int pcnt, User *user)
	{
		/* syntax: CBAN #channel time :reason goes here */
		/* 'time' is a human-readable timestring, like 2d3h2s. */

		if(pcnt == 1)
		{
			/* form: CBAN #channel removes a CBAN */
			for (cbanlist::iterator iter = cbans.begin(); iter != cbans.end(); iter++)
			{
				if (parameters[0] == iter->chname)
				{
					long remaining = iter->length + ServerInstance->Time();
					user->WriteServ("386 %s %s :Removed CBAN due to expire at %s (%s)", user->nick, iter->chname.c_str(), ServerInstance->TimeString(remaining).c_str(), iter->reason.c_str());
					cbans.erase(iter);
					break;
				}
			}
		}
		else if (pcnt >= 2)
		{
			/* full form to add a CBAN */
			if (ServerInstance->IsChannel(parameters[0]))
			{
				// parameters[0] = #channel
				// parameters[1] = 1h3m2s
				// parameters[2] = Tortoise abuser
				long length = ServerInstance->Duration(parameters[1]);
				std::string reason = (pcnt > 2) ? parameters[2] : "No reason supplied";
				
				cbans.push_back(CBan(parameters[0], user->nick, ServerInstance->Time(), length, reason));
					
				std::sort(cbans.begin(), cbans.end(), CBanComp);
				
				if(length > 0)
				{
					user->WriteServ("385 %s %s :Added %lu second channel ban (%s)", user->nick, parameters[0], length, reason.c_str());
					ServerInstance->WriteOpers("*** %s added %lu second channel ban on %s (%s)", user->nick, length, parameters[0], reason.c_str());
				}
				else
				{
					user->WriteServ("385 %s %s :Added permanent channel ban (%s)", user->nick, parameters[0], reason.c_str());
					ServerInstance->WriteOpers("*** %s added permanent channel ban on %s (%s)", user->nick, parameters[0], reason.c_str());
				}
			}
			else
			{
				user->WriteServ("403 %s %s :Invalid channel name", user->nick, parameters[0]);
				return CMD_FAILURE;
			}
		}

		/* we want this routed! */
		return CMD_SUCCESS;
	}
};

bool CBanComp(const CBan &ban1, const CBan &ban2)
{
	return ((ban1.set_on + ban1.length) < (ban2.set_on + ban2.length));
}

class ModuleCBan : public Module
{
	CommandCban* mycommand;
	

 public:
	ModuleCBan(InspIRCd* Me) : Module(Me)
	{
		
		mycommand = new CommandCban(Me);
		ServerInstance->AddCommand(mycommand);
		Implementation eventlist[] = { I_OnUserPreJoin, I_OnSyncOtherMetaData, I_OnDecodeMetaData, I_OnStats };
		ServerInstance->Modules->Attach(eventlist, this, 4);
	}

	
	virtual int OnStats(char symbol, User* user, string_list &results)
	{
		ExpireBans();
	
		if(symbol == 'C')
		{
			for(cbanlist::iterator iter = cbans.begin(); iter != cbans.end(); iter++)
			{
				unsigned long remaining = (iter->set_on + iter->length) - ServerInstance->Time();
				results.push_back(std::string(ServerInstance->Config->ServerName)+" 210 "+user->nick+" "+iter->chname.c_str()+" "+iter->set_by+" "+ConvToStr(iter->set_on)+" "+ConvToStr(iter->length)+" "+ConvToStr(remaining)+" :"+iter->reason);
			}
		}
		
		return 0;
	}

	virtual int OnUserPreJoin(User *user, Channel *chan, const char *cname, std::string &privs)
	{
		ExpireBans();
	
		/* check cbans in here, and apply as necessary. */
		for(cbanlist::iterator iter = cbans.begin(); iter != cbans.end(); iter++)
		{
			if(iter->chname == cname && !user->modes[UM_OPERATOR])
			{
				// Channel is banned.
				user->WriteServ( "384 %s %s :Cannot join channel, CBANed (%s)", user->nick, cname, iter->reason.c_str());
				ServerInstance->WriteOpers("*** %s tried to join %s which is CBANed (%s)", user->nick, cname, iter->reason.c_str());
				return 1;
			}
		}
		return 0;
	}
	
	virtual void OnSyncOtherMetaData(Module* proto, void* opaque, bool displayable)
	{
		for(cbanlist::iterator iter = cbans.begin(); iter != cbans.end(); iter++)
		{
			proto->ProtoSendMetaData(opaque, TYPE_OTHER, NULL, "cban", EncodeCBan(*iter));
		}
	}
	
	virtual void OnDecodeMetaData(int target_type, void* target, const std::string &extname, const std::string &extdata)
	{
		if((target_type == TYPE_OTHER) && (extname == "cban"))
		{
			cbans.push_back(DecodeCBan(extdata));
			std::sort(cbans.begin(), cbans.end(), CBanComp);
		}
	}

	virtual ~ModuleCBan()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1, 1, 0, 1, VF_COMMON | VF_VENDOR, API_VERSION);
	}

	std::string EncodeCBan(const CBan &ban)
	{
		std::ostringstream stream;	
		stream << ban.chname << " " << ban.set_by << " " << ban.set_on << " " << ban.length << " :" << ban.reason;
		return stream.str();
	}

	CBan DecodeCBan(const std::string &data)
	{
		CBan res;
		int set_on;
		irc::tokenstream tokens(data);
		tokens.GetToken(res.chname);
		tokens.GetToken(res.set_by);
		tokens.GetToken(set_on);
		res.set_on = set_on;
		tokens.GetToken(res.length);
		tokens.GetToken(res.reason);
		return res;
	}

	void ExpireBans()
	{
		bool go_again = true;

		while (go_again)
		{
			go_again = false;
	
			for (cbanlist::iterator iter = cbans.begin(); iter != cbans.end(); iter++)
			{
				/* 0 == permanent, don't mess with them! -- w00t */
				if (iter->length != 0)
				{
					if (iter->set_on + iter->length <= ServerInstance->Time())
					{
						ServerInstance->WriteOpers("*** %li second CBAN on %s (%s) set on %s expired", iter->length, iter->chname.c_str(), iter->reason.c_str(), ServerInstance->TimeString(iter->set_on).c_str());
						cbans.erase(iter);
						go_again = true;
					}
				}
	
				if (go_again == true)
					break;
			}
		}
	}
};

MODULE_INIT(ModuleCBan)

