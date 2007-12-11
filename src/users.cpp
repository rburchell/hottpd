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
	*host = 0;
	age = ServerInstance->Time(true);
	lastping = signon = idle_lastmsg = nping = 0;
	timeout = bytes_in = bytes_out = cmds_in = cmds_out = 0;
	muted = exempt = false;
	fd = -1;
	recvq.clear();
	sendq.clear();
	WriteError.clear();
	ip = NULL;
}

void User::RemoveCloneCounts()
{
	clonemap::iterator x = ServerInstance->local_clones.find(this->GetIPString());
	if (x != ServerInstance->local_clones.end())
	{
		x->second--;
		if (!x->second)
		{
			ServerInstance->local_clones.erase(x);
		}
	}
	
	clonemap::iterator y = ServerInstance->global_clones.find(this->GetIPString());
	if (y != ServerInstance->global_clones.end())
	{
		y->second--;
		if (!y->second)
		{
			ServerInstance->global_clones.erase(y);
		}
	}
}

User::~User()
{
	if (ip)
	{
		this->RemoveCloneCounts();

		if (this->GetProtocolFamily() == AF_INET)
		{
			delete (sockaddr_in*)ip;
		}
#ifdef SUPPORT_IP6LINKS
		else
		{
			delete (sockaddr_in6*)ip;
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
bool User::AddBuffer(std::string a)
{
	try
	{
		std::string::size_type i = a.rfind('\r');

		while (i != std::string::npos)
		{
			a.erase(i, 1);
			i = a.rfind('\r');
		}

		if (a.length())
			recvq.append(a);

		return true;
	}

	catch (...)
	{
		ServerInstance->Log(DEBUG,"Exception in User::AddBuffer()");
		return false;
	}
}

bool User::BufferIsReady()
{
	return (recvq.find('\n') != std::string::npos);
}

void User::ClearBuffer()
{
	recvq.clear();
}

std::string User::GetBuffer()
{
	try
	{
		if (recvq.empty())
			return "";

		/* Strip any leading \r or \n off the string.
		 * Usually there are only one or two of these,
		 * so its is computationally cheap to do.
		 */
		std::string::iterator t = recvq.begin();
		while (t != recvq.end() && (*t == '\r' || *t == '\n'))
		{
			recvq.erase(t);
			t = recvq.begin();
		}

		for (std::string::iterator x = recvq.begin(); x != recvq.end(); x++)
		{
			/* Find the first complete line, return it as the
			 * result, and leave the recvq as whats left
			 */
			if (*x == '\n')
			{
				std::string ret = std::string(recvq.begin(), x);
				recvq.erase(recvq.begin(), x + 1);
				return ret;
			}
		}
		return "";
	}

	catch (...)
	{
		ServerInstance->Log(DEBUG,"Exception in User::GetBuffer()");
		return "";
	}
}

void User::AddWriteBuf(const std::string &data)
{
	if (*this->GetWriteError())
		return;

	try
	{
		if (data.length() > MAXBUF - 2) /* MAXBUF has a value of 514, to account for line terminators */
			sendq.append(data.substr(0,MAXBUF - 4)).append("\r\n"); /* MAXBUF-4 = 510 */
		else
			sendq.append(data);
	}
	catch (...)
	{
		this->SetWriteError("SendQ exceeded");
	}
}

// send AS MUCH OF THE USERS SENDQ as we are able to (might not be all of it)
void User::FlushWriteBuf()
{
	try
	{
		if ((this->fd == FD_MAGIC_NUMBER) || (*this->GetWriteError()))
		{
			sendq.clear();
		}
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
					this->SetWriteError(errno ? strerror(errno) : "EOF from client");
					return;
				}
			}
			else
			{
				/* advance the queue */
				if (n_sent)
					this->sendq = this->sendq.substr(n_sent);
				/* update the user's stats counters */
				this->bytes_out += n_sent;
				this->cmds_out++;
				if (n_sent != old_sendq_length)
					this->ServerInstance->SE->WantWrite(this);
			}
		}
	}

	catch (...)
	{
		ServerInstance->Log(DEBUG,"Exception in User::FlushWriteBuf()");
	}

	if (this->sendq.empty())
	{
		FOREACH_MOD(I_OnBufferFlushed,OnBufferFlushed(this));
	}
}

void User::SetWriteError(const std::string &error)
{
	try
	{
		// don't try to set the error twice, its already set take the first string.
		if (this->WriteError.empty())
			this->WriteError = error;
	}

	catch (...)
	{
		ServerInstance->Log(DEBUG,"Exception in User::SetWriteError()");
	}
}

const char* User::GetWriteError()
{
	return this->WriteError.c_str();
}

void User::QuitUser(InspIRCd* Instance, User *user, const std::string &quitreason, const char* operreason)
{
	user->Write("Link closed. :)");
	user->muted = true;
	Instance->GlobalCulls.AddItem(user, quitreason.c_str(), operreason);
}

