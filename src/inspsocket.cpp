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

/* $Core: libIRCDinspsocket */

#include "socket.h"
#include "inspstring.h"
#include "socketengine.h"
#include "inspircd.h"

using irc::sockets::OpenTCPSocket;

bool BufferedSocket::Readable()
{
	return ((this->state != I_CONNECTING) && (this->WaitingForWriteEvent == false));
}

BufferedSocket::BufferedSocket(InspIRCd* SI)
{
	this->Timeout = NULL;
	this->state = I_DISCONNECTED;
	this->fd = -1;
	this->WaitingForWriteEvent = false;
	this->Instance = SI;
	this->IsIOHooked = false;
}

BufferedSocket::BufferedSocket(InspIRCd* SI, int newfd, const char* ip)
{
	this->Timeout = NULL;
	this->fd = newfd;
	this->state = I_CONNECTED;
	strlcpy(this->IP,ip,MAXBUF);
	this->WaitingForWriteEvent = false;
	this->Instance = SI;
	this->IsIOHooked = false;
	if (this->fd > -1)
		this->Instance->SE->AddFd(this);
}

BufferedSocket::BufferedSocket(InspIRCd* SI, const std::string &ipaddr, int aport, bool listening, unsigned long maxtime, const std::string &connectbindip)
{
	this->cbindip = connectbindip;
	this->fd = -1;
	this->Instance = SI;
	strlcpy(host,ipaddr.c_str(),MAXBUF);
	this->WaitingForWriteEvent = false;
	this->IsIOHooked = false;
	this->Timeout = NULL;
	if (listening)
	{
		if ((this->fd = OpenTCPSocket(host)) == ERROR)
		{
			this->fd = -1;
			this->state = I_ERROR;
			this->OnError(I_ERR_SOCKET);
			return;
		}
		else
		{
			if (!SI->BindSocket(this->fd,aport,(char*)ipaddr.c_str()))
			{
				this->Close();
				this->fd = -1;
				this->state = I_ERROR;
				this->OnError(I_ERR_BIND);
				this->ClosePending = true;
				return;
			}
			else
			{
				this->state = I_LISTENING;
				this->port = aport;
				if (this->fd > -1)
				{
					if (!this->Instance->SE->AddFd(this))
					{
						this->Close();
						this->state = I_ERROR;
						this->OnError(I_ERR_NOMOREFDS);
					}
				}
				return;
			}
		}
	}
	else
	{
		strlcpy(this->host,ipaddr.c_str(),MAXBUF);
		this->port = aport;

		bool ipvalid = true;
#ifdef IPV6
		if (strchr(host,':'))
		{
			in6_addr n;
			if (inet_pton(AF_INET6, host, &n) < 1)
				ipvalid = false;
		}
		else
#endif
		{
			in_addr n;
			if (inet_aton(host,&n) < 1)
				ipvalid = false;
		}
		if (!ipvalid)
		{
			this->Instance->Log(DEBUG,"BUG: Hostname passed to BufferedSocket, rather than an IP address!");
			this->OnError(I_ERR_CONNECT);
			this->Close();
			this->fd = -1;
			this->state = I_ERROR;
			return;
		}
		else
		{
			strlcpy(this->IP,host,MAXBUF);
			timeout_val = maxtime;
			if (!this->DoConnect())
			{
				this->OnError(I_ERR_CONNECT);
				this->Close();
				this->fd = -1;
				this->state = I_ERROR;
				return;
			}
		}
	}
}

void BufferedSocket::WantWrite()
{
	this->Instance->SE->WantWrite(this);
	this->WaitingForWriteEvent = true;
}

void BufferedSocket::SetQueues(int nfd)
{
	// attempt to increase socket sendq and recvq as high as its possible
	int sendbuf = 32768;
	int recvbuf = 32768;
	if(setsockopt(nfd,SOL_SOCKET,SO_SNDBUF,(const char *)&sendbuf,sizeof(sendbuf)) || setsockopt(nfd,SOL_SOCKET,SO_RCVBUF,(const char *)&recvbuf,sizeof(sendbuf)))
	{
		//this->Instance->Log(DEFAULT, "Could not increase SO_SNDBUF/SO_RCVBUF for socket %u", GetFd());
		; // do nothing. I'm a little sick of people trying to interpret this message as a result of why their incorrect setups don't work.
	}
}

