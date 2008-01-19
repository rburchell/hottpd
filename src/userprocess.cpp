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

void ProcessUserHandler::Call(Connection* cu)
{
	int result = EAGAIN;

	if (cu->GetFd() == FD_MAGIC_NUMBER)
		return;

	char* ReadBuffer = Server->GetReadBuffer();

	result = cu->ReadData(ReadBuffer, sizeof(ReadBuffer));

	if ((result) && (result != -EAGAIN))
	{
		Connection *current;
		int currfd;

		if (result > 0)
			ReadBuffer[result] = '\0';

		current = cu;
		currfd = current->GetFd();

		// add the data to the connection's buffer (which will process it if necessary)
		if (result > 0)
		{
			if (!current->AddBuffer(ReadBuffer))
			{
				// fuck, something exploded
				Server->Connections->Delete(current);
				return;
			}

			return;
		}

		if ((result == -1) && (errno != EAGAIN) && (errno != EINTR))
		{
			Server->Connections->Delete(cu);
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
		Server->Connections->Delete(cu);
		return;
	}
}


