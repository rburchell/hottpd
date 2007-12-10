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

/******************************************************
 *
 * The hash functions of InspIRCd are the centrepoint
 * of the entire system. If these functions are
 * inefficient or wasteful, the whole program suffers
 * as a result. A lot of C programmers in the ircd
 * scene spend a lot of time debating (arguing) about
 * the best way to write hash functions to hash irc
 * nicknames, channels etc.
 * We are lucky as C++ developers as hash_map does
 * a lot of this for us. It does intellegent memory
 * requests, bucketing, search functions, insertion
 * and deletion etc. All we have to do is write some
 * overloaded comparison and hash value operators which
 * cause it to act in an irc-like way. The features we
 * add to the standard hash_map are:
 *
 * Case insensitivity: The hash_map will be case
 * insensitive.
 *
 * Scandanavian Comparisons: The characters [, ], \ will
 * be considered the lowercase of {, } and |.
 *
 ******************************************************/

using namespace irc::sockets;

/* convert a string to lowercase. Note following special circumstances
 * taken from RFC 1459. Many "official" server branches still hold to this
 * rule so i will too;
 *
 *  Because of IRC's scandanavian origin, the characters {}| are
 *  considered to be the lower case equivalents of the characters []\,
 *  respectively. This is a critical issue when determining the
 *  equivalence of two nicknames.
 */
void nspace::strlower(char *n)
{
	if (n)
	{
		for (char* t = n; *t; t++)
			*t = lowermap[(unsigned char)*t];
	}
}

#ifndef WIN32
size_t nspace::hash<string>::operator()(const string &s) const
#else
size_t nspace::hash_compare<string, std::less<string> >::operator()(const string &s) const
#endif
{
	/* XXX: NO DATA COPIES! :)
	 * The hash function here is practically
	 * a copy of the one in STL's hash_fun.h,
	 * only with *x replaced with lowermap[*x].
	 * This avoids a copy to use hash<const char*>
	 */
	register size_t t = 0;
	for (std::string::const_iterator x = s.begin(); x != s.end(); ++x) /* ++x not x++, as its faster */
		t = 5 * t + lowermap[(unsigned char)*x];
	return t;
}

#ifndef WIN32
size_t nspace::hash<irc::string>::operator()(const irc::string &s) const
#else
size_t nspace::hash_compare<irc::string, std::less<irc::string> >::operator()(const irc::string &s) const
#endif
{
	register size_t t = 0;
	for (irc::string::const_iterator x = s.begin(); x != s.end(); ++x) /* ++x not x++, as its faster */
		t = 5 * t + lowermap[(unsigned char)*x];
	return t;
}

bool irc::StrHashComp::operator()(const std::string& s1, const std::string& s2) const
{
	unsigned char* n1 = (unsigned char*)s1.c_str();
	unsigned char* n2 = (unsigned char*)s2.c_str();
	for (; *n1 && *n2; n1++, n2++)
		if (lowermap[*n1] != lowermap[*n2])
			return false;
	return (lowermap[*n1] == lowermap[*n2]);
}

/******************************************************
 *
 * This is the implementation of our special irc::string
 * class which is a case-insensitive equivalent to
 * std::string which is not only case-insensitive but
 * can also do scandanavian comparisons, e.g. { = [, etc.
 *
 * This class depends on the const array 'lowermap'.
 *
 ******************************************************/

bool irc::irc_char_traits::eq(char c1st, char c2nd)
{
	return lowermap[(unsigned char)c1st] == lowermap[(unsigned char)c2nd];
}

bool irc::irc_char_traits::ne(char c1st, char c2nd)
{
	return lowermap[(unsigned char)c1st] != lowermap[(unsigned char)c2nd];
}

bool irc::irc_char_traits::lt(char c1st, char c2nd)
{
	return lowermap[(unsigned char)c1st] < lowermap[(unsigned char)c2nd];
}

int irc::irc_char_traits::compare(const char* str1, const char* str2, size_t n)
{
	for(unsigned int i = 0; i < n; i++)
	{
		if(lowermap[(unsigned char)*str1] > lowermap[(unsigned char)*str2])
			return 1;

		if(lowermap[(unsigned char)*str1] < lowermap[(unsigned char)*str2])
			return -1;

		if(*str1 == 0 || *str2 == 0)
		   	return 0;

		str1++;
		str2++;
	}
	return 0;
}

const char* irc::irc_char_traits::find(const char* s1, int  n, char c)
{
	while(n-- > 0 && lowermap[(unsigned char)*s1] != lowermap[(unsigned char)c])
		s1++;
	return s1;
}

irc::tokenstream::tokenstream(const std::string &source) : tokens(source), last_pushed(false)
{
	/* Record starting position and current position */
	last_starting_position = tokens.begin();
	n = tokens.begin();
}

irc::tokenstream::~tokenstream()
{
}

bool irc::tokenstream::GetToken(std::string &token)
{
	std::string::iterator lsp = last_starting_position;

	while (n != tokens.end())
	{
		/** Skip multi space, converting "  " into " "
		 */
		while ((n+1 != tokens.end()) && (*n == ' ') && (*(n+1) == ' '))
			n++;

		if ((last_pushed) && (*n == ':'))
		{
			/* If we find a token thats not the first and starts with :,
			 * this is the last token on the line
			 */
			std::string::iterator curr = ++n;
			n = tokens.end();
			token = std::string(curr, tokens.end());
			return true;
		}

		last_pushed = false;

		if ((*n == ' ') || (n+1 == tokens.end()))
		{
			/* If we find a space, or end of string, this is the end of a token.
			 */
			last_starting_position = n+1;
			last_pushed = true;

			std::string strip(lsp, n+1 == tokens.end() ? n+1  : n++);
			while ((strip.length()) && (strip.find_last_of(' ') == strip.length() - 1))
				strip.erase(strip.end() - 1);

			token = strip;
			return !token.empty();
		}

		n++;
	}
	token.clear();
	return false;
}

