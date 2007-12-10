/*	+------------------------------------+
 *	| Inspire Internet Relay Chat Daemon |
 *	+------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "httpclient.h"

/* $ModDep: httpclient.h */

class MyModule : public Module
{

public:

	MyModule(InspIRCd* Me)
		: Module::Module(Me)
	{
		Implementation eventlist[] = { I_OnRequest, I_OnUserJoin, I_OnUserPart };
		ServerInstance->Modules->Attach(eventlist, this, 3);
	}

	virtual ~MyModule()
	{
	}


	virtual Version GetVersion()
	{
		return Version(1,0,0,1,VF_VENDOR,API_VERSION);
	}

	virtual void OnUserJoin(User* user, Channel* channel, bool &silent)
	{
		// method called when a user joins a channel

		std::string chan = channel->name;
		std::string nick = user->nick;
		ServerInstance->Log(DEBUG,"User " + nick + " joined " + chan);

		Module* target = ServerInstance->Modules->Find("m_http_client.so");
		if(target)
		{
			HTTPClientRequest req(ServerInstance, this, target, "http://znc.in/~psychon");
			req.Send();
		}
		else
			ServerInstance->Log(DEBUG,"module not found, load it!!");
	}

	char* OnRequest(Request* req)
	{
		HTTPClientResponse* resp = (HTTPClientResponse*)req;
		if(!strcmp(resp->GetId(), HTTP_CLIENT_RESPONSE))
		{
			ServerInstance->Log(DEBUG, resp->GetData()); 
		}
		return NULL;
	}

	virtual void OnUserPart(User* user, Channel* channel, const std::string &partmessage, bool &silent)
	{
	}

};

MODULE_INIT(MyModule)

