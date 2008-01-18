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

/* $Core: libIRCDusers */

#include "inspircd.h"
#include <stdarg.h>
#include "socketengine.h"
#include "wildcard.h"

User::User(InspIRCd* Instance) : ServerInstance(Instance)
{
	quitting = false;
	fd = -1;
	privip = NULL;
	State = HTTP_WAIT_REQUEST;
}

User::~User()
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
}

void User::CloseSocket()
{
	ServerInstance->SE->Shutdown(this, 2);
	ServerInstance->SE->Close(this);
}

int User::ReadData(void* buffer, size_t size)
{
	if (IS_LOCAL(this))
	{
#ifndef WIN32
		return read(this->fd, buffer, size);
#else
		return recv(this->fd, (char*)buffer, size, 0);
#endif
	}
	else
		return 0;
}

/** NOTE: We cannot pass a const reference to this method.
 * The string is changed by the workings of the method,
 * so that if we pass const ref, we end up copying it to
 * something we can change anyway. Makes sense to just let
 * the compiler do that copy for us.
 */
bool User::AddBuffer(const std::string &a)
{
	if (!a.length())
	{
		/* how is this possible .. */
		return true;
	}

	ServerInstance->Log(DEBUG, "Adding to buffer: " + a);

	switch (State)
	{
		case HTTP_WAIT_REQUEST:
			requestbuf.append(a);

			if (requestbuf.length() > 2000)
			{
				// XXX this is arbitrary, but it should stop people flooding
				ServerInstance->Log(DEBUG, "Too much data, setting to SEND");
				State = HTTP_SEND_DATA;
			}

			this->CheckRequest();
			break;
		case HTTP_RECV_POSTDATA:
			return false; // XXX fixme we can't handle POST yet :P
			break;
		case HTTP_SEND_DATA:
		case HTTP_FINISHED:
			/* Drop it into the bit bucket. Don't care or shouldn't happen. */
			break;
	}

	return true;
}

void User::CheckRequest()
{
	/* Copied liberally from m_httpd for now */

	ServerInstance->Log(DEBUG, "Checking request.");

	std::string::size_type reqend = requestbuf.find("\r\n\r\n");
	if (reqend == std::string::npos)
		return;

	ServerInstance->Log(DEBUG, "Got headers.");
 
	// We have the headers; parse them all
	std::string::size_type hbegin = 0, hend;
	while ((hend = requestbuf.find("\r\n", hbegin)) != std::string::npos)
	{
		if (hbegin == hend)
			break;

		if (request_type.empty())
		{
			std::istringstream cheader(std::string(requestbuf, hbegin, hend - hbegin));
			cheader >> request_type;
			cheader >> uri;
			cheader >> http_version;

			if (request_type.empty() || uri.empty() || http_version.empty())
			{
				SendError(400);
				return;
			}

			hbegin = hend + 2;
			continue;
		}

		std::string cheader = requestbuf.substr(hbegin, hend - hbegin);

		std::string::size_type fieldsep = cheader.find(':');
		if ((fieldsep == std::string::npos) || (fieldsep == 0) || (fieldsep == cheader.length() - 1))
		{
			SendError(400);
			return;
		}

		headers.SetHeader(cheader.substr(0, fieldsep), cheader.substr(fieldsep + 2));

		hbegin = hend + 2;
	}

	requestbuf.erase(0, reqend + 4);

	std::transform(request_type.begin(), request_type.end(), request_type.begin(), ::toupper);
	std::transform(http_version.begin(), http_version.end(), http_version.begin(), ::toupper);

	if (http_version != "HTTP/1.1" &&
	    http_version != "HTTP/1.0")
	{
		SendError(505);
		return;
	}

	if (strcasecmp(headers.GetHeader("Connection").c_str(), "keep-alive") == 0)
		keepalive = true;

/*	if (headers.IsSet("Content-Length") && (postsize = atoi(headers.GetHeader("Content-Length").c_str())) != 0)
	{
		State = HTTP_RECV_POSTDATA;

		if (requestbuf.length() >= postsize)
		{
			postdata = requestbuf.substr(0, postsize);
			requestbuf.erase(0, postsize);
		}
		else if (!requestbuf.empty())
		{
			postdata = requestbuf;
			requestbuf.clear();
		}

		if (postdata.length() >= postsize)
			ServeData();

		return;
	}*/

	ServeData();
 }

void User::ServeData()
{
	ServerInstance->Log(DEBUG, "ServeData: %s %s: %s", request_type.c_str(), http_version.c_str(), uri.c_str());
	State = HTTP_SEND_DATA;

	if (request_type == "GET")
	{
		std::string response = "moocows rule my world";
		HTTPHeaders empty;
		this->SendHeaders(response.length(), 200, empty);
		this->Write(response);
	}
	else
	{
		this->SendError(404);
	}
}

void User::SendError(int code)
{
	HTTPHeaders empty;
	std::string data = "<html><head></head><body>Server error "+ConvToStr(code)+":<br>"+
	                   "<small>Powered by <a href='http://www.inspircd.org'>InspIRCd</a></small></body></html>";
		
	this->SendHeaders(data.length(), code, empty);
	this->Write(data);
}