/* Most irc servers require you to specify the ip you want to bind to.
 * If you dont specify an IP, they rather dumbly bind to the first IP
 * of the box (e.g. INADDR_ANY). In InspIRCd, we scan thought the IP
 * addresses we've bound server ports to, and we try and bind our outbound
 * connections to the first usable non-loopback and non-any IP we find.
 * This is easier to configure when you have a lot of links and a lot
 * of servers to configure.
 */
bool BufferedSocket::BindAddr(const std::string &ip)
{
	ConfigReader Conf(this->Instance);
	socklen_t size = sizeof(sockaddr_in);
#ifdef IPV6
	bool v6 = false;
	/* Are we looking for a binding to fit an ipv6 host? */
	if ((ip.empty()) || (ip.find(':') != std::string::npos))
		v6 = true;
#endif
	int j = 0;
	while (j < Conf.Enumerate("bind") || (!ip.empty()))
	{
		std::string IP = ip.empty() ? Conf.ReadValue("bind","address",j) : ip;
		if (!ip.empty() || Conf.ReadValue("bind","type",j) == "servers")
		{
			if (!ip.empty() || ((IP != "*") && (IP != "127.0.0.1") && (!IP.empty()) && (IP != "::1")))
			{
				/* The [2] is required because we may write a sockaddr_in6 here, and sockaddr_in6 is larger than sockaddr, where sockaddr_in4 is not. */
				sockaddr* s = new sockaddr[2];
#ifdef IPV6
				if (v6)
				{
					in6_addr n;
					if (inet_pton(AF_INET6, IP.c_str(), &n) > 0)
					{
						memcpy(&((sockaddr_in6*)s)->sin6_addr, &n, sizeof(sockaddr_in6));
						((sockaddr_in6*)s)->sin6_port = 0;
						((sockaddr_in6*)s)->sin6_family = AF_INET6;
						size = sizeof(sockaddr_in6);
					}
					else
					{
						delete[] s;
						j++;
						continue;
					}
				}
				else
#endif
				{
					in_addr n;
					if (inet_aton(IP.c_str(), &n) > 0)
					{
						((sockaddr_in*)s)->sin_addr = n;
						((sockaddr_in*)s)->sin_port = 0;
						((sockaddr_in*)s)->sin_family = AF_INET;
					}
					else
					{
						delete[] s;
						j++;
						continue;
					}
				}

				if (Instance->SE->Bind(this->fd, s, size) < 0)
				{
					this->state = I_ERROR;
					this->OnError(I_ERR_BIND);
					this->fd = -1;
					delete[] s;
					return false;
				}

				delete[] s;
				return true;
			}
		}
		j++;
	}
	Instance->Log(DEBUG,"nothing in the config to bind()!");
	return true;
}

