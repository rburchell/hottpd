/* $Core: libhttpd_mime */

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


