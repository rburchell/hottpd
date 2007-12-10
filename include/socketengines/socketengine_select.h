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

#ifndef __SOCKETENGINE_SELECT__
#define __SOCKETENGINE_SELECT__

#include <vector>
#include <string>
#include <map>
#include <sys/select.h>
#include "inspircd_config.h"
#include "globals.h"
#include "inspircd.h"
#include "socketengine.h"

class InspIRCd;

/** A specialisation of the SocketEngine class, designed to use traditional select().
 */
class SelectEngine : public SocketEngine
{
private:
	/** Because select() does not track an fd list for us between calls, we have one of our own
	 */
	std::map<int,int> fds;
	/** List of writeable ones (WantWrite())
	 */
	bool writeable[MAX_DESCRIPTORS];
	/** The read set and write set, populated before each call to select().
	 */
	fd_set wfdset, rfdset, errfdset;
public:
	/** Create a new SelectEngine
	 * @param Instance The creator of this object
	 */
	SelectEngine(InspIRCd* Instance);
	/** Delete a SelectEngine
	 */
	virtual ~SelectEngine();
	virtual bool AddFd(EventHandler* eh);
	virtual int GetMaxFds();
	virtual int GetRemainingFds();
	virtual bool DelFd(EventHandler* eh, bool force = false);
	virtual int DispatchEvents();
	virtual std::string GetName();
	virtual void WantWrite(EventHandler* eh);
};

/** Creates a SocketEngine
 */
class SocketEngineFactory
{
public:
	/** Create a new instance of SocketEngine based on SelectEngine
	 */
	SocketEngine* Create(InspIRCd* Instance) { return new SelectEngine(Instance); }
};

#endif
