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

#ifndef __USERS_H__
#define __USERS_H__

#include <string>
#include "inspircd_config.h"
#include "socket.h"
#include "inspstring.h"
#include "connection.h"
#include "hashcomp.h"

/* Required forward declaration */
class InspIRCd;
class User;

/** Holds all information about a user
 * This class stores all information about a user connected to the irc server. Everything about a
 * connection is stored here primarily, from the user's socket ID (file descriptor) through to the
 * user's nickname and hostname. Use the FindNick method of the InspIRCd class to locate a specific user
 * by nickname, or the FindDescriptor method of the InspIRCd class to find a specific user by their
 * file descriptor value.
 */
class CoreExport User : public connection
{
 private:
	/** Pointer to creator.
	 * This is required to make use of core functions
	 * from within the User class.
	 */
	InspIRCd* ServerInstance;

 public:
	/** The host displayed to non-opers (used for cloaking etc).
	 * This usually matches the value of User::host.
	 */
	std::string dhost;

	/** Timestamp of current time + connection class timeout.
	 * This user must send USER/NICK before this timestamp is
	 * reached or they will be disconnected.
	 */
	time_t timeout;

	/** User's receive queue.
	 * Lines from the IRCd awaiting processing are stored here.
	 * Upgraded april 2005, old system a bit hairy.
	 */
	std::string recvq;

	/** User's send queue.
	 * Lines waiting to be sent are stored here until their buffer is flushed.
	 */
	std::string sendq;

	/** If this is set to true, then all read operations for the user
	 * are dropped into the bit-bucket.
	 * This is used by the global CullList, but please note that setting this value
	 * alone will NOT cause the user to quit. This means it can be used seperately,
	 * for example by shun modules etc.
	 */
	bool muted;

	/** IPV4 or IPV6 ip address. Use SetSockAddr to set this and GetProtocolFamily/
	 * GetIPString/GetPort to obtain its values.
	 */
	sockaddr *ip;

	/** Initialize the clients sockaddr
	 * @param protocol_family The protocol family of the IP address, AF_INET or AF_INET6
	 * @param ip A human-readable IP address for this user matching the protcol_family
	 * @param port The port number of this user or zero for a remote user
	 */
	void SetSockAddr(int protocol_family, const char* ip, int port);

	/** Get port number from sockaddr
	 * @return The port number of this user.
	 */
	int GetPort();

	/** Get protocol family from sockaddr
	 * @return The protocol family of this user, either AF_INET or AF_INET6
	 */
	int GetProtocolFamily();

	/** Get IP string from sockaddr, using static internal buffer
	 * @return The IP string
	 */
	const char* GetIPString();

	/* Write error string
	 */
	std::string WriteError;

	/** This is true if the user matched an exception (E:Line). It is used to save time on ban checks.
	 */
	bool exempt;

	/** This value contains how far into the penalty threshold the user is. Once its over
	 * the penalty threshold then commands are held and processed on-timer.
	 */
	int Penalty;

	/** True if we are flushing penalty lines
	 */
	bool OverPenalty;

	/** If this bool is set then penalty rules do not apply to this user
	 */
	bool ExemptFromPenalty;

	/** Default constructor
	 * @throw CoreException if the UID allocated to the user already exists
	 * @param Instance Creator instance
	 */
	User(InspIRCd* Instance);

	/** Calls read() to read some data for this user using their fd.
	 * @param buffer The buffer to read into
	 * @param size The size of data to read
	 * @return The number of bytes read, or -1 if an error occured.
	 */
	int ReadData(void* buffer, size_t size);

	/** This method adds data to the read buffer of the user.
	 * The buffer can grow to any size within limits of the available memory,
	 * managed by the size of a std::string, however if any individual line in
	 * the buffer grows over 600 bytes in length (which is 88 chars over the
	 * RFC-specified limit per line) then the method will return false and the
	 * text will not be inserted.
	 * @param a The string to add to the users read buffer
	 * @return True if the string was successfully added to the read buffer
	 */
	bool AddBuffer(std::string a);