bool irc::tokenstream::GetToken(irc::string &token)
{
	std::string stdstring;
	bool returnval = GetToken(stdstring);
	token = assign(stdstring);
	return returnval;
}

bool irc::tokenstream::GetToken(int &token)
{
	std::string tok;
	bool returnval = GetToken(tok);
	token = ConvToInt(tok);
	return returnval;
}

bool irc::tokenstream::GetToken(long &token)
{
	std::string tok;
	bool returnval = GetToken(tok);
	token = ConvToInt(tok);
	return returnval;
}

irc::sepstream::sepstream(const std::string &source, char seperator) : tokens(source), sep(seperator)
{
	last_starting_position = tokens.begin();
	n = tokens.begin();
}

bool irc::sepstream::GetToken(std::string &token)
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

const std::string irc::sepstream::GetRemaining()
{
	return std::string(n, tokens.end());
}

bool irc::sepstream::StreamEnd()
{
	return ((n + 1) == tokens.end());
}

irc::sepstream::~sepstream()
{
}

std::string irc::hex(const unsigned char *raw, size_t rawsz)
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

CoreExport const char* irc::Spacify(const char* n)
{
	static char x[MAXBUF];
	strlcpy(x,n,MAXBUF);
	for (char* y = x; *y; y++)
		if (*y == '_')
			*y = ' ';
	return x;
}


irc::modestacker::modestacker(bool add) : adding(add)
{
	sequence.clear();
	sequence.push_back("");
}

void irc::modestacker::Push(char modeletter, const std::string &parameter)
{
	*(sequence.begin()) += modeletter;
	sequence.push_back(parameter);
}

void irc::modestacker::Push(char modeletter)
{
	this->Push(modeletter,"");
}

void irc::modestacker::PushPlus()
{
	this->Push('+',"");
}

void irc::modestacker::PushMinus()
{
	this->Push('-',"");
}

int irc::modestacker::GetStackedLine(std::deque<std::string> &result, int max_line_size)
{
	if (sequence.empty())
	{
		result.clear();
		return 0;
	}

	int n = 0;
	int size = 1; /* Account for initial +/- char */
	int nextsize = 0;
	result.clear();
	result.push_back(adding ? "+" : "-");

	if (sequence.size() > 1)
		nextsize = sequence[1].length() + 2;

	while (!sequence[0].empty() && (sequence.size() > 1) && (result.size() < MAXMODES) && ((size + nextsize) < max_line_size))
	{
		result[0] += *(sequence[0].begin());
		if (!sequence[1].empty())
		{
			result.push_back(sequence[1]);
			size += nextsize; /* Account for mode character and whitespace */
		}
		sequence[0].erase(sequence[0].begin());
		sequence.erase(sequence.begin() + 1);

		if (sequence.size() > 1)
			nextsize = sequence[1].length() + 2;

		n++;
	}

	return n;
}

irc::stringjoiner::stringjoiner(const std::string &seperator, const std::vector<std::string> &sequence, int begin, int end)
{
	for (int v = begin; v < end; v++)
		joined.append(sequence[v]).append(seperator);
	joined.append(sequence[end]);
}

irc::stringjoiner::stringjoiner(const std::string &seperator, const std::deque<std::string> &sequence, int begin, int end)
{
	for (int v = begin; v < end; v++)
		joined.append(sequence[v]).append(seperator);
	joined.append(sequence[end]);
}

irc::stringjoiner::stringjoiner(const std::string &seperator, const char** sequence, int begin, int end)
{
	for (int v = begin; v < end; v++)
		joined.append(sequence[v]).append(seperator);
	joined.append(sequence[end]);
}

std::string& irc::stringjoiner::GetJoined()
{
	return joined;
}

irc::portparser::portparser(const std::string &source, bool allow_overlapped) : in_range(0), range_begin(0), range_end(0), overlapped(allow_overlapped)
{
	sep = new irc::commasepstream(source);
	overlap_set.clear();
}

irc::portparser::~portparser()
{
	delete sep;
}

bool irc::portparser::Overlaps(long val)
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

long irc::portparser::GetToken()
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

irc::dynamicbitmask::dynamicbitmask() : bits_size(4)
{
	/* We start with 4 bytes allocated which is room
	 * for 4 items. Something makes me doubt its worth
	 * allocating less than 4 bytes.
	 */
	bits = new unsigned char[bits_size];
	memset(bits, 0, bits_size);
}

irc::dynamicbitmask::~dynamicbitmask()
{
	/* Tidy up the entire used memory on delete */
	delete[] bits;
}

irc::bitfield irc::dynamicbitmask::Allocate()
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

bool irc::dynamicbitmask::Deallocate(irc::bitfield &pos)
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

void irc::dynamicbitmask::Toggle(irc::bitfield &pos, bool state)
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

bool irc::dynamicbitmask::Get(irc::bitfield &pos)
{
	/* Range check the value */
	if (pos.first < bits_size)
		return (bits[pos.first] & pos.second);
	else
		/* We can't return false, otherwise we can't
		 * distinguish between failure and a cleared bit!
		 * Our only sensible choice is to throw (ew).
		 */
		throw ModuleException("irc::dynamicbitmask::Get(): Invalid bitfield, out of range");
}

unsigned char irc::dynamicbitmask::GetSize()
{
	return bits_size;
}