/* add a client connection to the sockets list */
void User::AddClient(InspIRCd* Instance, int socket, int port, bool iscached, int socketfamily, sockaddr* ip)
{
	User* New = NULL;
	New = new User(Instance);

	Instance->Log(DEBUG,"New user fd: %d", socket);

	int j = 0;

	char ipaddr[MAXBUF];
#ifdef IPV6
	if (socketfamily == AF_INET6)
		inet_ntop(AF_INET6, &((const sockaddr_in6*)ip)->sin6_addr, ipaddr, sizeof(ipaddr));
	else
#endif
	inet_ntop(AF_INET, &((const sockaddr_in*)ip)->sin_addr, ipaddr, sizeof(ipaddr));

	New->SetFd(socket);
	New->signon = Instance->Time() + Instance->Config->dns_timeout;
	New->lastping = 1;

	New->SetSockAddr(socketfamily, ipaddr, port);

	/* Smarter than your average bear^H^H^H^Hset of strlcpys. */
	for (const char* temp = New->GetIPString(); *temp && j < 64; temp++, j++)
		New->dhost[j] = New->host[j] = *temp;
	New->dhost[j] = New->host[j] = 0;

	Instance->AddLocalClone(New);
	Instance->AddGlobalClone(New);

	Instance->local_users.push_back(New);

	if ((Instance->local_users.size() > Instance->Config->SoftLimit) || (Instance->local_users.size() >= MAXCLIENTS))
	{
		User::QuitUser(Instance, New,"No more connections allowed");
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
		User::QuitUser(Instance, New, "Server is full");
		return;
	}
#endif

        if (socket > -1)
        {
                if (!Instance->SE->AddFd(New))
                {
			Instance->Log(DEBUG,"Internal error on new connection");
			User::QuitUser(Instance, New, "Internal error handling connection");
			return;
                }
        }

	New->FullConnect();
}

unsigned long User::LocalCloneCount()
{
	clonemap::iterator x = ServerInstance->local_clones.find(this->GetIPString());
	if (x != ServerInstance->local_clones.end())
		return x->second;
	else
		return 0;
}

void User::FullConnect()
{
	ServerInstance->stats->statsConnects++;
	this->idle_lastmsg = ServerInstance->Time();

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
			this->ip = (sockaddr*)sin;
		}
		break;
#endif
		case AF_INET:
		{
			sockaddr_in* sin = new sockaddr_in;
			sin->sin_family = AF_INET;
			sin->sin_port = port;
			inet_pton(AF_INET, ip, &sin->sin_addr);
			this->ip = (sockaddr*)sin;
		}
		break;
		default:
			ServerInstance->Log(DEBUG,"Uh oh, I dont know protocol %d to be set!", protocol_family);
		break;
	}
}

int User::GetPort()
{
	if (this->ip == NULL)
		return 0;

	switch (this->GetProtocolFamily())
	{
#ifdef SUPPORT_IP6LINKS
		case AF_INET6:
		{
			sockaddr_in6* sin = (sockaddr_in6*)this->ip;
			return sin->sin6_port;
		}
		break;
#endif
		case AF_INET:
		{
			sockaddr_in* sin = (sockaddr_in*)this->ip;
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
	if (this->ip == NULL)
		return 0;

	sockaddr_in* sin = (sockaddr_in*)this->ip;
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

	if (this->ip == NULL)
		return "";

	switch (this->GetProtocolFamily())
	{
#ifdef SUPPORT_IP6LINKS
		case AF_INET6:
		{
			static char temp[1024];

			sockaddr_in6* sin = (sockaddr_in6*)this->ip;
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
			sockaddr_in* sin = (sockaddr_in*)this->ip;
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
void User::Write(std::string text)
{
	if (!ServerInstance->SE->BoundsCheckFd(this))
		return;

	try
	{
		/* ServerInstance->Log(DEBUG,"C[%d] O %s", this->GetFd(), text.c_str());
		 * WARNING: The above debug line is VERY loud, do NOT
		 * enable it till we have a good way of filtering it
		 * out of the logs (e.g. 1.2 would be good).
		 */
		text.append("\r\n");
	}
	catch (...)
	{
		ServerInstance->Log(DEBUG,"Exception in User::Write() std::string::append");
		return;
	}

	this->AddWriteBuf(text);
	ServerInstance->stats->statsSent += text.length();
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
				ServerInstance->ProcessUser(this);
			break;
			case EVENT_WRITE:
				this->FlushWriteBuf();
			break;
			case EVENT_ERROR:
				/** This should be safe, but dont DARE do anything after it -- Brain */
				this->SetWriteError(errornum ? strerror(errornum) : "EOF from client");
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
			User::QuitUser(ServerInstance, this, GetWriteError());
		}
	}
}

