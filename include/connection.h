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

#ifndef __CONNECTION_H__
#define __CONNECTION_H__

#include <time.h>
#include "inspircd_config.h"
#include "base.h"
#include "socketengine.h"

/** connection is the base class of User, and holds basic user properties.
 * This can be extended for holding other user-like objects in the future.
 */
class CoreExport connection : public EventHandler
{
 public:
	/** Hostname of connection.
	 * This should be valid as per RFC1035.
	 */
	char host[65];

	/** Stats counter for bytes inbound
	 */
	int bytes_in;

	/** Stats counter for bytes outbound
	 */
	int bytes_out;

	/** Stats counter for commands inbound
	 */
	int cmds_in;

	/** Stats counter for commands outbound
	 */
	int cmds_out;

	/** True if user has authenticated, false if otherwise
	 */
	bool haspassed;

	/** Used by User to indicate the registration status of the connection
	 * It is a bitfield of the REG_NICK, REG_USER and REG_ALL bits to indicate
	 * the connection state.
	 */
	char registered;
	
	/** Time the connection was last pinged
	 */
	time_t lastping;
	
	/** Time the connection was created, set in the constructor. This
	 * may be different from the time the user's classbase object was
	 * created.
	 */
	time_t signon;
	
	/** Time that the connection last sent a message, used to calculate idle time
	 */
	time_t idle_lastmsg;
	
	/** Used by PING checking code
	 */
	time_t nping;
};


#endif
