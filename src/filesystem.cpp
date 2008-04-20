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
