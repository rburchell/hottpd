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

/*
 * Checks whether a connection is ready to be processed, passes it to ProcessBuffer if so.
 */
void CommandParser::DoLines(User *current)
{
	while (current->BufferIsReady())
	{
		// use GetBuffer to copy single lines into the sanitized string
		std::string single_line = current->GetBuffer();
		current->bytes_in += single_line.length();
		current->cmds_in++;
		if (single_line.length() > MAXBUF - 2)  // MAXBUF is 514 to allow for neccessary line terminators
			single_line.resize(MAXBUF - 2); // So to trim to 512 here, we use MAXBUF - 2

		// ProcessBuffer returns false if the user has gone over penalty
		if (!ServerInstance->Parser->ProcessBuffer(single_line, current))
			break;
	}
}

/*
 * User's buffer is ready to be manipulated. Do it.
 */
bool CommandParser::ProcessBuffer(std::string &buffer,User *user)
{
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
			ServerInstance->Log(DEBUG,"C[%d] I :%s %s",user->GetFd(), user->ip.c_str(), buffer.c_str());
			return true;
		}
	}
	return true;
}

CommandParser::CommandParser(InspIRCd* Instance) : ServerInstance(Instance)
{
	para.resize(128);
}

