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

/* $Core: libIRCDhelper */

#include "inspircd.h"
#include <stdarg.h>
#include "wildcard.h"
#include "xline.h"
#include "exitcodes.h"

static char TIMESTR[26];
static time_t LAST = 0;

/** Log()
 *  Write a line of text `text' to the logfile (and stdout, if in nofork) if the level `level'
 *  is greater than the configured loglevel.
 */
void InspIRCd::Log(int level, const char* text, ...)
{
	/* sanity check, just in case */
	if (!this->Config || !this->Logger)
		return;

	/* Do this check again here so that we save pointless vsnprintf calls */
	if ((level < Config->LogLevel) && !Config->forcedebug)
		return;

	va_list argsPtr;
	char textbuffer[65536];

	va_start(argsPtr, text);
	vsnprintf(textbuffer, 65536, text, argsPtr);
	va_end(argsPtr);

	this->Log(level, std::string(textbuffer));
}

void InspIRCd::Log(int level, const std::string &text)
{
	/* sanity check, just in case */
	if (!this->Config || !this->Logger)
		return;

	/* If we were given -debug we output all messages, regardless of configured loglevel */
	if ((level < Config->LogLevel) && !Config->forcedebug)
		return;

	if (Time() != LAST)
	{
		time_t local = Time();
		struct tm *timeinfo = localtime(&local);

		strlcpy(TIMESTR,asctime(timeinfo),26);
		TIMESTR[24] = ':';
		LAST = Time();
	}

	if (Config->log_file && Config->writelog)
	{
		std::string out = std::string(TIMESTR) + " " + text.c_str() + "\n";
		this->Logger->WriteLogLine(out);
	}

	if (Config->nofork)
	{
		printf("%s %s\n", TIMESTR, text.c_str());
	}
}

User* InspIRCd::FindNickOnly(const std::string &nick)
{
	user_hash::iterator iter = clientlist->find(nick);

	if (iter == clientlist->end())
		return NULL;

	return iter->second;
}

User* InspIRCd::FindNickOnly(const char* nick)
{
	user_hash::iterator iter = clientlist->find(nick);

	if (iter == clientlist->end())
		return NULL;

	return iter->second;
}

bool InspIRCd::IsValidMask(const std::string &mask)
{
	char* dest = (char*)mask.c_str();
	int exclamation = 0;
	int atsign = 0;

	for (char* i = dest; *i; i++)
	{
		/* out of range character, bad mask */
		if (*i < 32 || *i > 126)
		{
			return false;
		}

		switch (*i)
		{
			case '!':
				exclamation++;
				break;
			case '@':
				atsign++;
				break;
		}
	}

	/* valid masks only have 1 ! and @ */
	if (exclamation != 1 || atsign != 1)
		return false;

	return true;
}

/* open the proper logfile */
bool InspIRCd::OpenLog(char**, int)
{
	Config->MyDir = Config->GetFullProgDir();

	if (!*this->LogFileName)
	{
		if (Config->logpath.empty())
		{
			Config->logpath = Config->MyDir + "/ircd.log";
		}

		Config->log_file = fopen(Config->logpath.c_str(),"a+");
	}
	else
	{
		Config->log_file = fopen(this->LogFileName,"a+");
	}

	if (!Config->log_file)
	{
		this->Logger = NULL;
		return false;
	}

	this->Logger = new FileLogger(this, Config->log_file);
	return true;
}

void InspIRCd::CheckRoot()
{
	if (geteuid() == 0)
	{
		printf("WARNING!!! You are running an irc server as ROOT!!! DO NOT DO THIS!!!\n\n");
		this->Log(DEFAULT,"Cant start as root");
		Exit(EXIT_STATUS_ROOT);
	}
}

void InspIRCd::CheckDie()
{
	if (*Config->DieValue)
	{
		printf("WARNING: %s\n\n",Config->DieValue);
		this->Log(DEFAULT,"Died because of <die> tag: %s",Config->DieValue);
		Exit(EXIT_STATUS_DIETAG);
	}
}

/** Refactored by Brain, Jun 2007. Much faster with some clever O(1) array
 * lookups and pointer maths.
 */
long InspIRCd::Duration(const std::string &str)
{
	unsigned char multiplier = 0;
	long total = 0;
	long times = 1;
	long subtotal = 0;

	/* Iterate each item in the string, looking for number or multiplier */
	for (std::string::const_reverse_iterator i = str.rbegin(); i != str.rend(); ++i)
	{
		/* Found a number, queue it onto the current number */
		if ((*i >= '0') && (*i <= '9'))
		{
			subtotal = subtotal + ((*i - '0') * times);
			times = times * 10;
		}
		else
		{
			/* Found something thats not a number, find out how much
			 * it multiplies the built up number by, multiply the total
			 * and reset the built up number.
			 */
			if (subtotal)
				total += subtotal * duration_multi[multiplier];

			/* Next subtotal please */
			subtotal = 0;
			multiplier = *i;
			times = 1;
		}
	}
	if (multiplier)
	{
		total += subtotal * duration_multi[multiplier];
		subtotal = 0;
	}
	/* Any trailing values built up are treated as raw seconds */
	return total + subtotal;
}

std::string InspIRCd::TimeString(time_t curtime)
{
	return std::string(ctime(&curtime),24);
}

