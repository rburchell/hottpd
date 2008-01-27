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

/* $Core: libIRCDhash */

#include "inspircd.h"
#include "hashcomp.h"
#ifndef WIN32
#include <ext/hash_map>
#define nspace __gnu_cxx
#else
#include <hash_map>
#define nspace stdext
using stdext::hash_map;
#endif

using namespace utils::sockets;

utils::sepstream::sepstream(const std::string &source, char seperator) : tokens(source), sep(seperator)
{
	last_starting_position = tokens.begin();
	n = tokens.begin();
}

bool utils::sepstream::GetToken(std::string &token)
{
	std::string::iterator lsp = last_starting_position;

	while (n != tokens.end())
	{
		if ((*n == sep) || (n+1 == tokens.end()))
		{
			last_starting_position = n+1;
			token = std::string(lsp, n+1 == tokens.end() ? n+1  : n++);

			while ((token.length()) && (token.find_last_of(sep) == token.length() - 1))
				token.erase(token.end() - 1);

			if (token.empty())
				n++;

			return n == tokens.end() ? false : true;
		}

		n++;
	}

	token = "";
	return false;
}

const std::string utils::sepstream::GetRemaining()
{
	return std::string(n, tokens.end());
}

bool utils::sepstream::StreamEnd()
{
	return ((n + 1) == tokens.end());
}

utils::sepstream::~sepstream()
{
}

std::string utils::hex(const unsigned char *raw, size_t rawsz)
{
	if (!rawsz)
		return "";

	/* EWW! This used to be using sprintf, which is WAY inefficient. -Special */
	
	const char *hex = "0123456789abcdef";
	static char hexbuf[MAXBUF];

	size_t i, j;
	for (i = 0, j = 0; j < rawsz; ++j)
	{
		hexbuf[i++] = hex[raw[j] / 16];
		hexbuf[i++] = hex[raw[j] % 16];
	}
	hexbuf[i] = 0;

	return hexbuf;
}

unsigned char utils::unhexchar(char a, char b)
{
	static const char *hex = "0123456789abcdef";
	
}

utils::stringjoiner::stringjoiner(const std::string &seperator, const std::vector<std::string> &sequence, int begin, int end)
{
	for (int v = begin; v < end; v++)
		joined.append(sequence[v]).append(seperator);
	joined.append(sequence[end]);
}

utils::stringjoiner::stringjoiner(const std::string &seperator, const std::deque<std::string> &sequence, int begin, int end)
{
	for (int v = begin; v < end; v++)
		joined.append(sequence[v]).append(seperator);
	joined.append(sequence[end]);
}

utils::stringjoiner::stringjoiner(const std::string &seperator, const char** sequence, int begin, int end)
{
	for (int v = begin; v < end; v++)
		joined.append(sequence[v]).append(seperator);
	joined.append(sequence[end]);
}

std::string& utils::stringjoiner::GetJoined()
{
	return joined;
}

utils::portparser::portparser(const std::string &source, bool allow_overlapped) : in_range(0), range_begin(0), range_end(0), overlapped(allow_overlapped)
{
	sep = new utils::commasepstream(source);
	overlap_set.clear();
}

utils::portparser::~portparser()
{
	delete sep;
}

bool utils::portparser::Overlaps(long val)
{
	if (!overlapped)
		return false;

	if (overlap_set.find(val) == overlap_set.end())
	{
		overlap_set[val] = true;
		return false;
	}
	else
		return true;
}

long utils::portparser::GetToken()
{
	if (in_range > 0)
	{
		in_range++;
		if (in_range <= range_end)
		{
			if (!Overlaps(in_range))
			{
				return in_range;
			}
			else
			{
				while (((Overlaps(in_range)) && (in_range <= range_end)))
					in_range++;
				
				if (in_range <= range_end)
					return in_range;
			}
		}
		else
			in_range = 0;
	}

	std::string x;
	sep->GetToken(x);

	if (x.empty())
		return 0;

	while (Overlaps(atoi(x.c_str())))
	{
		if (!sep->GetToken(x))
			return 0;
	}

	std::string::size_type dash = x.rfind('-');
	if (dash != std::string::npos)
	{
		std::string sbegin = x.substr(0, dash);
		std::string send = x.substr(dash+1, x.length());
		range_begin = atoi(sbegin.c_str());
		range_end = atoi(send.c_str());

		if ((range_begin > 0) && (range_end > 0) && (range_begin < 65536) && (range_end < 65536) && (range_begin < range_end))
		{
			in_range = range_begin;
			return in_range;
		}
		else
		{
			/* Assume its just the one port */
			return atoi(sbegin.c_str());
		}
	}
	else
	{
		return atoi(x.c_str());
	}
}

