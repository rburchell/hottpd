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

/* $Core: libIRCDbase */

#include "inspircd_config.h"
#include "base.h"
#include <time.h>
#include "inspircd.h"

const int bitfields[]           =       {1,2,4,8,16,32,64,128};
const int inverted_bitfields[]  =       {~1,~2,~4,~8,~16,~32,~64,~128};

classbase::classbase()
{
	this->age = time(NULL);
}

bool Extensible::Shrink(const std::string &key)
{
	/* map::size_type map::erase( const key_type& key );
	 * returns the number of elements removed, std::map
	 * is single-associative so this should only be 0 or 1
	 */
	return this->Extension_Items.erase(key);
}

void Extensible::GetExtList(std::deque<std::string> &list)
{
	for (ExtensibleStore::iterator u = Extension_Items.begin(); u != Extension_Items.end(); u++)
	{
		list.push_back(u->first);
	}
}

void BoolSet::Set(int number)
{
	this->bits |= bitfields[number];
}

void BoolSet::Unset(int number)
{
	this->bits &= inverted_bitfields[number];
}

void BoolSet::Invert(int number)
{
	this->bits ^= bitfields[number];
}

bool BoolSet::Get(int number)
{
	return ((this->bits | bitfields[number]) > 0);
}

bool BoolSet::operator==(BoolSet other)
{
	return (this->bits == other.bits);
}

BoolSet BoolSet::operator|(BoolSet other)
{
	BoolSet x(this->bits | other.bits);
	return x;
}

BoolSet BoolSet::operator&(BoolSet other)
{
	BoolSet x(this->bits & other.bits);
	return x;
}

BoolSet::BoolSet()
{
	this->bits = 0;
}

BoolSet::BoolSet(char bitmask)
{
	this->bits = bitmask;
}

bool BoolSet::operator=(BoolSet other)
{
	this->bits = other.bits;
	return true;
}
