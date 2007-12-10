/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* $Core: libIRCDxline */

#include "inspircd.h"
#include "wildcard.h"
#include "xline.h"
#include "bancache.h"

/*
 * This is now version 3 of the XLine subsystem, let's see if we can get it as nice and 
 * efficient as we can this time so we can close this file and never ever touch it again ..
 *
 * Background:
 *  Version 1 stored all line types in one list (one for g, one for z, etc). This was fine,
 *  but both version 1 and 2 suck at applying lines efficiently. That is, every time a new line
 *  was added, it iterated every existing line for every existing user. Ow. Expiry was also
 *  expensive, as the lists were NOT sorted.
 *
 *  Version 2 moved permanent lines into a seperate list from non-permanent to help optimize
 *  matching speed, but matched in the same way.
 *  Expiry was also sped up by sorting the list by expiry (meaning just remove the items at the
 *  head of the list that are outdated.)
 *
 * This was fine and good, but it looked less than ideal in code, and matching was still slower
 * than it could have been, something which we address here.
 *
 * VERSION 3:
 *  All lines are (as in v1) stored together -- no seperation of perm and non-perm. They are stored in
 *  a map of maps (first map is line type, second map is for quick lookup on add/delete/etc).
 *
 *  Expiry is *no longer* performed on a timer, and no longer uses a sorted list of any variety. This
 *  is now done by only checking for expiry when a line is accessed, meaning that expiry is no longer
 *  a resource intensive problem.
 *
 *  Application no longer tries to apply every single line on every single user - instead, now only lines
 *  added since the previous application are applied. This keeps S2S ADDLINE during burst nice and fast,
 *  while at the same time not slowing things the fuck down when we try adding a ban with lots of preexisting
 *  bans. :)
 */

bool XLine::Matches(User *u)
{
	return false;
}

XLineLookup* XLineManager::GetAll(const std::string &type)
{
	ContainerIter n = lookup_lines.find(type);

	if (n == lookup_lines.end())
		return NULL;

	LookupIter safei;
	const time_t current = ServerInstance->Time();

	/* Expire any dead ones, before sending */
	for (LookupIter x = n->second.begin(); x != n->second.end(); )
	{
		safei = x;
		safei++;
		if (x->second->duration && current > x->second->expiry)
		{
			ExpireLine(n, x);
		}
		x = safei;
	}

	return &(n->second);
}

std::vector<std::string> XLineManager::GetAllTypes()
{
	std::vector<std::string> items;
	for (ContainerIter x = lookup_lines.begin(); x != lookup_lines.end(); ++x)
		items.push_back(x->first);
	return items;
}

// adds a line

bool XLineManager::AddLine(XLine* line, User* user)
{
	if (DelLine(line->Displayable(), line->type, user, true))
		return false;

	pending_lines.push_back(line);
	lookup_lines[line->type][line->Displayable()] = line;
	line->OnAdd();

	FOREACH_MOD(I_OnAddLine,OnAddLine(user, line));	

	return true;
}

// deletes a line, returns true if the line existed and was removed

bool XLineManager::DelLine(const char* hostmask, const std::string &type, User* user, bool simulate)
{
	ContainerIter x = lookup_lines.find(type);

	if (x == lookup_lines.end())
		return false;

	LookupIter y = x->second.find(hostmask);

	if (y == x->second.end())
		return false;

	if (simulate)
		return true;

	FOREACH_MOD(I_OnDelLine,OnDelLine(user, y->second));

	y->second->Unset();

	std::vector<XLine*>::iterator pptr = std::find(pending_lines.begin(), pending_lines.end(), y->second);
	if (pptr != pending_lines.end())
		pending_lines.erase(pptr);

	delete y->second;
	x->second.erase(y);

	return true;
}

XLine* XLineManager::MatchesLine(const std::string &type, User* user)
{
	ContainerIter x = lookup_lines.find(type);

	if (x == lookup_lines.end())
		return NULL;

	const time_t current = ServerInstance->Time();

	LookupIter safei;

	for (LookupIter i = x->second.begin(); i != x->second.end(); )
	{
		safei = i;
		safei++;

		if (i->second->Matches(user))
		{
			if (i->second->duration && current > i->second->expiry)
			{
				/* Expire the line, return nothing */
				ExpireLine(x, i);
				/* Continue, there may be another that matches
				 * (thanks aquanight)
				 */
				i = safei;
				continue;
			}
			else
				return i->second;
		}

		i = safei;
	}
	return NULL;
}

