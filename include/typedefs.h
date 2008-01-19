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

/** A cached text file stored line by line.
 */
typedef std::deque<std::string> file_cache;

#endif

