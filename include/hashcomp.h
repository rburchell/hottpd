/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef _HASHCOMP_H_
#define _HASHCOMP_H_

#include "inspircd_config.h"
#include "socket.h"
#include "hash_map.h"
#include <algorithm>

/** Required namespaces and symbols */
using namespace std;

/** aton() */
using utils::sockets::insp_aton;

/** nota() */
using utils::sockets::insp_ntoa;

namespace utils
{
	struct StrCaseLess
	{
		static bool charicmp(char c1, char c2)
		{
			return tolower(c1) < tolower(c2);
		}
		
		bool operator()(const std::string &s1, const std::string &s2) const
		{
			return std::lexicographical_compare(s1.begin(), s1.end(), s2.begin(), s2.end(), charicmp);
		}
	};

	/** Compose a hex string from raw data.
	 * @param raw The raw data to compose hex from
	 * @pram rawsz The size of the raw data buffer
	 * @return The hex string.
	 */
	CoreExport std::string hex(const unsigned char *raw, size_t rawsz);
	
	/** Convert two hex characters into the byte they represent
	 * @param dest The location to place the decoded byte
	 * @param a The first hex byte (first 4 bits)
	 * @param b The second hex byte (last 4 bits)
	 * @return True for success or false for failure (invalid chars)
	 */
	CoreExport bool unhexchar(char &dest, char a, char b);
	
	/** utils::stringjoiner joins string lists into a string, using
	 * the given seperator string.
	 * This class can join a vector of std::string, a deque of
	 * std::string, or a const char** array, using overloaded
	 * constructors.
	 */
	class CoreExport stringjoiner
	{
	 private:
		/** Output string
		 */
		std::string joined;
	 public:
		/** Join elements of a vector, between (and including) begin and end
		 * @param seperator The string to seperate values with
		 * @param sequence One or more items to seperate
		 * @param begin The starting element in the sequence to be joined
		 * @param end The ending element in the sequence to be joined
		 */
		stringjoiner(const std::string &seperator, const std::vector<std::string> &sequence, int begin, int end);
		/** Join elements of a deque, between (and including) begin and end
		 * @param seperator The string to seperate values with
		 * @param sequence One or more items to seperate
		 * @param begin The starting element in the sequence to be joined
		 * @param end The ending element in the sequence to be joined
		 */
		stringjoiner(const std::string &seperator, const std::deque<std::string> &sequence, int begin, int end);
		/** Join elements of an array of char arrays, between (and including) begin and end
		 * @param seperator The string to seperate values with
		 * @param sequence One or more items to seperate
		 * @param begin The starting element in the sequence to be joined
		 * @param end The ending element in the sequence to be joined
		 */
		stringjoiner(const std::string &seperator, const char** sequence, int begin, int end);

		/** Get the joined sequence
		 * @return A reference to the joined string
		 */
		std::string& GetJoined();
	};
	
	/** utils::sepstream allows for splitting token seperated lists.
	 * Each successive call to sepstream::GetToken() returns
	 * the next token, until none remain, at which point the method returns
	 * an empty string.
	 */
	class CoreExport sepstream : public classbase
	{
	 private:
		/** Original string.
		 */
		std::string tokens;
		/** Last position of a seperator token
		 */
		std::string::iterator last_starting_position;
		/** Current string position
		 */
		std::string::iterator n;
		/** Seperator value
		 */
		char sep;
	 public:
		/** Create a sepstream and fill it with the provided data
		 */
		sepstream(const std::string &source, char seperator);
		virtual ~sepstream();

		/** Fetch the next token from the stream
		 * @param token The next token from the stream is placed here
		 * @return True if tokens still remain, false if there are none left
		 */
		virtual bool GetToken(std::string &token);
		
		/** Fetch the entire remaining stream, without tokenizing
		 * @return The remaining part of the stream
		 */
		virtual const std::string GetRemaining();
		
		/** Returns true if the end of the stream has been reached
		 * @return True if the end of the stream has been reached, otherwise false
		 */
		virtual bool StreamEnd();
	};

	/** A derived form of sepstream, which seperates on commas
	 */
	class CoreExport commasepstream : public sepstream
	{
	 public:
		/** Initialize with comma seperator
		 */
		commasepstream(const std::string &source) : sepstream(source, ',')
		{
		}
	};

	/** A derived form of sepstream, which seperates on spaces
	 */
	class CoreExport spacesepstream : public sepstream
	{
	 public:
		/** Initialize with space seperator
		 */
		spacesepstream(const std::string &source) : sepstream(source, ' ')
		{
		}
	};

	/** The portparser class seperates out a port range into integers.
	 * A port range may be specified in the input string in the form
	 * "6660,6661,6662-6669,7020". The end of the stream is indicated by
	 * a return value of 0 from portparser::GetToken(). If you attempt
	 * to specify an illegal range (e.g. one where start >= end, or
	 * start or end < 0) then GetToken() will return the first element
	 * of the pair of numbers.
	 */
	class CoreExport portparser : public classbase
	{
	 private:
		/** Used to split on commas
		 */
		commasepstream* sep;
		/** Current position in a range of ports
		 */
		long in_range;
		/** Starting port in a range of ports
		 */
		long range_begin;
		/** Ending port in a range of ports
		 */
		long range_end;
		/** Allow overlapped port ranges
		 */
		bool overlapped;
		/** Used to determine overlapping of ports
		 * without O(n) algorithm being used
		 */
		std::map<long, bool> overlap_set;
		/** Returns true if val overlaps an existing range
		 */
		bool Overlaps(long val);
	 public:
		/** Create a portparser and fill it with the provided data
		 * @param source The source text to parse from
		 * @param allow_overlapped Allow overlapped ranges
		 */
		portparser(const std::string &source, bool allow_overlapped = true);
		/** Frees the internal commasepstream object
		 */
		~portparser();
		/** Fetch the next token from the stream
		 * @return The next port number is returned, or 0 if none remain
		 */
		long GetToken();
	};