void User::SendHeaders(unsigned long size, int response, HTTPHeaders &rheaders)
{
	this->Write(http_version + " "+ConvToStr(response)+"\r\n");

	time_t local = this->ServerInstance->Time();
	struct tm *timeinfo = gmtime(&local);
	char *date = asctime(timeinfo);
	date[strlen(date) - 1] = '\0';
	rheaders.CreateHeader("Date", date);
		
	rheaders.CreateHeader("Server", "InspIRCd/m_httpd.so/1.1");
	rheaders.SetHeader("Content-Length", ConvToStr(size));
	
	if (size)
		rheaders.CreateHeader("Content-Type", "text/html");
	else
		rheaders.RemoveHeader("Content-Type");
		
	if (rheaders.GetHeader("Connection") == "Close")
		keepalive = false;
	else if (rheaders.GetHeader("Connection") == "Keep-Alive" && !headers.IsSet("Connection"))
		keepalive = true;
	else if (!rheaders.IsSet("Connection") && !keepalive)
		rheaders.SetHeader("Connection", "Close");
	
	this->Write(rheaders.GetFormattedHeaders());
	this->Write("\r\n");
		
	if (!size && keepalive)
		ResetRequest();
}

void User::ResetRequest()
{
	headers.Clear();
	request_type.clear();
	uri.clear();
	http_version.clear();
	State = HTTP_WAIT_REQUEST;
		
	if (requestbuf.size())
		requestbuf.clear();
}



















void User::AddWriteBuf(const std::string &data)
{
	sendq.append(data);
}

// send AS MUCH OF THE USERS SENDQ as we are able to (might not be all of it)
void User::FlushWriteBuf()
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
				User::QuitUser(this->ServerInstance, this); // XXX shouldn't try flush buffer, add a bool?
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
		if (State == HTTP_SEND_DATA) // XXX reset conn, http 1.1 keepalive!
			User::QuitUser(this->ServerInstance, this);
	}
}

void User::QuitUser(InspIRCd* Instance, User *user)
{
	Instance->GlobalCulls.AddItem(user);
	user->quitting = true;
	user->State = HTTP_FINISHED;
}

/* add a client connection to the sockets list */
void User::AddClient(InspIRCd* Instance, int socket, int port, bool iscached, int socketfamily, sockaddr* ip)
{
	User* New = NULL;
	New = new User(Instance);

	Instance->Log(DEBUG,"New user fd: %d", socket);

	char ipaddr[MAXBUF];
#ifdef IPV6
	if (socketfamily == AF_INET6)
		inet_ntop(AF_INET6, &((const sockaddr_in6*)ip)->sin6_addr, ipaddr, sizeof(ipaddr));
	else
#endif
	inet_ntop(AF_INET, &((const sockaddr_in*)ip)->sin_addr, ipaddr, sizeof(ipaddr));

	New->SetFd(socket);
	New->SetSockAddr(socketfamily, ipaddr, port);
	New->ip = New->GetIPString();

	Instance->local_users.push_back(New);

	if ((Instance->local_users.size() > Instance->Config->SoftLimit) || (Instance->local_users.size() >= MAXCLIENTS))
	{
		User::QuitUser(Instance, New);
		return;
	}

	/*
	 * XXX -
	 * this is done as a safety check to keep the file descriptors within range of fd_ref_table.
	 * its a pretty big but for the moment valid assumption:
	 * file descriptors are handed out starting at 0, and are recycled as theyre freed.
	 * therefore if there is ever an fd over 65535, 65536 clients must be connected to the
	 * irc server at once (or the irc server otherwise initiating this many connections, files etc)
	 * which for the time being is a physical impossibility (even the largest networks dont have more
	 * than about 10,000 users on ONE server!)
	 */
#ifndef WINDOWS
	if ((unsigned int)socket >= MAX_DESCRIPTORS)
	{
		User::QuitUser(Instance, New);
		return;
	}
#endif

	if (socket > -1)
	{
		if (!Instance->SE->AddFd(New))
		{
			Instance->Log(DEBUG,"Internal error on new connection");
			User::QuitUser(Instance, New);
			return;
		}
	}

	New->FullConnect();
}

void User::FullConnect()
{
	FOREACH_MOD(I_OnUserConnect,OnUserConnect(this));
	FOREACH_MOD(I_OnPostConnect,OnPostConnect(this));
}

void User::SetSockAddr(int protocol_family, const char* ip, int port)
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

int User::GetPort()
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

int User::GetProtocolFamily()
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
const char* User::GetIPString()
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
void User::Write(const std::string &text)
{
	if (!ServerInstance->SE->BoundsCheckFd(this))
		return;

	this->AddWriteBuf(text);
	this->ServerInstance->SE->WantWrite(this);
}

/** Write()
 */
void User::Write(const char *text, ...)
{
	va_list argsPtr;
	char textbuffer[MAXBUF];

	va_start(argsPtr, text);
	vsnprintf(textbuffer, MAXBUF, text, argsPtr);
	va_end(argsPtr);

	this->Write(std::string(textbuffer));
}

void User::HandleEvent(EventType et, int errornum)
{
	/* WARNING: May delete this user! */
	int thisfd = this->GetFd();

	try
	{
		switch (et)
		{
			case EVENT_READ:
				if (!this->quitting)
					ServerInstance->ProcessUser(this);
			break;
			case EVENT_WRITE:
				this->FlushWriteBuf();
			break;
			case EVENT_ERROR:
				User::QuitUser(this->ServerInstance, this);
			break;
		}
	}
	catch (...)
	{
		ServerInstance->Log(DEBUG,"Exception in User::HandleEvent intercepted");
	}

	/* If the user has raised an error whilst being processed, quit them now we're safe to */
	if ((ServerInstance->SE->GetRef(thisfd) == this))
	{
		if (!WriteError.empty())
		{
			User::QuitUser(ServerInstance, this);
		}
	}
}

