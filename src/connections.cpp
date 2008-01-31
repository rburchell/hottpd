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
	// XXX - Make keepalive by default an option?
	keepalive = true;
	rfilesize = rfilesent = RequestsCompleted = 0;
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

void Connection::ReadData()
{
	int result = EAGAIN;

	if (this->GetFd() == FD_MAGIC_NUMBER)
		return;

	char* ReadBuffer = ServerInstance->GetReadBuffer();

#ifndef WIN32
	result = read(this->fd, ReadBuffer, sizeof(ReadBuffer));
#else
	result = recv(this->fd, (char*)ReadBuffer, sizeof(ReadBuffer), 0);
#endif

	if ((result) && (result != -EAGAIN))
	{
		int currfd;

		if (result > 0)
			ReadBuffer[result] = '\0';

		currfd = this->GetFd();

		// add the data to the connection's buffer (which will process it if necessary)
		if (result > 0)
		{
			if (!this->AddBuffer(ReadBuffer))
			{
				// fuck, something exploded
				ServerInstance->Connections->Delete(this);
				return;
			}

			return;
		}

		if ((result == -1) && (errno != EAGAIN) && (errno != EINTR))
		{
			ServerInstance->Connections->Delete(this);
			return;
		}
	}

	// result EAGAIN means nothing read
	else if ((result == EAGAIN) || (result == -EAGAIN))
	{
		/* do nothing */
	}
	else if (result == 0)
	{
		ServerInstance->Connections->Delete(this);
		return;
	}
}

/** NOTE: We cannot pass a const reference to this method.
 * The string is changed by the workings of the method,
 * so that if we pass const ref, we end up copying it to
 * something we can change anyway. Makes sense to just let
 * the compiler do that copy for us.
 */
bool Connection::AddBuffer(const std::string &a)
{
	if (!a.length())
	{
		/* how is this possible .. */
		return true;
	}
	
	if (State == HTTP_RECV_REQBODY)
	{
		// We don't do request bodies yet :P
		return false;
	}
	else
	{
		/* We can get data for a future request at any time, and that
		 * is what we're doing here. We only trigger the check for a
		 * new request if we're waiting for one. */
		
		int nspos = requestbuf.length();
		
		requestbuf.append(a);

		if (requestbuf.length() > 5120)
		{
			// XXX arbitrary limit; needs discussion of a proper default
			ServerInstance->Log(DEBUG, "Too much data in buffer; dropping");
			return false;
		}
		
		if (State == HTTP_WAIT_REQUEST)
			this->CheckRequest(nspos);		
	}

	return true;
}

