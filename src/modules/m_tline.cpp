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

/* $ModDesc: Provides /tline command used to test who a mask matches */

/** Handle /TLINE
 */ 
class CommandTline : public Command
{
 public:
	CommandTline (InspIRCd* Instance) : Command(Instance,"TLINE", 'o', 1)
	{
		this->source = "m_tline.so";
		this->syntax = "<mask>";
	}

	CmdResult Handle (const char** parameters, int pcnt, User *user)
	{
		float n_counted = 0;
		float n_matched = 0;
		float n_match_host = 0;
		float n_match_ip = 0;

		for (user_hash::const_iterator u = ServerInstance->clientlist->begin(); u != ServerInstance->clientlist->end(); u++)
		{
			n_counted++;
			if (match(u->second->GetFullRealHost(),parameters[0]))
			{
				n_matched++;
				n_match_host++;
			}
			else
			{
				char host[MAXBUF];
				snprintf(host, MAXBUF, "%s@%s", u->second->ident, u->second->GetIPString());
				if (match(host, parameters[0], true))
				{
					n_matched++;
					n_match_ip++;
				}
			}
		}
		if (n_matched)
			user->WriteServ( "NOTICE %s :*** TLINE: Counted %0.0f user(s). Matched '%s' against %0.0f user(s) (%0.2f%% of the userbase). %0.0f by hostname and %0.0f by IP address.",user->nick, n_counted, parameters[0], n_matched, (n_matched/n_counted)*100, n_match_host, n_match_ip);
		else
			user->WriteServ( "NOTICE %s :*** TLINE: Counted %0.0f user(s). Matched '%s' against no user(s).", user->nick, n_counted, parameters[0]);

		return CMD_LOCALONLY;			
	}
};

class ModuleTLine : public Module
{
	CommandTline* newcommand;
 public:
	ModuleTLine(InspIRCd* Me)
		: Module(Me)
	{
		
		newcommand = new CommandTline(ServerInstance);
		ServerInstance->AddCommand(newcommand);

	}


	virtual ~ModuleTLine()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1, 1, 0, 0, VF_VENDOR,API_VERSION);
	}
};

MODULE_INIT(ModuleTLine)

