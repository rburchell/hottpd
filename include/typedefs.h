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

#ifndef __TYPEDEF_H__
#define __TYPEDEF_H__

#include <string>
#include "inspircd_config.h"
#include "hash_map.h"
#include "hashcomp.h"

class User;
class Channel;

#ifndef WIN32
/** User hash (POSIX systems with GCC)
 */
typedef nspace::hash_map<std::string, User*, nspace::hash<string>, irc::StrHashComp> user_hash;
/** Channel hash (POSIX systems with GCC)
 */
typedef nspace::hash_map<std::string, Channel*, nspace::hash<string>, irc::StrHashComp> chan_hash;
#else
/** User hash (windows systems with visual studio)
 */
typedef nspace::hash_map<std::string, User*, nspace::hash_compare<string, less<string> > > user_hash;
/** Channel hash (windows systems with visual studio)
 */
typedef nspace::hash_map<std::string, Channel*, nspace::hash_compare<string, less<string> > > chan_hash;
#endif

/** Server name cache
 */
typedef std::vector<std::string*> servernamelist;

/** A cached text file stored line by line.
 */
typedef std::deque<std::string> file_cache;

#endif