	/** Used to hold a bitfield definition in dynamicbitmask.
	 * You must be allocated one of these by dynamicbitmask::Allocate(),
	 * you should not fill the values yourself!
	 */
	typedef std::pair<size_t, unsigned char> bitfield;

	/** The utils::dynamicbitmask class is used to maintain a bitmap of
	 * boolean values, which can grow to any reasonable size no matter
	 * how many bitfields are in it.
	 *
	 * It starts off at 32 bits in size, large enough to hold 32 boolean
	 * values, with a memory allocation of 8 bytes. If you allocate more
	 * than 32 bits, the class will grow the bitmap by 8 bytes at a time
	 * for each set of 8 bitfields you allocate with the Allocate()
	 * method.
	 *
	 * This method is designed so that multiple modules can be allocated
	 * bit values in a bitmap dynamically, rather than having to define
	 * costs in a fixed size unsigned integer and having the possibility
	 * of collisions of values in different third party modules.
	 *
	 * IMPORTANT NOTE:
	 *
	 * To use this class, you must derive from it.
	 * This is because each derived instance has its own freebits array
	 * which can determine what bitfields are allocated on a TYPE BY TYPE
	 * basis, e.g. an utils::dynamicbitmask type for users, and one for
	 * Channels, etc. You should inheret it in a very simple way as follows.
	 * The base class will resize and maintain freebits as required, you are
	 * just required to make the pointer static and specific to this class
	 * type.
	 *
	 * \code
	 * class mydbitmask : public utils::dynamicbitmask
	 * {
	 *  private:
	 *
	 *      static unsigned char* freebits;
	 *
	 *  public:
	 *
	 *      mydbitmask() : utils::dynamicbitmask()
	 *      {
	 *	  freebits = new unsigned char[this->bits_size];
	 *	  memset(freebits, 0, this->bits_size);
	 *      }
	 *
	 *      ~mydbitmask()
	 *      {
	 *	  delete[] freebits;
	 *      }
	 *
	 *      unsigned char* GetFreeBits()
	 *      {
	 *	  return freebits;
	 *      }
	 *
	 *      void SetFreeBits(unsigned char* freebt)
	 *      {
	 *	  freebits = freebt;
	 *      }
	 * };
	 * \endcode
	 */
	class CoreExport dynamicbitmask : public classbase
	{
	 private:
		/** Data bits. We start with four of these,
		 * and we grow the bitfield as we allocate
		 * more than 32 entries with Allocate().
		 */
		unsigned char* bits;
	 protected:
		/** Current set size (size of freebits and bits).
		 * Both freebits and bits will ALWAYS be the
		 * same length.
		 */
		unsigned char bits_size;
	 public:
		/** Allocate the initial memory for bits and
		 * freebits and zero the memory.
		 */
		dynamicbitmask();

		/** Free the memory used by bits and freebits
		 */
		virtual ~dynamicbitmask();

		/** Allocate an utils::bitfield.
		 * @return An utils::bitfield which can be used
		 * with Get, Deallocate and Toggle methods.
		 * @throw Can throw std::bad_alloc if there is
		 * no ram left to grow the bitmask.
		 */
		bitfield Allocate();

		/** Deallocate an utils::bitfield.
		 * @param An utils::bitfield to deallocate.
		 * @return True if the bitfield could be
		 * deallocated, false if it could not.
		 */
		bool Deallocate(bitfield &pos);

		/** Toggle the value of a bitfield.
		 * @param pos A bitfield to allocate, previously
		 * allocated by dyamicbitmask::Allocate().
		 * @param state The state to set the field to.
		 */
		void Toggle(bitfield &pos, bool state);

		/** Get the value of a bitfield.
		 * @param pos A bitfield to retrieve, previously
		 * allocated by dyamicbitmask::Allocate().
		 * @return The value of the bitfield.
		 * @throw Will throw ModuleException if the bitfield
		 * you provide is outside of the range
		 * 0 >= bitfield.first < size_bits.
		 */
		bool Get(bitfield &pos);

		/** Return the size in bytes allocated to the bits
		 * array.
		 * Note that the actual allocation is twice this,
		 * as there are an equal number of bytes allocated
		 * for the freebits array.
		 */
		unsigned char GetSize();

		/** Get free bits mask
		 */
		virtual unsigned char* GetFreeBits() { return NULL; }

		/** Set free bits mask
		 */
		virtual void SetFreeBits(unsigned char* freebits) { freebits = freebits; }
	};

	/** Trim the leading and trailing spaces from a std::string.
	 */
	inline std::string& trim(std::string &str)
	{
		std::string::size_type start = str.find_first_not_of(" ");
		std::string::size_type end = str.find_last_not_of(" ");
		if (start == std::string::npos || end == std::string::npos)
			str = "";
		else
			str = str.substr(start, end-start+1);
		
		return str;
	}
}

#endif

