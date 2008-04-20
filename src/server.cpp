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

/* $Core: libIRCDserver */

#include <signal.h>
#include "exitcodes.h"
#include "inspircd.h"


void InspIRCd::SignalHandler(int signal)
{
	switch (signal)
	{
		case SIGTERM:
			Exit(signal);
			break;
	}
}

void InspIRCd::Exit(int status)
{
#ifdef WINDOWS
	delete WindowsIPC;
#endif
	if (this)
	{
		this->Cleanup();
	}
	exit (status);
}

std::string InspIRCd::GetVersionString()
{
	char versiondata[MAXBUF];
	if (*Config->CustomVersion)
	{
		snprintf(versiondata,MAXBUF,"%s-%s",VERSION,Config->CustomVersion);
	}
	else
	{
		snprintf(versiondata,MAXBUF,"%s-%s [FLAGS=%s,%s]",VERSION,SYSTEM,REVISION,SE->GetName().c_str());
	}
	return versiondata;
}

std::string InspIRCd::GetRevision()
{
	return REVISION;
}