bool BufferedSocket::DoConnect()
{
	/* The [2] is required because we may write a sockaddr_in6 here, and sockaddr_in6 is larger than sockaddr, where sockaddr_in4 is not. */
	sockaddr* addr = new sockaddr[2];
	socklen_t size = sizeof(sockaddr_in);
#ifdef IPV6
	bool v6 = false;
	if ((!*this->host) || strchr(this->host, ':'))
		v6 = true;

	if (v6)
	{
		this->fd = socket(AF_INET6, SOCK_STREAM, 0);
		if ((this->fd > -1) && ((strstr(this->IP,"::ffff:") != (char*)&this->IP) && (strstr(this->IP,"::FFFF:") != (char*)&this->IP)))
		{
			if (!this->BindAddr(this->cbindip))
			{
				delete[] addr;
				return false;
			}
		}
	}
	else
#endif
	{
		this->fd = socket(AF_INET, SOCK_STREAM, 0);
		if (this->fd > -1)
		{
			if (!this->BindAddr(this->cbindip))
			{
				delete[] addr;
				return false;
			}
		}
	}

	if (this->fd == -1)
	{
		this->state = I_ERROR;
		this->OnError(I_ERR_SOCKET);
		delete[] addr;
		return false;
	}

#ifdef IPV6
	if (v6)
	{
		in6_addr addy;
		if (inet_pton(AF_INET6, this->host, &addy) > 0)
		{
			((sockaddr_in6*)addr)->sin6_family = AF_INET6;
			memcpy(&((sockaddr_in6*)addr)->sin6_addr, &addy, sizeof(addy));
			((sockaddr_in6*)addr)->sin6_port = htons(this->port);
			size = sizeof(sockaddr_in6);
		}
	}
	else
#endif
	{
		in_addr addy;
		if (inet_aton(this->host, &addy) > 0)
		{
			((sockaddr_in*)addr)->sin_family = AF_INET;
			((sockaddr_in*)addr)->sin_addr = addy;
			((sockaddr_in*)addr)->sin_port = htons(this->port);
		}
	}

	Instance->SE->NonBlocking(this->fd);

#ifdef WIN32
	/* UGH for the LOVE OF ZOMBIE JESUS SOMEONE FIX THIS!!!!!!!!!!! */
	Instance->SE->Blocking(this->fd);
#endif

	if (Instance->SE->Connect(this, (sockaddr*)addr, size) == -1)
	{
		if (errno != EINPROGRESS)
		{
			this->OnError(I_ERR_CONNECT);
			this->Close();
			this->state = I_ERROR;
			return false;
		}

		this->Timeout = new SocketTimeout(this->GetFd(), this->Instance, this, timeout_val, this->Instance->Time());
		this->Instance->Timers->AddTimer(this->Timeout);
	}
#ifdef WIN32
	/* CRAQ SMOKING STUFF TO BE FIXED */
	Instance->SE->NonBlocking(this->fd);
#endif
	this->state = I_CONNECTING;
	if (this->fd > -1)
	{
		if (!this->Instance->SE->AddFd(this))
		{
			this->OnError(I_ERR_NOMOREFDS);
			this->Close();
			this->state = I_ERROR;
			return false;
		}
		this->SetQueues(this->fd);
	}

	Instance->Log(DEBUG,"BufferedSocket::DoConnect success");
	return true;
}


void BufferedSocket::Close()
{
	/* Save this, so we dont lose it,
	 * otherise on failure, error messages
	 * might be inaccurate.
	 */
	int save = errno;
	if (this->fd > -1)
	{
		if (this->IsIOHooked && Instance->Config->GetIOHook(this))
		{
			try
			{
				Instance->Config->GetIOHook(this)->OnRawSocketClose(this->fd);
			}
			catch (CoreException& modexcept)
			{
				Instance->Log(DEFAULT,"%s threw an exception: %s", modexcept.GetSource(), modexcept.GetReason());
			}
		}
		Instance->SE->Shutdown(this, 2);
		if (Instance->SE->Close(this) != -1)
			this->OnClose();

		if (Instance->SocketCull.find(this) == Instance->SocketCull.end())
			Instance->SocketCull[this] = this;
	}
	errno = save;
}

std::string BufferedSocket::GetIP()
{
	return this->IP;
}

char* BufferedSocket::Read()
{
	if (!Instance->SE->BoundsCheckFd(this))
		return NULL;

	int n = 0;

	if (this->IsIOHooked)
	{
		int result2 = 0;
		int MOD_RESULT = 0;
		try
		{
			MOD_RESULT = Instance->Config->GetIOHook(this)->OnRawSocketRead(this->fd,this->ibuf,sizeof(this->ibuf),result2);
		}
		catch (CoreException& modexcept)
		{
			Instance->Log(DEFAULT,"%s threw an exception: %s", modexcept.GetSource(), modexcept.GetReason());
		}
		if (MOD_RESULT < 0)
		{
			n = -1;
			errno = EAGAIN;
		}
		else
		{
			n = result2;
		}
	}
	else
	{
		n = recv(this->fd,this->ibuf,sizeof(this->ibuf),0);
	}

	if ((n > 0) && (n <= (int)sizeof(this->ibuf)))
	{
		ibuf[n] = 0;
		return ibuf;
	}
	else
	{
		int err = errno;
		if (err == EAGAIN)
			return "";
		else
			return NULL;
	}
}

void BufferedSocket::MarkAsClosed()
{
}

// There are two possible outcomes to this function.
// It will either write all of the data, or an undefined amount.
// If an undefined amount is written the connection has failed
// and should be aborted.
int BufferedSocket::Write(const std::string &data)
{
	/* Try and append the data to the back of the queue, and send it on its way
	 */
	outbuffer.push_back(data);
	this->Instance->SE->WantWrite(this);
	return (!this->FlushWriteBuffer());
}

