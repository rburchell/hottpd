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

#ifndef __URLENCODE_H__
#define __URLENCODE_H__

class URIQueryString
{
 private
	typedef std::multimap<std::string,std::string,utils::StrCaseLess> ValueMap;
	
	ValueMap Values;
 public:
	URIQueryString(const std::string &str);
	
	bool IsSet(const std::string &key)
	{
		return (Values.find(key) != Values.end());
	}
	
	std::string Get(const std::string &key)
	{
		ValueMap::iterator it = Values.find(key);
		if (it == Values.end())
			return std::string();
		return it->second;
	}
};

#endif
