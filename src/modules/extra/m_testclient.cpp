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

#include "inspircd.h"
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "configreader.h"
#include "m_sqlv2.h"

class ModuleTestClient : public Module
{
private:
	

public:
	ModuleTestClient(InspIRCd* Me)
		: Module::Module(Me)
	{
		Implementation eventlist[] = { I_OnRequest, I_OnBackgroundTimer };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}

		
	virtual Version GetVersion()
	{
		return Version(1, 1, 0, 0, VF_VENDOR, API_VERSION);
	}
	
	virtual void OnBackgroundTimer(time_t foo)
	{
		Module* target = ServerInstance->Modules->FindFeature("SQL");
		
		if(target)
		{
			SQLrequest foo = SQLrequest(this, target, "foo",
					SQLquery("UPDATE rawr SET foo = '?' WHERE bar = 42") % time(NULL));
			
			if(foo.Send())
			{
				ServerInstance->Log(DEBUG, "Sent query, got given ID %lu", foo.id);
			}
			else
			{
				ServerInstance->Log(DEBUG, "SQLrequest failed: %s", foo.error.Str());
			}
		}
	}
	
	virtual char* OnRequest(Request* request)
	{
		if(strcmp(SQLRESID, request->GetId()) == 0)
		{
			ServerInstance->Log(DEBUG, "Got SQL result (%s)", request->GetId());
		
			SQLresult* res = (SQLresult*)request;

			if (res->error.Id() == NO_ERROR)
			{
				if(res->Cols())
				{
					ServerInstance->Log(DEBUG, "Got result with %d rows and %d columns", res->Rows(), res->Cols());

					for (int r = 0; r < res->Rows(); r++)
					{
						ServerInstance->Log(DEBUG, "Row %d:", r);
						
						for(int i = 0; i < res->Cols(); i++)
						{
							ServerInstance->Log(DEBUG, "\t[%s]: %s", res->ColName(i).c_str(), res->GetValue(r, i).d.c_str());
						}
					}
				}
				else
				{
					ServerInstance->Log(DEBUG, "%d rows affected in query", res->Rows());
				}
			}
			else
			{
				ServerInstance->Log(DEBUG, "SQLrequest failed: %s", res->error.Str());
				
			}
		
			return SQLSUCCESS;
		}
		
		ServerInstance->Log(DEBUG, "Got unsupported API version string: %s", request->GetId());
		
		return NULL;
	}
	
	virtual ~ModuleTestClient()
	{
	}	
};

MODULE_INIT(ModuleTestClient)