XLine* XLineManager::MatchesLine(const std::string &type, const std::string &pattern)
{
	ContainerIter x = lookup_lines.find(type);

	if (x == lookup_lines.end())
		return NULL;

	const time_t current = ServerInstance->Time();

	 LookupIter safei;

	for (LookupIter i = x->second.begin(); i != x->second.end(); )
	{
		safei = i;
		safei++;

		if (i->second->Matches(pattern))
		{
			if (i->second->duration && current > i->second->expiry)
			{
				/* Expire the line, return nothing */
				ExpireLine(x, i);
				/* See above */
				i = safei;
				continue;
			}
			else
				return i->second;
		}

		i = safei;
	}
	return NULL;
}

// removes lines that have expired
void XLineManager::ExpireLine(ContainerIter container, LookupIter item)
{
	item->second->DisplayExpiry();
	item->second->Unset();

	/* TODO: Can we skip this loop by having a 'pending' field in the XLine class, which is set when a line
	 * is pending, cleared when it is no longer pending, so we skip over this loop if its not pending?
	 * -- Brain
	 */
	std::vector<XLine*>::iterator pptr = std::find(pending_lines.begin(), pending_lines.end(), item->second);
	if (pptr != pending_lines.end())
		pending_lines.erase(pptr);

	delete item->second;
	container->second.erase(item);
}


void XLineManager::ApplyLines()
{
	for (std::vector<User*>::const_iterator u2 = ServerInstance->local_users.begin(); u2 != ServerInstance->local_users.end(); u2++)
	{
		User* u = (User*)(*u2);

		for (std::vector<XLine *>::iterator i = pending_lines.begin(); i != pending_lines.end(); i++)
		{
			XLine *x = *i;
			if (x->Matches(u))
				x->Apply(u);
		}
	}

	pending_lines.clear();
}

void XLineManager::InvokeStats(const std::string &type, int numeric, User* user, string_list &results)
{
#if 0
	std::string sn = ServerInstance->Config->ServerName;

	ContainerIter n = lookup_lines.find(type);

	time_t current = ServerInstance->Time();

	LookupIter safei;

	if (n != lookup_lines.end())
	{
		XLineLookup& list = n->second;
		for (LookupIter i = list.begin(); i != list.end(); )
		{
			safei = i;
			safei++;

			if (i->second->duration && current > i->second->expiry)
			{
				ExpireLine(n, i);
			}
			else
				results.push_back(sn+" "+ConvToStr(numeric)+" "+user->nick+" :"+i->second->Displayable()+" "+
					ConvToStr(i->second->set_time)+" "+ConvToStr(i->second->duration)+" "+std::string(i->second->source)+" :"+(i->second->reason));
			i = safei;
		}
	}
#endif
}


XLineManager::XLineManager(InspIRCd* Instance) : ServerInstance(Instance)
{
	ZFact = new ZLineFactory(Instance);
	RegisterFactory(ZFact);
}

XLineManager::~XLineManager()
{
	UnregisterFactory(ZFact);
	delete ZFact;
}

void XLine::Apply(User* u)
{
}

void XLine::DefaultApply(User* u, const std::string &line, bool bancache)
{
	char reason[MAXBUF];
	snprintf(reason, MAXBUF, "%s-Lined: %s", line.c_str(), this->reason);
	if (ServerInstance->Config->HideBans)
		User::QuitUser(ServerInstance, u, line + "-Lined", reason);
	else
		User::QuitUser(ServerInstance, u, reason);


	if (bancache)
	{
		ServerInstance->Log(DEBUG, std::string("BanCache: Adding positive hit (") + line + ") for " + u->GetIPString());
		ServerInstance->BanCache->AddHit(u->GetIPString(), this->type, line + "-Lined: " + this->reason);
	}
}

bool ZLine::Matches(User *u)
{
	if (u->exempt)
		return false;

	if (match(u->GetIPString(), this->ipaddr, true))
		return true;
	else
		return false;
}

void ZLine::Apply(User* u)
{       
	DefaultApply(u, "Z", true);
}

bool ZLine::Matches(const std::string &str)
{
	if (match(str.c_str(), this->ipaddr, true))
		return true;
	else
		return false;
}

void ZLine::DisplayExpiry()
{
}

const char* ZLine::Displayable()
{
	return ipaddr;
}

bool XLineManager::RegisterFactory(XLineFactory* xlf)
{
	XLineFactMap::iterator n = line_factory.find(xlf->GetType());

	if (n != line_factory.end())
		return false;

	line_factory[xlf->GetType()] = xlf;

	return true;
}

bool XLineManager::UnregisterFactory(XLineFactory* xlf)
{
	XLineFactMap::iterator n = line_factory.find(xlf->GetType());

	if (n == line_factory.end())
		return false;

	line_factory.erase(n);

	return true;
}

XLineFactory* XLineManager::GetFactory(const std::string &type)
{
	XLineFactMap::iterator n = line_factory.find(type);

	if (n != line_factory.end())
		return NULL;

	return n->second;
}

