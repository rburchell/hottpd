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

#include "inspircd_config.h"
#include "socket.h"
#include "inspstring.h"
#include "hashcomp.h"


/** HTTP socket states
 */
enum HttpState
{
	HTTP_WAIT_REQUEST = 0, /* Waiting for a full request */
	HTTP_RECV_POSTDATA = 1, /* Waiting to finish recieving POST data */
	HTTP_SEND_DATA = 2, /* Sending response */
	HTTP_FINISHED = 3
};


/** A modifyable list of HTTP header fields
 */
class HTTPHeaders
{
 protected:
	std::map<std::string,std::string> headers;
 public:
	
	/** Set the value of a header
	 * Sets the value of the named header. If the header is already present, it will be replaced
	 */
	void SetHeader(const std::string &name, const std::string &data)
	{
		headers[name] = data;
	}
	
	/** Set the value of a header, only if it doesn't exist already
	 * Sets the value of the named header. If the header is already present, it will NOT be updated
	 */
	void CreateHeader(const std::string &name, const std::string &data)
	{
		if (!IsSet(name))
			SetHeader(name, data);
	}
	
	/** Remove the named header
	 */
	void RemoveHeader(const std::string &name)
	{
		headers.erase(name);
	}
	
	/** Remove all headers
	 */
	void Clear()
	{
		headers.clear();
	}
	
	/** Get the value of a header
	 * @return The value of the header, or an empty string
	 */
	std::string GetHeader(const std::string &name)
	{
		std::map<std::string,std::string>::iterator it = headers.find(name);
		if (it == headers.end())
			return std::string();
		
		return it->second;
	}
	
	/** Check if the given header is specified
	 * @return true if the header is specified
	 */
	bool IsSet(const std::string &name)
	{
		std::map<std::string,std::string>::iterator it = headers.find(name);
		return (it != headers.end());
	}
	
	/** Get all headers, formatted by the HTTP protocol
	 * @return Returns all headers, formatted according to the HTTP protocol. There is no request terminator at the end
	 */
	std::string GetFormattedHeaders()
	{
		std::string re;
		
		for (std::map<std::string,std::string>::iterator i = headers.begin(); i != headers.end(); i++)
			re += i->first + ": " + i->second + "\r\n";
		
		return re;
	}
};




/** Holds all information about a user
 * This class stores all information about a user connected to the irc server. Everything about a
 * connection is stored here primarily, from the user's socket ID (file descriptor) through to the
 * user's nickname and hostname. Use the FindNick method of the InspIRCd class to locate a specific user
 * by nickname, or the FindDescriptor method of the InspIRCd class to find a specific user by their
 * file descriptor value.
 */
class CoreExport User : public EventHandler
{
 private:
	/** Pointer to creator.
	 * This is required to make use of core functions
	 * from within the User class.
	 */
	InspIRCd* ServerInstance;

	HttpState State;

	HTTPHeaders headers;

	/** Get IP string from sockaddr, using static internal buffer
	 * @return The IP string
	 */
	const char* GetIPString();
 public:
	/** IP of connection.
	 */
	std::string ip;

	/** User's send queue.
	 * Lines waiting to be sent are stored here until their buffer is flushed.
	 */
	std::string sendq;

	/** The request buffer from this connection.
	 */
	std::string requestbuf;

	std::string request_type;
	std::string uri;
	std::string http_version;
	bool keepalive;

	/** If this is set to true, then all read/error operations for the user
	 * are dropped into the bit-bucket.
	 * This is used by the global CullList.
	 */
	bool quitting;

	/** IPV4 or IPV6 ip address. Use SetSockAddr to set this and GetProtocolFamily/
	 * GetIPString/GetPort to obtain its values.
	 */
	sockaddr *privip;

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

	/* Write error string
	 */
	std::string WriteError;

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

	bool AddBuffer(const std::string &a);

	void CheckRequest();

	void ServeData();

	void SendHeaders(unsigned long size, int response, HTTPHeaders &rheaders);

	void SendError(int code);

	void ResetRequest();

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
	 * @return Although this function has no return type, on exit the user provided will no longer exist.
	 */
	static void QuitUser(InspIRCd* Instance, User *user);

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

	/** Write text to this user
	 * @param text A std::string to send to the user
	 */
	void Write(const std::string &text);

	/** Write text to this user
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

#endif