void Connection::CheckRequest(int newpos)
{
	/* A HTTP request ends with a blank header, i.e. \r\n\r\n. We're trying first
	 * to search for this sequence, but we only search in new data (and the last
	 * three bytes of old data) to save time.
	 *
	 * rfind can't be used, as it would break pipelining.
	 */
	
	std::string::size_type reqend = requestbuf.find("\r\n\r\n", (newpos >= 3) ? (newpos - 3) : 0);
	if (reqend == std::string::npos)
		return;

	ServerInstance->Log(DEBUG, "Got headers.");
	
	/* BIG HUGE WARNING THAT YOU SHOULD READ BEFORE YOU MAKE AN INFINITE LOOP!
	 *
	 * Erase from the requestbuf before you call anything like SendError, etc.
	 * Not doing so has very unfortunate consequences, k?
	 */
	
	std::string::size_type hbegin = 0, hend;
	for (; (hend = requestbuf.find("\r\n", hbegin)) != std::string::npos; hbegin = hend + 2)
	{
		if (hbegin == hend)
			break;
		
		if (method.empty())
		{
			std::istringstream cheader(std::string(requestbuf, hbegin, hend - hbegin));
			std::string tmpversion;
			cheader >> method;
			cheader >> uri;
			cheader >> tmpversion;

			if (method.empty() || uri.empty() || tmpversion.empty())
			{
				requestbuf.erase(0, reqend + 4);
				SendError(400, "Bad Request");
				return;
			}

			std::transform(tmpversion.begin(), tmpversion.end(), tmpversion.begin(), ::toupper);
			
			if (tmpversion == "HTTP/1.1")
			{
				http_version = HTTP_1_1;
			}
			else if (tmpversion == "HTTP/1.0")
			{
				http_version = HTTP_1_0;

				// HTTP 1.0 defaults to Connection: Close (but only set this on the first request of the connection)
				if (!RequestsCompleted)
					keepalive = false;
			}
			else
			{
				requestbuf.erase(0, reqend + 4);
				SendError(505, "Version Not Supported");
				return;
			}
		}
		else
		{
			std::string cheader = requestbuf.substr(hbegin, hend - hbegin);
	
			std::string::size_type fieldsep = cheader.find(':');
			if ((fieldsep == std::string::npos) || (fieldsep == 0) || (fieldsep == cheader.length() - 1))
			{
				requestbuf.erase(0, reqend + 4);
				SendError(400, "Bad Request");
				return;
			}
	
			headers.SetHeader(cheader.substr(0, fieldsep), cheader.substr(fieldsep + 2));
		}
	}

	requestbuf.erase(0, reqend + 4);

	// In the interest of convention, make the method uppercase
	std::transform(method.begin(), method.end(), method.begin(), ::toupper);
	
	// Important header checks for internal state and RFC compatibility
	if (strcasecmp(headers.GetHeader("Connection").c_str(), "close") == 0)
		keepalive = false;
	else if (strcasecmp(headers.GetHeader("Connection").c_str(), "keep-alive") == 0)
		keepalive = true;
	
	if ((RequestsCompleted + 1) >= ServerInstance->Config->KeepAliveMax)
	{
		ServerInstance->Log(DEBUG, "Closing connection after request due to keepalive limit");
		keepalive = false;
	}
	
	ServeData();
 }

void Connection::HandleURI()
{
	// TODO absolute URLs (i.e. http://[HOST HEADER VALUE]/blah)
	if (uri[0] != '/')
		return;
	
	std::string sanepath;
	std::string pathpart;
	for (std::string::const_iterator i = uri.begin() + 1; ; ++i)
	{
		char c = (i == uri.end()) ? 0 : *i;
		
		if (c == '%')
		{
			// URL Encoded value
			if (((i + 1) == uri.end()) || ((i + 2) == uri.end()))
			{
				// Without an actual value. Idiots.
				i = uri.end() - 1;
				continue;
			}
			
			unsigned char v = utils::unhexchar(*(i + 1), *(i + 2));
			if ((v < 32) || (v == 127))
			{
				ServerInstance->Log(DEBUG, "URLEncoded unprintable character %d removed from URI.", v);
				i += 2;
				continue;
			}
			
			if (v != '/')
			{
				// Slashes are *NOT* allowed. They will be parsed like a normal /. Other characters are fine.
				pathpart.push_back(v);
				i += 2;
				continue;
			}
			
			// Parse this as if it were the normal character
			i += 2;
			c = v;
		}
		
		if (!c || (c == '/'))
		{
			// End of the URL *or* a path seperator
			
			if (!pathpart.length() || (pathpart == "."))
			{
				// Do nothing; these are meaningless and will be cleaned from the path
			}
			else if (pathpart == "..")
			{
				std::string::size_type sloc = sanepath.rfind('/', sanepath.length() - 2);
				if (sloc != std::string::npos)
					sanepath.erase(sloc + 1);
				else
					sanepath.clear();
			}
			else
			{
				sanepath.append(pathpart);
				if (c)
					sanepath.push_back('/');
			}
			
			pathpart.clear();
			
			if (!c)
				break;
		}
		else if (c == '?')
		{
			// Begin query
			uriquery.assign<std::string::const_iterator>(i + 1, uri.end());
			i = uri.end() - 1;
		}
		else if (c == '#')
		{
			// The client should not be sending this. Idiot.
			i = uri.end() - 1;
		}
		else
		{
			pathpart.push_back(c);
		}
	}
	
	upath = ServerInstance->Config->DocRoot;
	if ((upath[upath.length() - 1] != '/') && (sanepath[0] != '/'))
		upath.push_back('/');
	upath.append(sanepath);
	
	ServerInstance->Log(DEBUG, "Mapped URI '%s' to path %s", uri.c_str(), upath.c_str());
}

