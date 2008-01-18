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

/* $Core: libIRCDuserprocess */

#include "inspircd.h"
#include "wildcard.h"
#include "socketengine.h"
#include "command_parse.h"

void ProcessUserHandler::Call(User* cu)
{
	int result = EAGAIN;

	if (cu->GetFd() == FD_MAGIC_NUMBER)
		return;

	char* ReadBuffer = Server->GetReadBuffer();

	result = cu->ReadData(ReadBuffer, sizeof(ReadBuffer));

	if ((result) && (result != -EAGAIN))
	{
		User *current;
		int currfd;

		/*
		 * perform a check on the raw buffer as an array (not a string!) to remove
		 * character 0 which is illegal in the RFC - replace them with spaces.
		 */

		for (int checker = 0; checker < result; checker++)
		{
			if (ReadBuffer[checker] == 0)
				ReadBuffer[checker] = ' ';
		}

		if (result > 0)
			ReadBuffer[result] = '\0';

		current = cu;
		currfd = current->GetFd();

		// add the data to the users buffer
		if (result > 0)
		{
			if (!current->AddBuffer(ReadBuffer))
			{
				// AddBuffer returned false, theres too much data in the user's buffer and theyre up to no good.
				User::QuitUser(Server, current);
				return;
			}

			Server->Parser->DoLines(current);
			return;
		}

		if ((result == -1) && (errno != EAGAIN) && (errno != EINTR))
		{
			User::QuitUser(Server, cu);
			return;
		}
	}

	// result EAGAIN means nothing read
	else if ((result == EAGAIN) || (result == -EAGAIN))
	{
		/* do nothing */
	}
	else if (result == 0)
	{
		User::QuitUser(Server, cu);
		return;
	}
}

/**
 * This function is called once a second from the mainloop.
 * It is intended to do background checking on all the user structs, e.g.
 * stuff like ping checks, registration timeouts, etc.
 */
void InspIRCd::DoBackgroundUserStuff()
{
	/*
	 * loop over all local users..
	 */
	for (std::vector<User*>::iterator count2 = local_users.begin(); count2 != local_users.end(); count2++)
	{
		User *curr = *count2;

		/* process input if it's there (XXX I'm sure this could be done only on reading of data) */
		Parser->DoLines(curr);

		if (TIME > curr->timeout)
		{
			/*
			 * timeout: they've been connected too long ..
			 */
			User::QuitUser(this, curr);
			continue;
		}
	}
}

