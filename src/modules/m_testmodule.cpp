/*
 *   hottpd - a fast, extensible, featureful http server
 *          (C) 2007-2008 hottpd development team
 *
 * Based on InspIRCd - (C) 2002-2007 InspIRCd Development Team
 *
 *    This program is free but copyrighted software; see
 *              the file COPYING for details.
 *
 */

#include "inspircd.h"

/* $ModDesc: A simple module to test the module system */

class ModuleTest : public Module
{
 private:
	 
 public:
	ModuleTest(InspIRCd *Srv)
		: Module(Srv)
	{
		Implementation eventlist[] = { I_OnConnectionConnect, I_OnConnectionDisconnect };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}
	
	virtual ~ModuleTest()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1, 0, 0, 0, VF_VENDOR, API_VERSION);
	}
	
	virtual void OnConnectionConnect(Connection *conn)
	{
		ServerInstance->Log(DEBUG, "Module new connection triggered");
	}
	
	virtual void OnConnectionDisconnect(Connection *conn)
	{
		ServerInstance->Log(DEBUG, "Module disconnect connection triggered");
	}
};


MODULE_INIT(ModuleTest)

