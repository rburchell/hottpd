/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* $Core: libIRCDcommand_parse */

#include "inspircd.h"
#include "wildcard.h"
#include "socketengine.h"
#include "socket.h"
#include "command_parse.h"
#include "exitcodes.h"

/* Directory Searching for Unix-Only */
#ifndef WIN32
#include <dirent.h>
#include <dlfcn.h>
#endif

/* LoopCall is used to call a command classes handler repeatedly based on the contents of a comma seperated list.
 * There are two overriden versions of this method, one of which takes two potential lists and the other takes one.
 * We need a version which takes two potential lists for JOIN, because a JOIN may contain two lists of items at once,
 * the channel names and their keys as follows:
 * JOIN #chan1,#chan2,#chan3 key1,,key3
 * Therefore, we need to deal with both lists concurrently. The first instance of this method does that by creating
 * two instances of irc::commasepstream and reading them both together until the first runs out of tokens.
 * The second version is much simpler and just has the one stream to read, and is used in NAMES, WHOIS, PRIVMSG etc.
 * Both will only parse until they reach ServerInstance->Config->MaxTargets number of targets, to stop abuse via spam.
 */
int CommandParser::LoopCall(User* user, Command* CommandObj, const char** parameters, int pcnt, unsigned int splithere, unsigned int extra)
{
	/* First check if we have more than one item in the list, if we don't we return zero here and the handler
	 * which called us just carries on as it was.
	 */
	if (!strchr(parameters[splithere],','))
		return 0;

	/** Some lame ircds will weed out dupes using some shitty O(n^2) algorithm.
	 * By using std::map (thanks for the idea w00t) we can cut this down a ton.
	 * ...VOOODOOOO!
	 */
	std::map<irc::string, bool> dupes;

	/* Create two lists, one for channel names, one for keys
	 */
	irc::commasepstream items1(parameters[splithere]);
	irc::commasepstream items2(parameters[extra]);
	std::string extrastuff;
	std::string item;
	unsigned int max = 0;

	/* Attempt to iterate these lists and call the command objech
	 * which called us, for every parameter pair until there are
	 * no more left to parse.
	 */
	while (items1.GetToken(item) && (max++ < ServerInstance->Config->MaxTargets))
	{
		if (dupes.find(item.c_str()) == dupes.end())
		{
			const char* new_parameters[MAXPARAMETERS];

			for (int t = 0; (t < pcnt) && (t < MAXPARAMETERS); t++)
				new_parameters[t] = parameters[t];

			if (!items2.GetToken(extrastuff))
				extrastuff = "";

			new_parameters[splithere] = item.c_str();
			new_parameters[extra] = extrastuff.c_str();

			CommandObj->Handle(new_parameters,pcnt,user);

			dupes[item.c_str()] = true;
		}
	}
	return 1;
}

int CommandParser::LoopCall(User* user, Command* CommandObj, const char** parameters, int pcnt, unsigned int splithere)
{
	/* First check if we have more than one item in the list, if we don't we return zero here and the handler
	 * which called us just carries on as it was.
	 */
	if (!strchr(parameters[splithere],','))
		return 0;

	std::map<irc::string, bool> dupes;

	/* Only one commasepstream here */
	irc::commasepstream items1(parameters[splithere]);
	std::string item;
	unsigned int max = 0;

	/* Parse the commasepstream until there are no tokens remaining.
	 * Each token we parse out, call the command handler that called us
	 * with it
	 */
	while (items1.GetToken(item) && (max++ < ServerInstance->Config->MaxTargets))
	{
		if (dupes.find(item.c_str()) == dupes.end())
		{
			const char* new_parameters[MAXPARAMETERS];

			for (int t = 0; (t < pcnt) && (t < MAXPARAMETERS); t++)
				new_parameters[t] = parameters[t];

			new_parameters[splithere] = item.c_str();

			parameters[splithere] = item.c_str();

			/* Execute the command handler over and over. If someone pulls our user
			 * record out from under us (e.g. if we /kill a comma sep list, and we're
			 * in that list ourselves) abort if we're gone.
			 */
			CommandObj->Handle(new_parameters,pcnt,user);

			dupes[item.c_str()] = true;
		}
	}
	/* By returning 1 we tell our caller that nothing is to be done,
	 * as all the previous calls handled the data. This makes the parent
	 * return without doing any processing.
	 */
	return 1;
}

Command* CommandParser::GetHandler(const std::string &commandname)
{
	Commandable::iterator n = cmdlist.find(commandname);
	if (n != cmdlist.end())
		return n->second;

	return NULL;
}

// calls a handler function for a command

void CommandParser::DoLines(User* current, bool one_only)
{
#if 0
	// while there are complete lines to process...
	unsigned int floodlines = 0;

	while (current->BufferIsReady())
	{
		if (current->MyClass)
		{
			if (ServerInstance->Time() > current->reset_due)
			{
				current->reset_due = ServerInstance->Time() + current->MyClass->GetThreshold();
				current->lines_in = 0;
			}

			if (++current->lines_in > current->MyClass->GetFlood() && current->MyClass->GetFlood())
			{
				ServerInstance->FloodQuitUser(current);
				return;
			}

			if ((++floodlines > current->MyClass->GetFlood()) && (current->MyClass->GetFlood() != 0))
			{
				ServerInstance->FloodQuitUser(current);
				return;
			}
		}

		// use GetBuffer to copy single lines into the sanitized string
		std::string single_line = current->GetBuffer();
		current->bytes_in += single_line.length();
		current->cmds_in++;
		if (single_line.length() > MAXBUF - 2)  // MAXBUF is 514 to allow for neccessary line terminators
			single_line.resize(MAXBUF - 2); // So to trim to 512 here, we use MAXBUF - 2

		// ProcessBuffer returns false if the user has gone over penalty
		if (!ServerInstance->Parser->ProcessBuffer(single_line, current) || one_only)
			break;
	}
#endif
}

