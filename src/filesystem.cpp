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

/* $Core: libhttpd_filesystem */

#include "inspircd.h"
#include "filesystem.h"

FileSystem::FileSystem(InspIRCd *Instance)
	: ServerInstance(Instance)
{
}


std::string FileSystem::CheckFilePath(const std::string &basedir, const std::string &path, struct stat *&fst)
{
	std::string fullpath(basedir);
	
	if (fullpath[fullpath.length() - 1] == '/')
		fullpath.erase(fullpath.end() - 1);
	
	// Skip the basedir in this process
	int l = fullpath.length();
	
	fullpath.append(path);
	std::string::iterator i = fullpath.begin() + l;
	
	/*
	 * This is climbing up the entire directory tree and stating each item,
	 * used because it's necessary for the FollowSymLinks setting when off, and
	 * because it's the only way pathinfo can work. In the interest of ricering,
	 * we may want to add a short circuit for this when followsymlinks is on or
	 * possibly add an option to disable pathinfo.
	 */
	
	for (i++; i != fullpath.end(); i++)
	{
		if (*i == '/')
		{
			if (this->Stat(std::string(fullpath.begin(), i).c_str(), fst, ServerInstance->Config->FollowSymLinks) < 0)
				return std::string();
			
			if (!S_ISDIR(fst->st_mode))
			{
				if (S_ISREG(fst->st_mode))
				{
					// Pathinfo!
					ServerInstance->Log(DEBUG, "PathInfo found: '%s'", std::string(i + 1, fullpath.end()).c_str());
					
					return std::string(fullpath.begin(), i);;
				}
				else
				{
					errno = EACCES;
					return std::string();
				}
			}
		}
	}
	
	if (this->Stat(fullpath.c_str(), fst, ServerInstance->Config->FollowSymLinks) < 0)
		return  std::string();
	
	if (!S_ISREG(fst->st_mode))
	{
		errno = EACCES;
		return std::string();
	}
	
	return fullpath;
}

int FileSystem::Stat(const char *path, struct stat *&buf, bool followlink, bool fromcache)
{
	if (ServerInstance->Config->StatCacheDuration < 1)
	{
		// Cache disabled; simply wrap the call
		buf = &this->static_stat;
		if (followlink)
			return stat(path, &this->static_stat);
		else
			return lstat(path, &this->static_stat);
	}
	
	std::map<std::string,StatCacheItem*> *cache = (followlink) ? &StatCache : &LinkStatCache;
	
	std::map<std::string,StatCacheItem*>::iterator it = cache->find(path);
	if (it != cache->end())
	{
		StatCacheItem *v = it->second;
		
		int expire = v->created + ServerInstance->Config->StatCacheDuration;
		// XXX Make error cache time configurable seperately?
		if (v->result < 0)
			expire = v->created + 1;
		
		if (fromcache && (expire > ServerInstance->Time()))
		{
			ServerInstance->Log(DEBUG, "Providing stat result from cache for %s", path);
			
			buf = &v->value;
			if (v->result < 0)
				errno = v->error;
			return v->result;
		}
		else
		{
			// If fromcache is off, we must delete the cache because it would be overwritten later
			ServerInstance->Log(DEBUG, "Expiring stat cache for %s", path);
			delete v;
			cache->erase(it);
		}
	}
	
	StatCacheItem *result = new StatCacheItem;
	result->created = ServerInstance->Time();
	
	if (followlink)
		result->result = stat(path, &result->value);
	else
		result->result = lstat(path, &result->value);
	
	if (result->result < 0)
		result->error = errno;
	else
		result->error = 0;
	
	cache->insert(std::make_pair<std::string,StatCacheItem*>(path, result));
	
	ServerInstance->Log(DEBUG, "Cached %sstat result (%s) for %s", (followlink) ? "" : "link ", (result->result < 0) ? "error" : "success", path);
	
	buf = &result->value;
	return result->result;
}
