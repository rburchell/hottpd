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

/* $Core: libhttpd_connections */

#include "inspircd.h"
#include <stdarg.h>
#include "socketengine.h"
#include "wildcard.h"

Connection::Connection(InspIRCd* Instance) : ServerInstance(Instance)
{
	quitting = false;
	fd = filefd = -1;
	privip = NULL;
	State = HTTP_WAIT_REQUEST;
	http_version = HTTP_UNSPECIFIED;
	keepalive = true;
	RequestBodyLength = 0;
	rfilesize = rfilesent = RequestsCompleted = 0;
	LastSocketEvent = ServerInstance->Time();
	ResponseBackend = NULL;
	RespondType = HTTP_RESPOND_FLUSH;
}

Connection::~Connection()
{
	if (privip)
	{
		if (this->GetProtocolFamily() == AF_INET)
		{
			delete (sockaddr_in*)privip;
		}
#ifdef SUPPORT_IP6LINKS
		else
		{
			delete (sockaddr_in6*)privip;
		}
#endif
	}
	
	if (filefd > -1)
	{
		close(filefd);
		filefd = -1;
	}
}

void Connection::CloseSocket()
{
	ServerInstance->SE->Shutdown(this, 2);
	ServerInstance->SE->Close(this);
}

void Connection::SendError(int code, const std::string &text)
{
	HTTPHeaders empty;
	empty.SetHeader("Content-Type", "text/html");
	std::string data = "<html><head></head><body>" + text + "<br><small>Powered by Hottpd</small></body></html>";
	RespondType = HTTP_RESPOND_FLUSH;
	this->SendHeaders(data.length(), code, text, empty);
	State = HTTP_SEND_DATA;
	this->Write(data);
	// Flush the write buffer now instead of waiting an iteration, since we've written all we need to
	this->FlushWriteBuf();
	// End request will be triggered once the write buffer is empty
}

void Connection::SetSockAddr(int protocol_family, const char* ip, int port)
{
	switch (protocol_family)
	{
#ifdef SUPPORT_IP6LINKS
		case AF_INET6:
		{
			sockaddr_in6* sin = new sockaddr_in6;
			sin->sin6_family = AF_INET6;
			sin->sin6_port = port;
			inet_pton(AF_INET6, ip, &sin->sin6_addr);
			this->privip = (sockaddr*)sin;
		}
		break;
#endif
		case AF_INET:
		{
			sockaddr_in* sin = new sockaddr_in;
			sin->sin_family = AF_INET;
			sin->sin_port = port;
			inet_pton(AF_INET, ip, &sin->sin_addr);
			this->privip = (sockaddr*)sin;
		}
		break;
		default:
			ServerInstance->Log(DEBUG,"Uh oh, I dont know protocol %d to be set!", protocol_family);
		break;
	}
}

int Connection::GetPort()
{
	if (this->privip == NULL)
		return 0;

	switch (this->GetProtocolFamily())
	{
#ifdef SUPPORT_IP6LINKS
		case AF_INET6:
		{
			sockaddr_in6* sin = (sockaddr_in6*)this->privip;
			return sin->sin6_port;
		}
		break;
#endif
		case AF_INET:
		{
			sockaddr_in* sin = (sockaddr_in*)this->privip;
			return sin->sin_port;
		}
		break;
		default:
		break;
	}
	return 0;
}

int Connection::GetProtocolFamily()
{
	if (this->privip == NULL)
		return 0;

	sockaddr_in* sin = (sockaddr_in*)this->privip;
	return sin->sin_family;
}

/*
 * XXX the duplication here is horrid..
 * do we really need two methods doing essentially the same thing?
 * should cache, too!
 */
const char* Connection::GetIPString()
{
	static char buf[1024];

	if (this->privip == NULL)
		return "";

	switch (this->GetProtocolFamily())
	{
#ifdef SUPPORT_IP6LINKS
		case AF_INET6:
		{
			static char temp[1024];

			sockaddr_in6* sin = (sockaddr_in6*)this->privip;
			inet_ntop(sin->sin6_family, &sin->sin6_addr, buf, sizeof(buf));
			/* IP addresses starting with a : on irc are a Bad Thing (tm) */
			if (*buf == ':')
			{
				strlcpy(&temp[1], buf, sizeof(temp) - 1);
				*temp = '0';
				return temp;
			}
			return buf;
		}
		break;
#endif
		case AF_INET:
		{
			sockaddr_in* sin = (sockaddr_in*)this->privip;
			inet_ntop(sin->sin_family, &sin->sin_addr, buf, sizeof(buf));
			return buf;
		}
		break;
		default:
		break;
	}
	return "";
}

void Connection::HandleEvent(EventType et, int errornum)
{
	/* WARNING: May delete this connection! */
	LastSocketEvent = ServerInstance->Time();

	switch (et)
	{
		case EVENT_READ:
			if (!this->quitting)
				this->ReadData();
		break;
		case EVENT_WRITE:
			if ((State == HTTP_SEND_DATA) && (RespondType == HTTP_RESPOND_BACKEND))
				this->SendStaticData();
			else
				this->FlushWriteBuf();
		break;
		case EVENT_ERROR:
			ServerInstance->Connections->Delete(this);
		break;
	}
}

