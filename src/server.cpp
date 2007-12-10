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
			Rehash();
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

void InspIRCd::Rehash()
{
	this->CloseLog();
	this->OpenLog(this->Config->argv, this->Config->argc);
	this->RehashUsersAndChans();
	FOREACH_MOD_I(this, I_OnGarbageCollect, OnGarbageCollect());
	/*this->Config->Read(false,NULL);*/
	this->Res->Rehash();
	FOREACH_MOD_I(this,I_OnRehash,OnRehash(NULL,""));
}

void InspIRCd::RehashServer()
{
	this->RehashUsersAndChans();
	this->Res->Rehash();
}

std::string InspIRCd::GetVersionString()
{
	char versiondata[MAXBUF];
	if (*Config->CustomVersion)
	{
		snprintf(versiondata,MAXBUF,"%s %s :%s",VERSION,Config->ServerName,Config->CustomVersion);
	}
	else
	{
		snprintf(versiondata,MAXBUF,"%s %s :%s [FLAGS=%s,%s,%d]",VERSION,Config->ServerName,SYSTEM,REVISION,SE->GetName().c_str(),Config->sid);
	}
	return versiondata;
}

std::string InspIRCd::GetRevision()
{
	return REVISION;
}

void InspIRCd::AddServerName(const std::string &servername)
{
	servernamelist::iterator itr = servernames.begin();
	for(; itr != servernames.end(); ++itr)
		if(**itr == servername)
			return;

	string * ns = new string(servername);
	servernames.push_back(ns);
}

const char* InspIRCd::FindServerNamePtr(const std::string &servername)
{
	servernamelist::iterator itr = servernames.begin();
	for(; itr != servernames.end(); ++itr)
		if(**itr == servername)
			return (*itr)->c_str();

	servernames.push_back(new string(servername));
	itr = --servernames.end();
	return (*itr)->c_str();
}

bool InspIRCd::FindServerName(const std::string &servername)
{
	servernamelist::iterator itr = servernames.begin();
	for(; itr != servernames.end(); ++itr)
		if(**itr == servername)
			return true;
	return false;
}

	