bool CommandParser::ProcessCommand(User *user, std::string &cmd)
{
#if 0
	const char *command_p[MAXPARAMETERS];
	int items = 0;
	irc::tokenstream tokens(cmd);
	std::string command;
	tokens.GetToken(command);

	/* A client sent a nick prefix on their command (ick)
	 * rhapsody and some braindead bouncers do this --
	 * the rfc says they shouldnt but also says the ircd should
	 * discard it if they do.
	 */
	if (*command.c_str() == ':')
		tokens.GetToken(command);

	while (tokens.GetToken(para[items]) && (items < MAXPARAMETERS))
	{
		command_p[items] = para[items].c_str();
		items++;
	}

	std::transform(command.begin(), command.end(), command.begin(), ::toupper);
		
	int MOD_RESULT = 0;
	FOREACH_RESULT(I_OnPreCommand,OnPreCommand(command,command_p,items,user,false,cmd));
	if (MOD_RESULT == 1) {
		return true;
	}

	/* find the command, check it exists */
	Commandable::iterator cm = cmdlist.find(command);
	
	if (cm == cmdlist.end())
	{
		ServerInstance->stats->statsUnknown++;
		user->WriteServ("421 %s %s :Unknown command",user->nick,command.c_str());
		return true;
	}

	/* Modify the user's penalty */
	bool do_more = true;
	if (!user->ExemptFromPenalty)
	{
		user->IncreasePenalty(cm->second->Penalty);
		do_more = (user->Penalty < 10);
		if (!do_more)
			user->OverPenalty = true;
	}

	/* activity resets the ping pending timer */
	if (user->MyClass)
		user->nping = ServerInstance->Time() + user->MyClass->GetPingTime();

	if (!user->HasPermission(command))
	{
		user->WriteServ("481 %s :Permission Denied - Oper type %s does not have access to command %s",user->nick,user->oper,command.c_str());
		return do_more;
	}
	if ((user->registered == REG_ALL) && (!IS_OPER(user)) && (cm->second->IsDisabled()))
	{
		/* command is disabled! */
		user->WriteServ("421 %s %s :This command has been disabled.",user->nick,command.c_str());
		return do_more;
	}
	if (items < cm->second->min_params)
	{
		user->WriteServ("461 %s %s :Not enough parameters.", user->nick, command.c_str());
		if ((ServerInstance->Config->SyntaxHints) && (user->registered == REG_ALL) && (cm->second->syntax.length()))
			user->WriteServ("304 %s :SYNTAX %s %s", user->nick, cm->second->command.c_str(), cm->second->syntax.c_str());
		return do_more;
	}
	if ((user->registered != REG_ALL) && (!cm->second->WorksBeforeReg()))
	{
		user->WriteServ("451 %s :You have not registered",command.c_str());
		return do_more;
	}
	else
	{
		/* passed all checks.. first, do the (ugly) stats counters. */
		cm->second->use_count++;
		cm->second->total_bytes += cmd.length();

		/* module calls too */
		int MOD_RESULT = 0;
		FOREACH_RESULT(I_OnPreCommand,OnPreCommand(command,command_p,items,user,true,cmd));
		if (MOD_RESULT == 1)
			return do_more;

		/*
		 * WARNING: be careful, the user may be deleted soon
		 */
		CmdResult result = cm->second->Handle(command_p,items,user);

		FOREACH_MOD(I_OnPostCommand,OnPostCommand(command, command_p, items, user, result,cmd));
		return do_more;
	}
#endif
}

bool CommandParser::RemoveCommands(const char* source)
{
	Commandable::iterator i,safei;
	for (i = cmdlist.begin(); i != cmdlist.end(); i++)
	{
		safei = i;
		safei++;
		if (safei != cmdlist.end())
		{
			RemoveCommand(safei, source);
		}
	}
	safei = cmdlist.begin();
	if (safei != cmdlist.end())
	{
		RemoveCommand(safei, source);
	}
	return true;
}

void CommandParser::RemoveCommand(Commandable::iterator safei, const char* source)
{
	Command* x = safei->second;
	if (x->source == std::string(source))
	{
		cmdlist.erase(safei);
		delete x;
	}
}

bool CommandParser::ProcessBuffer(std::string &buffer,User *user)
{
#if 0
	std::string::size_type a;

	if (!user)
		return true;

	while ((a = buffer.rfind("\n")) != std::string::npos)
		buffer.erase(a);
	while ((a = buffer.rfind("\r")) != std::string::npos)
		buffer.erase(a);

	if (buffer.length())
	{
		if (!user->muted)
		{
			ServerInstance->Log(DEBUG,"C[%d] I :%s %s",user->GetFd(), user->nick, buffer.c_str());
			return this->ProcessCommand(user,buffer);
		}
	}
	return true;
#endif
}

CommandParser::CommandParser(InspIRCd* Instance) : ServerInstance(Instance)
{
	para.resize(128);
}

