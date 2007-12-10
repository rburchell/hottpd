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

/* $ModDesc: Provides the /check command to retrieve information on a user, channel, or IP address */

/** Handle /CHECK
 */
class CommandCheck : public Command
{
 public:
 	CommandCheck (InspIRCd* Instance) : Command(Instance,"CHECK", 'o', 1)
	{
		this->source = "m_check.so";
		syntax = "<nickname>|<ip>|<hostmask>|<channel>";
	}

	CmdResult Handle (const char** parameters, int pcnt, User *user)
	{
		User *targuser;
		Channel *targchan;
		std::string checkstr;
		std::string chliststr;

		char timebuf[60];
		struct tm *mytime;


		checkstr = "304 " + std::string(user->nick) + " :CHECK";

		targuser = ServerInstance->FindNick(parameters[0]);
		targchan = ServerInstance->FindChan(parameters[0]);

		/*
		 * Syntax of a /check reply:
		 *  :server.name 304 target :CHECK START <target>
		 *  :server.name 304 target :CHECK <field> <value>
		 *  :server.name 304 target :CHECK END
		 */

		user->WriteServ(checkstr + " START " + parameters[0]);

		if (targuser)
		{
			/* /check on a user */
			user->WriteServ(checkstr + " nuh " + targuser->GetFullHost());
			user->WriteServ(checkstr + " realnuh " + targuser->GetFullRealHost());
			user->WriteServ(checkstr + " realname " + targuser->fullname);
			user->WriteServ(checkstr + " modes +" + targuser->FormatModes());
			user->WriteServ(checkstr + " snomasks +" + targuser->FormatNoticeMasks());
			user->WriteServ(checkstr + " server " + targuser->server);

			if (IS_AWAY(targuser))
			{
				/* user is away */
				user->WriteServ(checkstr + " awaymsg " + targuser->awaymsg);
			}

			if (IS_OPER(targuser))
			{
				/* user is an oper of type ____ */
				user->WriteServ(checkstr + " opertype " + irc::Spacify(targuser->oper));
			}

			if (IS_LOCAL(targuser))
			{
				/* port information is only held for a local user! */
				user->WriteServ(checkstr + " onport " + ConvToStr(targuser->GetPort()));
			}

			chliststr = targuser->ChannelList(targuser);
			std::stringstream dump(chliststr);

			ServerInstance->DumpText(user,checkstr + " onchans ", dump);
		}
		else if (targchan)
		{
			/* /check on a channel */
			time_t creation_time = targchan->created;
			time_t topic_time = targchan->topicset;

			mytime = gmtime(&creation_time);
			strftime(timebuf, 59, "%Y/%m/%d - %H:%M:%S", mytime);
			user->WriteServ(checkstr + " created " + timebuf);

			if (targchan->topic[0] != 0)
			{
				/* there is a topic, assume topic related information exists */
				user->WriteServ(checkstr + " topic " + targchan->topic);
				user->WriteServ(checkstr + " topic_setby " + targchan->setby);
				mytime = gmtime(&topic_time);
				strftime(timebuf, 59, "%Y/%m/%d - %H:%M:%S", mytime);
				user->WriteServ(checkstr + " topic_setat " + timebuf);
			}

			user->WriteServ(checkstr + " modes " + targchan->ChanModes(true));
			user->WriteServ(checkstr + " membercount " + ConvToStr(targchan->GetUserCounter()));
			
			/* now the ugly bit, spool current members of a channel. :| */

			CUList *ulist= targchan->GetUsers();

			/* note that unlike /names, we do NOT check +i vs in the channel */
			for (CUList::iterator i = ulist->begin(); i != ulist->end(); i++)
			{
				char tmpbuf[MAXBUF];
				/*
				 * Unlike Asuka, I define a clone as coming from the same host. --w00t
				 */
				snprintf(tmpbuf, MAXBUF, "%lu    %s%s (%s@%s) %s ", i->first->GlobalCloneCount(), targchan->GetAllPrefixChars(i->first), i->first->nick, i->first->ident, i->first->dhost, i->first->fullname);
				user->WriteServ(checkstr + " member " + tmpbuf);
			}
		}
		else
		{
			/*  /check on an IP address, or something that doesn't exist */
			long x = 0;

			/* hostname or other */
			for (user_hash::const_iterator a = ServerInstance->clientlist->begin(); a != ServerInstance->clientlist->end(); a++)
			{
				if (match(a->second->host, parameters[0]) || match(a->second->dhost, parameters[0]))
				{
					/* host or vhost matches mask */
					user->WriteServ(checkstr + " match " + ConvToStr(++x) + " " + a->second->GetFullRealHost());
				}
				/* IP address */
				else if (match(a->second->GetIPString(), parameters[0], true))
				{
					/* same IP. */
					user->WriteServ(checkstr + " match " + ConvToStr(++x) + " " + a->second->GetFullRealHost());
				}
			}

			user->WriteServ(checkstr + " matches " + ConvToStr(x));
		}

		user->WriteServ(checkstr + " END " + std::string(parameters[0]));

		return CMD_LOCALONLY;
	}
};


class ModuleCheck : public Module
{
 private:
	CommandCheck *mycommand;
 public:
	ModuleCheck(InspIRCd* Me) : Module(Me)
	{
		
		mycommand = new CommandCheck(ServerInstance);
		ServerInstance->AddCommand(mycommand);

	}
	
	virtual ~ModuleCheck()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1, 1, 0, 0, VF_VENDOR, API_VERSION);
	}

	
};

MODULE_INIT(ModuleCheck)
