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

#ifndef __CONNECTION_H__
#define __CONNECTION_H__

#include "inspircd_config.h"
#include "socket.h"
#include "inspstring.h"
#include "hashcomp.h"
#include "backend.h"

/** HTTP socket states
 */
enum HttpState
{
	HTTP_WAIT_REQUEST = 0, /* Waiting for a full request */
	HTTP_RECV_REQBODY, /* Waiting to finish recieving POST data */
	HTTP_SEND_HEADERS, /* Sending response headers */
	HTTP_SEND_DATA, /* Sending response body */
	HTTP_FINISHED
};

enum HttpResponder
{
	HTTP_RESPOND_FLUSH = 0, /* Response is complete when the write bufer is empty */
	HTTP_RESPOND_BACKEND, /* Responding with a file backend */
	HTTP_RESPOND_MODULE /* Response is operating externally; end will be manually signalled */
};

class Backend;

/** A modifyable list of HTTP header fields
 */
class HTTPHeaders
{
 protected:
	typedef std::map<std::string,std::string,utils::StrCaseLess> HeaderMap;
	HeaderMap headers;
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
		HeaderMap::iterator it = headers.find(name);
		if (it == headers.end())
			return std::string();
		
		return it->second;
	}
	
	/** Check if the given header is specified
	 * @return true if the header is specified
	 */
	bool IsSet(const std::string &name)
	{
		HeaderMap::iterator it = headers.find(name);
		return (it != headers.end());
	}
	
	/** Get all headers, formatted by the HTTP protocol
	 * @return Returns all headers, formatted according to the HTTP protocol. There is no request terminator at the end
	 */
	std::string GetFormattedHeaders()
	{
		std::string re;
		
		for (HeaderMap::iterator i = headers.begin(); i != headers.end(); i++)
			re += i->first + ": " + i->second + "\r\n";
		
		return re;
	}
};




/** Holds all information about a connection
 */
class CoreExport Connection : public EventHandler
{
 private:
	/** Pointer to creator.
	 * This is required to make use of core functions
	 * from within the Connection class.
	 */
	InspIRCd* ServerInstance;

	HTTPHeaders headers;
 public:

	HttpState State;

	/** Get IP string from sockaddr, using static internal buffer.
	 * This should not be called after the connection is setup.
	 * @return The IP string
	 */
	const char* GetIPString();

	/** IP of connection.
	 */
	std::string ip;

	/** Connection's send queue.
	 * Lines waiting to be sent are stored here until their buffer is flushed.
	 */
	std::string sendq;

	/** The request buffer from this connection.
	 */
	std::string requestbuf;

	/** The last time socket polled as readable/writable, used by idle timeout code
	 */
	time_t LastSocketEvent;
	
	int RequestsCompleted;
	
	std::string method;
	std::string uri;
	std::string uriquery;
	std::string upath;
	enum
	{
		HTTP_UNSPECIFIED,
		HTTP_1_0,
		HTTP_1_1
	} http_version;
	bool keepalive;
	
	HttpResponder RespondType;
	Backend *ResponseBackend;
	int filefd;
	off_t rfilesize, rfilesent;

	/** If this is set to true, then all read/error operations for the connection
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
	 * @param ip A human-readable IP address for this connection matching the protcol_family
	 * @param port The port number of this connection
	 */
	void SetSockAddr(int protocol_family, const char* ip, int port);

	/** Get port number from sockaddr
	 * @return The port number of this connection.
	 */
	int GetPort();

	/** Get protocol family from sockaddr
	 * @return The protocol family of this connection, either AF_INET or AF_INET6
	 */
	int GetProtocolFamily();

	/* Write error string
	 */
	std::string WriteError;

	/** Default constructor
	 * @param Instance Creator instance
	 */
	Connection(InspIRCd* Instance);

	/** Calls read() to read some data for this connection using their fd and process it.
	 */
	void ReadData();

	bool AddBuffer(const std::string &a);
	
	void HandleURI();
	
	void CheckRequest(int newpos);

	bool CheckFilePath(const std::string &basedir, const std::string &path, struct stat *&fst);

	void ServeData();

	void SendHeaders(unsigned long size, int response, const std::string &rtext, HTTPHeaders &rheaders);

	void SendError(int code, const std::string &text);

	void EndRequest();

	void SendStaticData();
	
	/** Sets the write error for a connection. This is done because the actual disconnect
	 * of a client may occur at an inopportune time such as half way through /LIST output.
	 * The WriteErrors of clients are checked at a more ideal time (in the mainloop) and
	 * errored clients purged.
	 * @param error The error string to set.
	 */
	void SetWriteError(const std::string &error);

	/** Returns the write error which last occured on this connection or an empty string
	 * if none occured.
	 * @return The error string which has occured for this connection
	 */
	const char* GetWriteError();

	/** Adds to the connection's write buffer.
	 * You may add any amount of text up to this connections sendq value, if you exceed the
	 * sendq value, SetWriteError() will be called to set the connections error string to
	 * "SendQ exceeded", and further buffer adds will be dropped.
	 * @param data The data to add to the write buffer
	 */
	void AddWriteBuf(const std::string &data);

	/** Flushes as much of the connection's buffer to the file descriptor as possible.
	 * This function may not always flush the entire buffer, rather instead as much of it
	 * as it possibly can. If the send() call fails to send the entire buffer, the buffer
	 * position is advanced forwards and the rest of the data sent at the next call to
	 * this method.
	 */
	void FlushWriteBuf();

	/** Shuts down and closes the connection's socket
	 * This will not cause the connection to be deleted. Use InspIRCd::QuitConnection for this,
	 * which will call CloseSocket() for you.
	 */
	void CloseSocket();

	/** Write text to this connection
	 * @param text A std::string to send to the connection
	 */
	void Write(const std::string &text);

	/** Write text to this connection
	 * @param text The format string for text to send to the connection
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
	virtual ~Connection();
};

#endif

