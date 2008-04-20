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
		case SIGHUP:
			GracefulShutdown();
			break;
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

/*
 * Graceful shutdown.
 *  This is rehash, but simpler (we can't plain reload conf, as we may be chroot/setuid'd)
 *  so instead, we close all listeners, and shut down "when we can" (when all clients fuck off).
 *
 *  New process can bind and take new requests instantly.
 */
void InspIRCd::GracefulShutdown()
{
	for (unsigned int i = 0; i < Config->ports.size(); i++)
	{
		/* This calls the constructor and closes the listening socket */
		delete Config->ports[i];
	}
	ShuttingDown = true;
	FOREACH_MOD_I(this,I_OnGracefulShutdown, OnGracefulShutdown());
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

