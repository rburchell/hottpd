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
#include "urlencode.h"

URIQueryString::URIQueryString(const std::string &str)
{
	std::string::iterator it = str.begin();
	std::string key, value;
	bool keynow = true;
	
	for (; it != str.end(); it++)
	{
		if (*it == '=')
		{
			keynow = false;
		}
		else if (*it == '&')
		{
			if (!key.empty() && !value.empty())
				Values.insert(std::make_pair<std::string,std::string>(key, value));
			
			key.clear();
			value.clear();
			keynow = true;
		}
		else if (*it == '+')
		{
			if (keynow)
				key += ' ';
			else
				value += ' ';
		}
		else if (*it == '%')
		{
			if ((it + 1 == str.end()) || (it + 2 == str.end())
			{
				// Invalid hex representation, skip it
				it = str.end() - 1;
				continue;
			}
			
			char c;
			if (!utils::unhexchar(*(it + 1), *(it + 2)))
			{
				i += 2;
				continue;
			}
			
			if (keynow)
				key += c;
			else
				value += c;
			
			i += 2;
		}
		else
		{
			if (keynow)
				key += *it;
			else
				value += *it;
		}
	}
}