	/** This method returns true if the buffer contains at least one carriage return
	 * character (e.g. one complete line may be read)
	 * @return True if there is at least one complete line in the users buffer
	 */
	bool BufferIsReady();

	/** This function clears the entire buffer by setting it to an empty string.
	 */
	void ClearBuffer();

	/** This method returns the first available string at the tail end of the buffer
	 * and advances the tail end of the buffer past the string. This means it is
	 * a one way operation in a similar way to strtok(), and multiple calls return
	 * multiple lines if they are available. The results of this function if there
	 * are no lines to be read are unknown, always use BufferIsReady() to check if
	 * it is ok to read the buffer before calling GetBuffer().
	 * @return The string at the tail end of this users buffer
	 */
	std::string GetBuffer();

	/** Sets the write error for a connection. This is done because the actual disconnect
	 * of a client may occur at an inopportune time such as half way through /LIST output.
	 * The WriteErrors of clients are checked at a more ideal time (in the mainloop) and
	 * errored clients purged.
	 * @param error The error string to set.
	 */
	void SetWriteError(const std::string &error);

	/** Returns the write error which last occured on this connection or an empty string
	 * if none occured.
	 * @return The error string which has occured for this user
	 */
	const char* GetWriteError();

	/** Adds to the user's write buffer.
	 * You may add any amount of text up to this users sendq value, if you exceed the
	 * sendq value, SetWriteError() will be called to set the users error string to
	 * "SendQ exceeded", and further buffer adds will be dropped.
	 * @param data The data to add to the write buffer
	 */
	void AddWriteBuf(const std::string &data);

	/** Flushes as much of the user's buffer to the file descriptor as possible.
	 * This function may not always flush the entire buffer, rather instead as much of it
	 * as it possibly can. If the send() call fails to send the entire buffer, the buffer
	 * position is advanced forwards and the rest of the data sent at the next call to
	 * this method.
	 */
	void FlushWriteBuf();

	/** Shuts down and closes the user's socket
	 * This will not cause the user to be deleted. Use InspIRCd::QuitUser for this,
	 * which will call CloseSocket() for you.
	 */
	void CloseSocket();

	/** Disconnect a user gracefully
	 * @param user The user to remove
	 * @param r The quit reason to show to normal users
	 * @param oreason The quit reason to show to opers
	 * @return Although this function has no return type, on exit the user provided will no longer exist.
	 */
	static void QuitUser(InspIRCd* Instance, User *user, const std::string &r, const char* oreason = "");

	/** Use this method to fully connect a user.
	 * This will send the message of the day, check G/K/E lines, etc.
	 */
	void FullConnect();

	/** Add a client to the system.
	 * This will create a new User, insert it into the user_hash,
	 * initialize it as not yet registered, and add it to the socket engine.
	 * @param Instance a pointer to the server instance
	 * @param socket The socket id (file descriptor) this user is on
	 * @param port The port number this user connected on
	 * @param iscached This variable is reserved for future use
	 * @param ip The IP address of the user
	 * @return This function has no return value, but a call to AddClient may remove the user.
	 */
	static void AddClient(InspIRCd* Instance, int socket, int port, bool iscached, int socketfamily, sockaddr* ip);

	/** Return the number of local clones of this user
	 * @return The local clone count of this user
	 */
	unsigned long LocalCloneCount();

	/** Remove all clone counts from the user, you should
	 * use this if you change the user's IP address in
	 * User::ip after they have registered.
	 */
	void RemoveCloneCounts();

	/** Write text to this user, appending CR/LF.
	 * @param text A std::string to send to the user
	 */
	void Write(std::string text);

	/** Write text to this user, appending CR/LF.
	 * @param text The format string for text to send to the user
	 * @param ... POD-type format arguments
	 */
	void Write(const char *text, ...);

	/** Handle socket event.
	 * From EventHandler class.
	 * @param et Event type
	 * @param errornum Error number for EVENT_ERROR events
	 */
	void HandleEvent(EventType et, int errornum = 0);

	/** Default destructor
	 */
	virtual ~User();
};

/* Configuration callbacks */
//class ServerConfig;

#endif