bool BufferedSocket::FlushWriteBuffer()
{
	errno = 0;
	if ((this->fd > -1) && (this->state == I_CONNECTED))
	{
		if (this->IsIOHooked)
		{
			while (outbuffer.size() && (errno != EAGAIN))
			{
				try
				{
					/* XXX: The lack of buffering here is NOT a bug, modules implementing this interface have to
					 * implement their own buffering mechanisms
					 */
					Instance->Config->GetIOHook(this)->OnRawSocketWrite(this->fd, outbuffer[0].c_str(), outbuffer[0].length());
					outbuffer.pop_front();
				}
				catch (CoreException& modexcept)
				{
					Instance->Log(DEBUG,"%s threw an exception: %s", modexcept.GetSource(), modexcept.GetReason());
					return true;
				}
			}
		}
		else
		{
			/* If we have multiple lines, try to send them all,
			 * not just the first one -- Brain
			 */
			while (outbuffer.size() && (errno != EAGAIN))
			{
				/* Send a line */
				int result = Instance->SE->Send(this, outbuffer[0].c_str(), outbuffer[0].length(), 0);

				if (result > 0)
				{
					if ((unsigned int)result >= outbuffer[0].length())
					{
						/* The whole block was written (usually a line)
						 * Pop the block off the front of the queue,
						 * dont set errno, because we are clear of errors
						 * and want to try and write the next block too.
						 */
						outbuffer.pop_front();
					}
					else
					{
						std::string temp = outbuffer[0].substr(result);
						outbuffer[0] = temp;
						/* We didnt get the whole line out. arses.
						 * Try again next time, i guess. Set errno,
						 * because we shouldnt be writing any more now,
						 * until the socketengine says its safe to do so.
						 */
						errno = EAGAIN;
					}
				}
				else if (result == 0)
				{
					this->Instance->SE->DelFd(this);
					this->Close();
					return true;
				}
				else if ((result == -1) && (errno != EAGAIN))
				{
					this->OnError(I_ERR_WRITE);
					this->state = I_ERROR;
					this->Instance->SE->DelFd(this);
					this->Close();
					return true;
				}
			}
		}
	}

	if ((errno == EAGAIN) && (fd > -1))
	{
		this->Instance->SE->WantWrite(this);
	}

	return (fd < 0);
}

void SocketTimeout::Tick(time_t)
{
	ServerInstance->Log(DEBUG,"SocketTimeout::Tick");

	if (ServerInstance->SE->GetRef(this->sfd) != this->sock)
		return;

	if (this->sock->state == I_CONNECTING)
	{
		// for non-listening sockets, the timeout can occur
		// which causes termination of the connection after
		// the given number of seconds without a successful
		// connection.
		this->sock->OnTimeout();
		this->sock->OnError(I_ERR_TIMEOUT);
		this->sock->timeout = true;

		/* NOTE: We must set this AFTER DelFd, as we added
		 * this socket whilst writeable. This means that we
		 * must DELETE the socket whilst writeable too!
		 */
		this->sock->state = I_ERROR;

		if (ServerInstance->SocketCull.find(this->sock) == ServerInstance->SocketCull.end())
			ServerInstance->SocketCull[this->sock] = this->sock;
	}

	this->sock->Timeout = NULL;
}

