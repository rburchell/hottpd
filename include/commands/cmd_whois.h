/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *      the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef __CMD_WHOIS_H__
#define __CMD_WHOIS_H__

// include the common header files

#include "users.h"
#include "channels.h"

const char* Spacify(char* n);
DllExport void do_whois(InspIRCd* Instance, User* user, User* dest,unsigned long signon, unsigned long idle, const char* nick);

/** Handle /WHOIS. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class CommandWhois : public Command
{
 public:
	/** Constructor for whois.
	 */
	CommandWhois (InspIRCd* Instance) : Command(Instance,"WHOIS",0,1,false,2) { syntax = "<nick>{,<nick>}"; }
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const char** parameters, int pcnt, User *user);
};

#endif
