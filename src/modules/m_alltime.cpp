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

/* $ModDesc: Display timestamps from all servers connected to the network */

class CommandAlltime : public Command
{
 public:
	CommandAlltime(InspIRCd *Instance) : Command(Instance, "ALLTIME", 'o', 0)
	{
		this->source = "m_alltime.so";
		syntax.clear();
		translation.push_back(TR_END);
	}

	CmdResult Handle(const char **parameters, int pcnt, User *user)
	{
		char fmtdate[64];
		char fmtdate2[64];
		time_t now = ServerInstance->Time(false);
		strftime(fmtdate, sizeof(fmtdate), "%F %T", gmtime(&now));
		now = ServerInstance->Time(true);
		strftime(fmtdate2, sizeof(fmtdate2), "%F %T", gmtime(&now));
		
		int delta = ServerInstance->GetTimeDelta();
		
		string msg = ":" + string(ServerInstance->Config->ServerName) + " NOTICE " + user->nick + " :System time for " +
			ServerInstance->Config->ServerName + " is: " + fmtdate + " (delta " + ConvToStr(delta) + " seconds): Time with delta: "+ fmtdate2;
		
		if (IS_LOCAL(user))
		{
			user->Write(msg);
		}
		else
		{
			deque<string> params;
			params.push_back(user->nick);
			params.push_back(msg);
			Event ev((char *) &params, NULL, "send_push");
			ev.Send(ServerInstance);
		}

		/* we want this routed out! */
		return CMD_SUCCESS;
	}
};


class Modulealltime : public Module
{
	CommandAlltime *mycommand;
 public:
	Modulealltime(InspIRCd *Me)
		: Module(Me)
	{
		mycommand = new CommandAlltime(ServerInstance);
		ServerInstance->AddCommand(mycommand);

	}
	
	virtual ~Modulealltime()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1, 0, 0, 0, VF_COMMON | VF_VENDOR, API_VERSION);
	}
	
};

MODULE_INIT(Modulealltime)