void Connection::ServeData()
{
	ServerInstance->Log(DEBUG, "ServeData: %s: %s", method.c_str(), uri.c_str());

	if (method == "GET")
	{
		HandleURI();
		
		// Check if the file exists
		struct stat *fst;
		if (ServerInstance->FileSys->Stat(upath.c_str(), fst) < 0)
		{
			if (errno == EACCES)
				this->SendError(403, "Forbidden");
			else if ((errno == ENOENT) || (errno == ELOOP) || (errno == ENAMETOOLONG) || (errno == ENOTDIR))
				this->SendError(404, "File Not Found");
			else
				this->SendError(500, "Internal Server Error");
			return;
		}
		
		// Stat will follow symlinks, so this refers to the actual destination file. Follow symlink checks are elsewhere
		if (!S_ISREG(fst->st_mode))
		{
			// This isn't a normal file
			this->SendError(403, "Forbidden");
			return;
		}
		
		int oflags = O_RDONLY;
		// While cool, O_NOATIME causes an EPERM when trying to open a file not owned by this user (read isn't enough).
/*#ifdef O_NOATIME
		if (ServerInstance->Config->NoAtime)
			oflags |= O_NOATIME;
#endif*/
		
		if (filefd > -1)
			close(filefd);
		
		filefd = open(upath.c_str(), oflags);
		if (filefd < 0)
		{
			switch (errno)
			{
				case EACCES:
					this->SendError(403, "Forbidden");
					break;
				case ENOENT:
				case ENOTDIR:
					this->SendError(404, "File Not Found");
					break;
				default:
					ServerInstance->Log(DEBUG, "open() to serve file '%s' failed with error: %s", upath.c_str(), strerror(errno));
					this->SendError(500, "Internal Server Error");
					break;
			}
			
			return;
		}
		
		// Don't set request state here; SendHeaders() will do that.
		
		rfilesize = fst->st_size;
		rfilesent = 0;
		RespondType = HTTP_RESPOND_BACKEND;
		ResponseBackend = WriteBackend::GetInstance(ServerInstance);

		HTTPHeaders empty;
		this->SendHeaders(fst->st_size, 200, "OK", empty);
		
		// When the headers have finished being sent, sending of data will be automatically triggered.
	}
	else
	{
		this->SendError(501, "Not Implemented");
	}
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

void Connection::SendHeaders(unsigned long size, int response, const std::string &rtext, HTTPHeaders &rheaders)
{
	State = HTTP_SEND_HEADERS;
	
	if (http_version == HTTP_1_0)
		this->Write("HTTP/1.0 ");
	else
		this->Write("HTTP/1.1 ");
	
	this->Write(ConvToStr(response)+" "+rtext+"\r\n");

	time_t local = this->ServerInstance->Time();
	struct tm *timeinfo = gmtime(&local);
	char *date = asctime(timeinfo);
	date[strlen(date) - 1] = '\0';
	rheaders.CreateHeader("Date", date);
	
	rheaders.CreateHeader("Server", "hottpd");
	rheaders.SetHeader("Content-Length", ConvToStr(size));
	
	if (size && !rheaders.IsSet("Content-Type"))
	{
		size_t p = uri.find_last_of("."); // XXX hack of sorts
		std::string mime;

		if (p != std::string::npos)
			mime = ServerInstance->MimeTypes->GetType(uri.substr(p + 1));

		if (mime.empty())
			mime = "application/x-octet-stream";

		ServerInstance->Log(DEBUG, "Sending mimetype %s for %s", mime.c_str(), uri.c_str());

		rheaders.SetHeader("Content-Type", mime);
	}
	else if (!size)
		rheaders.RemoveHeader("Content-Type");
	
	if (strcasecmp(rheaders.GetHeader("Connection").c_str(), "Close") == 0)
		keepalive = false;
	else if (keepalive)
		rheaders.SetHeader("Connection", "Keep-Alive");
	else
		rheaders.SetHeader("Connection", "Close");
	
	this->Write(rheaders.GetFormattedHeaders());
	this->Write("\r\n");
		
	if (!size)
	{
		// No request body, so we're done now
		EndRequest();
	}
}

void Connection::EndRequest()
{
	ServerInstance->Log(DEBUG, "End request");
	
	RequestsCompleted++;
	
	if (!keepalive)
	{
		State = HTTP_FINISHED;
		ServerInstance->Connections->Delete(this);
		return;
	}
	
	headers.Clear();
	method.clear();
	uri.clear();
	uriquery.clear();
	upath.clear();
	http_version = HTTP_UNSPECIFIED;
	State = HTTP_WAIT_REQUEST;
	ResponseBackend = NULL;
	RespondType = HTTP_RESPOND_FLUSH;
	rfilesize = rfilesent = 0;
	
	if (filefd > -1)
	{
		close(filefd);
		filefd = -1;
	}
	
	if (requestbuf.length())
	{
		/* XXX should this be delayed to the next iteration to avoid letting a
		 * connection hog the process? If so, it would need to trigger regardless
		 * of a read event... */
		
		this->CheckRequest(0);
	}
}

void Connection::SendStaticData()
{
	ServerInstance->Log(DEBUG, "Sending response with backend");
	
	if (filefd < 0)
	{
		ServerInstance->Log(DEBUG, "Backend triggered but connection has no open file - closing connection");
		ServerInstance->Connections->Delete(this);
	}
	
	int re = ResponseBackend->ServeFile(GetFd(), filefd, rfilesent, rfilesize);
	if (re < 0)
	{
		ServerInstance->Log(DEBUG, "Response backend returned error; closing connection");
		ServerInstance->Connections->Delete(this);
	}	
	else if (rfilesent == rfilesize)
	{
		ServerInstance->Log(DEBUG, "Response backend finished serving request");
		EndRequest();
	}
	else
	{
		// More data to send; poll for write
		ServerInstance->SE->WantWrite(this);
	}
}

















void Connection::AddWriteBuf(const std::string &data)
{
	sendq.append(data);
}

// send AS MUCH OF THE ConnectionS SENDQ as we are able to (might not be all of it)
void Connection::FlushWriteBuf()
{
	if ((sendq.length()) && (this->fd != FD_MAGIC_NUMBER))
	{
		int old_sendq_length = sendq.length();
		int n_sent = ServerInstance->SE->Send(this, this->sendq.data(), this->sendq.length(), 0);

		if (n_sent == -1)
		{
			if (errno == EAGAIN)
			{
				/* The socket buffer is full. This isnt fatal,
				 * try again later.
				 */
				this->ServerInstance->SE->WantWrite(this);
			}
			else
			{
				/* Fatal error, set write error and bail
				 */
				ServerInstance->Connections->Delete(this); // XXX shouldn't try flush buffer, add a bool?
				return;
			}
		}
		else
		{
			/* advance the queue */
			if (n_sent)
				this->sendq = this->sendq.substr(n_sent);
			if (n_sent != old_sendq_length)
				this->ServerInstance->SE->WantWrite(this);
		}
	}

	if (this->sendq.empty())
	{
		FOREACH_MOD(I_OnBufferFlushed,OnBufferFlushed(this));
		
		if ((RespondType == HTTP_RESPOND_FLUSH) && (State == HTTP_SEND_DATA))
		{
			EndRequest();
		}
		else if ((State == HTTP_SEND_HEADERS) && (RespondType == HTTP_RESPOND_BACKEND))
		{
			State = HTTP_SEND_DATA;
			SendStaticData();
		}
	}
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

/** NOTE: We cannot pass a const reference to this method.
 * The string is changed by the workings of the method,
 * so that if we pass const ref, we end up copying it to
 * something we can change anyway. Makes sense to just let
 * the compiler do that copy for us.
 */
void Connection::Write(const std::string &text)
{
	if (!ServerInstance->SE->BoundsCheckFd(this))
		return;

	this->AddWriteBuf(text);
	this->ServerInstance->SE->WantWrite(this);
}

/** Write()
 */
void Connection::Write(const char *text, ...)
{
	va_list argsPtr;
	char textbuffer[MAXBUF];

	va_start(argsPtr, text);
	vsnprintf(textbuffer, MAXBUF, text, argsPtr);
	va_end(argsPtr);

	this->Write(std::string(textbuffer));
}

void Connection::HandleEvent(EventType et, int errornum)
{
	/* WARNING: May delete this connection! */
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