utils::dynamicbitmask::dynamicbitmask() : bits_size(4)
{
	/* We start with 4 bytes allocated which is room
	 * for 4 items. Something makes me doubt its worth
	 * allocating less than 4 bytes.
	 */
	bits = new unsigned char[bits_size];
	memset(bits, 0, bits_size);
}

utils::dynamicbitmask::~dynamicbitmask()
{
	/* Tidy up the entire used memory on delete */
	delete[] bits;
}

utils::bitfield utils::dynamicbitmask::Allocate()
{
	/* Yeah, this isnt too efficient, however a module or the core
	 * should only be allocating bitfields on load, the Toggle and
	 * Get methods are O(1) as these are called much more often.
	 */
	unsigned char* freebits = this->GetFreeBits();
	for (unsigned char i = 0; i < bits_size; i++)
	{
		/* Yes, this is right. You'll notice we terminate the  loop when !current_pos,
		 * this is because we logic shift our bit off the end of unsigned char, and its
		 * lost, making the loop counter 0 when we're done.
		 */
		for (unsigned char current_pos = 1; current_pos; current_pos = current_pos << 1)
		{
			if (!(freebits[i] & current_pos))
			{
				freebits[i] |= current_pos;
				return std::make_pair(i, current_pos);
			}
		}
	}
	/* We dont have any free space left, increase by one */

	if (bits_size == 255)
		/* Oh dear, cant grow it any further */
		throw std::bad_alloc();

	unsigned char old_bits_size = bits_size;
	bits_size++;
	/* Allocate new bitfield space */
	unsigned char* temp_bits = new unsigned char[bits_size];
	unsigned char* temp_freebits = new unsigned char[bits_size];
	/* Copy the old data in */
	memcpy(temp_bits, bits, old_bits_size);
	memcpy(temp_freebits, freebits, old_bits_size);
	/* Delete the old data pointers */
	delete[] bits;
	delete[] freebits;
	/* Swap the pointers over so now the new 
	 * pointers point to our member values
	 */
	bits = temp_bits;
	freebits = temp_freebits;
	this->SetFreeBits(freebits);
	/* Initialize the new byte on the end of
	 * the bitfields, pre-allocate the one bit
	 * for this allocation
	 */
	bits[old_bits_size] = 0;
	freebits[old_bits_size] = 1;
	/* We already know where we just allocated
	 * the bitfield, so no loop needed
	 */
	return std::make_pair(old_bits_size, 1);
}

bool utils::dynamicbitmask::Deallocate(utils::bitfield &pos)
{
	/* We dont bother to shrink the bitfield
	 * on deallocation, the most we could do
	 * is save one byte (!) and this would cost
	 * us a loop (ugly O(n) stuff) so we just
	 * clear the bit and leave the memory
	 * claimed -- nobody will care about one
	 * byte.
	 */
	if (pos.first < bits_size)
	{
		this->GetFreeBits()[pos.first] &= ~pos.second;
		return true;
	}
	/* They gave a bitfield outside of the
	 * length of our array. BAD programmer.
	 */
	return false;
}

void utils::dynamicbitmask::Toggle(utils::bitfield &pos, bool state)
{
	/* Range check the value */
	if (pos.first < bits_size)
	{
		if (state)
			/* Set state, OR the state in */
			bits[pos.first] |= pos.second;
		else
			/* Clear state, AND the !state out */
			bits[pos.first] &= ~pos.second;
	}
}

bool utils::dynamicbitmask::Get(utils::bitfield &pos)
{
	/* Range check the value */
	if (pos.first < bits_size)
		return (bits[pos.first] & pos.second);
	else
		/* We can't return false, otherwise we can't
		 * distinguish between failure and a cleared bit!
		 * Our only sensible choice is to throw (ew).
		 */
		throw ModuleException("utils::dynamicbitmask::Get(): Invalid bitfield, out of range");
}

unsigned char utils::dynamicbitmask::GetSize()
{
	return bits_size;
}