bool BufferedSocket::Poll()
{
	int incoming = -1;

#ifndef WINDOWS
	if (!Instance->SE->BoundsCheckFd(this))
		return false;
#endif

	if (Instance->SE->GetRef(this->fd) != this)
		return false;

	switch (this->state)
	{
		case I_CONNECTING:
			/* Our socket was in write-state, so delete it and re-add it
			 * in read-state.
			 */
#ifndef WINDOWS
			if (this->fd > -1)
			{
				this->Instance->SE->DelFd(this);
				if (!this->Instance->SE->AddFd(this))
					return false;
			}
#endif
			this->SetState(I_CONNECTED);

			if (Instance->Config->GetIOHook(this))
			{
				Instance->Log(DEBUG,"Hook for raw connect");
				try
				{
					Instance->Config->GetIOHook(this)->OnRawSocketConnect(this->fd);
				}
				catch (CoreException& modexcept)
				{
					Instance->Log(DEBUG,"%s threw an exception: %s", modexcept.GetSource(), modexcept.GetReason());
				}
			}
			return this->OnConnected();
		break;
		case I_LISTENING:
		{
			/* The [2] is required because we may write a sockaddr_in6 here, and sockaddr_in6 is larger than sockaddr, where sockaddr_in4 is not. */
			sockaddr* client = new sockaddr[2];
			length = sizeof (sockaddr_in);
			std::string recvip;
#ifdef IPV6
			if ((!*this->host) || strchr(this->host, ':'))
				length = sizeof(sockaddr_in6);
#endif
			incoming = Instance->SE->Accept(this, client, &length);
#ifdef IPV6
			if ((!*this->host) || strchr(this->host, ':'))
			{
				char buf[1024];
				recvip = inet_ntop(AF_INET6, &((sockaddr_in6*)client)->sin6_addr, buf, sizeof(buf));
			}
			else
#endif
			Instance->SE->NonBlocking(incoming);

			recvip = inet_ntoa(((sockaddr_in*)client)->sin_addr);
			this->OnIncomingConnection(incoming, (char*)recvip.c_str());

			if (this->IsIOHooked)
			{
				try
				{
					Instance->Config->GetIOHook(this)->OnRawSocketAccept(incoming, recvip.c_str(), this->port);
				}
				catch (CoreException& modexcept)
				{
					Instance->Log(DEBUG,"%s threw an exception: %s", modexcept.GetSource(), modexcept.GetReason());
				}
			}

			this->SetQueues(incoming);

			delete[] client;
			return true;
		}
		break;
		case I_CONNECTED:
			/* Process the read event */
			return this->OnDataReady();
		break;
		default:
		break;
	}
	return true;
}

void BufferedSocket::SetState(BufferedSocketState s)
{
	this->state = s;
}

BufferedSocketState BufferedSocket::GetState()
{
	return this->state;
}

int BufferedSocket::GetFd()
{
	return this->fd;
}

bool BufferedSocket::OnConnected() { return true; }
void BufferedSocket::OnError(BufferedSocketError) { return; }
int BufferedSocket::OnDisconnect() { return 0; }
int BufferedSocket::OnIncomingConnection(int, char*) { return 0; }
bool BufferedSocket::OnDataReady() { return true; }
bool BufferedSocket::OnWriteReady() { return true; }
void BufferedSocket::OnTimeout() { return; }
void BufferedSocket::OnClose() { return; }

BufferedSocket::~BufferedSocket()
{
	this->Close();
	if (Timeout)
	{
		Instance->Timers->DelTimer(Timeout);
		Timeout = NULL;
	}
}

void BufferedSocket::HandleEvent(EventType et, int errornum)
{
	switch (et)
	{
		case EVENT_ERROR:
			switch (errornum)
			{
				case ETIMEDOUT:
					this->OnError(I_ERR_TIMEOUT);
				break;
				case ECONNREFUSED:
				case 0:
					this->OnError(this->state == I_CONNECTING ? I_ERR_CONNECT : I_ERR_WRITE);
				break;
				case EADDRINUSE:
					this->OnError(I_ERR_BIND);
				break;
				case EPIPE:
				case EIO:
					this->OnError(I_ERR_WRITE);
				break;
			}
			if (this->Instance->SocketCull.find(this) == this->Instance->SocketCull.end())
				this->Instance->SocketCull[this] = this;
			return;
		break;
		case EVENT_READ:
			if (!this->Poll())
			{
				if (this->Instance->SocketCull.find(this) == this->Instance->SocketCull.end())
					this->Instance->SocketCull[this] = this;
				return;
			}
		break;
		case EVENT_WRITE:
			if (this->WaitingForWriteEvent)
			{
				this->WaitingForWriteEvent = false;
				if (!this->OnWriteReady())
				{
					if (this->Instance->SocketCull.find(this) == this->Instance->SocketCull.end())
						this->Instance->SocketCull[this] = this;
					return;
				}
			}
			if (this->state == I_CONNECTING)
			{
				/* This might look wrong as if we should be actually calling
				 * with EVENT_WRITE, but trust me it is correct. There are some
				 * writeability-state things in the read code, because of how
				 * BufferedSocket used to work regarding write buffering in previous
				 * versions of InspIRCd. - Brain
				 */
				this->HandleEvent(EVENT_READ);
				return;
			}
			else
			{
				if (this->FlushWriteBuffer())
				{
					if (this->Instance->SocketCull.find(this) == this->Instance->SocketCull.end())
						this->Instance->SocketCull[this] = this;
					return;
				}
			}
		break;
	}
}

