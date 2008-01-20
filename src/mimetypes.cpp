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

/* $Core: libhttpd_mimetypes */

#include "inspircd.h"

void MimeManager::AddType(const std::string &ext, const std::string &type)
{
	MimeTypes[ext] = type;
}

const std::string MimeManager::GetType(const std::string &ext)
{
	std::map<std::string, std::string>::iterator i = MimeTypes.find(ext);

	if (i == MimeTypes.end())
		return "";

	return i->second;
}


