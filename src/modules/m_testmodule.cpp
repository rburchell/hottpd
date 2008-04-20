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
		Implementation eventlist[] = { I_OnConnectionConnect, I_OnConnectionDisconnect, I_OnPreRequest };
		ServerInstance->Modules->Attach(eventlist, this, 3);
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

	virtual int OnPreRequest(Connection *c, const std::string &method, const std::string &vhost, const std::string &dir, const std::string &file)
	{
		ServerInstance->Log(DEBUG, "Got a %s request from %d on %s for %s and file %s", method.c_str(), c->GetFd(), vhost.c_str(), dir.c_str(), file.c_str());
		return 0;
	}
};


MODULE_INIT(ModuleTest)

