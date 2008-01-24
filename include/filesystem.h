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

#ifndef __FILESYSTEM_H__
#define __FILESYSTEM_H__

#include "inspircd_config.h"
#include "socket.h"
#include "inspstring.h"
#include "hashcomp.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

struct StatCacheItem
{
	time_t created;
	struct stat value;
	int result;
	int error;
};

class CoreExport FileSystem
{
 protected:
	InspIRCd *ServerInstance;
	/* XXX - This needs a background timer that will run through and remove expired
	 * entries! Right now, any file that is requested once will use up memory forever.
	 */
	std::map<std::string,StatCacheItem*> StatCache;
	
	/* Static buffer used for stat results when caching is disabled */
	struct stat static_stat;
 public:
	FileSystem(InspIRCd *Instance);
	
	int Stat(const char *path, struct stat *&buf, bool fromcache = true);
};

#endif
