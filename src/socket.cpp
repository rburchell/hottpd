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


/* $Core: libhttpd_socket */

#include "inspircd.h"
#include "socket.h"
#include "socketengine.h"

ListenSocket::ListenSocket(InspIRCd* Instance, int port, char* addr) : ServerInstance(Instance), desc("plaintext"), bind_addr(addr), bind_port(port)
{
	this->SetFd(utils::sockets::OpenTCPSocket(addr));
	if (this->GetFd() > -1)
	{
		if (!Instance->BindSocket(this->fd,port,addr))
		{
			Instance->Log(DEBUG, "Failed to bind listener: %s (%d)", strerror(errno), errno);
			this->fd = -1;
		}
#ifdef IPV6
		if ((!*addr) || (strchr(addr,':')))
			this->family = AF_INET6;
		else
#endif
		this->family = AF_INET;
		Instance->SE->AddFd(this);
	}
}

ListenSocket::~ListenSocket()
{
	if (this->GetFd() > -1)
	{
		ServerInstance->SE->DelFd(this);
		ServerInstance->Log(DEBUG,"Shut down listener on fd %d", this->fd);
		if (ServerInstance->SE->Shutdown(this, 2) || ServerInstance->SE->Close(this))
			ServerInstance->Log(DEBUG,"Failed to cancel listener: %s", strerror(errno));
		this->fd = -1;
	}
}

// XXX - There's probably a nicer way to do this.
static sockaddr *sock_us;
static sockaddr *client;
static bool setup_sock;

void ListenSocket::HandleEvent(EventType, int)
{
	if (!setup_sock)
	{
		sock_us = new sockaddr[2];
		client = new sockaddr[2];
		setup_sock = true;
	}

	socklen_t uslen, length;		// length of our port number
	int fd, in_port;

#ifdef IPV6
	if (this->family == AF_INET6)
	{
		uslen = sizeof(sockaddr_in6);
		length = sizeof(sockaddr_in6);
	}
	else
#endif
	{
		uslen = sizeof(sockaddr_in);
		length = sizeof(sockaddr_in);
	}

	do
	{
		if ((ServerInstance->local_connections.size() + 1) > ServerInstance->Config->SoftLimit ||
			(ServerInstance->local_connections.size() + 1) >= MAXCLIENTS)
		{
			/*
			 * Don't even *try* to accept under these conditions, get the fuck out of here.
			 * This will doubtlessly cause more read events, but pulling in the queue is
			 * going to do no good for us.
			 *
			 * XXX: in the future, perhaps a way to avoid monitoring listeners under
			 * such a condition would be nice.
			 *		-- w00t
			 */
			break;
		}

		fd = ServerInstance->SE->Accept(this, (sockaddr*)client, &length);

		/*
		 * XXX -
		 * This is done as a safety check to keep the file descriptors within an acceptable range.
		 * If there is ever an fd over 65,535, 65,536 connections must exist to this httpd, which seems
		 * quite insane, even allowing for modules to create their own connections, so this probably
		 * would indicate an FD leak if anything - but regardless, we can't actually use any FDs this bug.. so..
		 * kill it.
		 */
#ifndef WINDOWS
		if ((unsigned int)fd >= MAX_DESCRIPTORS)
		{
			close(fd);
			shutdown(fd, 2);
			break;
		}
#endif

		if ((fd > -1) && (!ServerInstance->SE->GetSockName(this, sock_us, &uslen)))
		{
			char buf[MAXBUF];
	#ifdef IPV6
			if (this->family == AF_INET6)
			{
				inet_ntop(AF_INET6, &((const sockaddr_in6*)client)->sin6_addr, buf, sizeof(buf));
				in_port = ntohs(((sockaddr_in6*)sock_us)->sin6_port);
			}
			else
	#endif
			{
				inet_ntop(AF_INET, &((const sockaddr_in*)client)->sin_addr, buf, sizeof(buf));
				in_port = ntohs(((sockaddr_in*)sock_us)->sin_port);
			}

			ServerInstance->SE->NonBlocking(fd);
			ServerInstance->Connections->Add(fd, in_port, this->family, client);
		}
		else
		{
			ServerInstance->SE->Shutdown(fd, 2);
			ServerInstance->SE->Close(fd);
		}
	} while (ServerInstance->SE->CanMultiaccept && fd >= 0);
}

/** This will bind a socket to a port. It works for UDP/TCP.
 * It can only bind to IP addresses, if you wish to bind to hostnames
 * you should first resolve them using class 'Resolver'.
 */ 
bool InspIRCd::BindSocket(int sockfd, int port, char* addr, bool dolisten)
{
	/* We allocate 2 of these, because sockaddr_in6 is larger than sockaddr (ugh, hax) */
	sockaddr* server = new sockaddr[2];
	memset(server,0,sizeof(sockaddr)*2);

	int ret, size;

	if (*addr == '*')
		*addr = 0;

#ifdef IPV6
	if (*addr)
	{
		/* There is an address here. Is it ipv6? */
		if (strchr(addr,':'))
		{
			/* Yes it is */
			in6_addr addy;
			if (inet_pton(AF_INET6, addr, &addy) < 1)
			{
				delete[] server;
				return false;
			}

			((sockaddr_in6*)server)->sin6_family = AF_INET6;
			memcpy(&(((sockaddr_in6*)server)->sin6_addr), &addy, sizeof(in6_addr));
			((sockaddr_in6*)server)->sin6_port = htons(port);
			size = sizeof(sockaddr_in6);
		}
		else
		{
			/* No, its not */
			in_addr addy;
			if (inet_pton(AF_INET, addr, &addy) < 1)
			{
				delete[] server;
				return false;
			}

			((sockaddr_in*)server)->sin_family = AF_INET;
			((sockaddr_in*)server)->sin_addr = addy;
			((sockaddr_in*)server)->sin_port = htons(port);
			size = sizeof(sockaddr_in);
		}
	}
	else
	{
		if (port == -1)
		{
			/* Port -1: Means UDP IPV4 port binding - Special case
			 * used by DNS engine.
			 */
			((sockaddr_in*)server)->sin_family = AF_INET;
			((sockaddr_in*)server)->sin_addr.s_addr = htonl(INADDR_ANY);
			((sockaddr_in*)server)->sin_port = 0;
			size = sizeof(sockaddr_in);
		}
		else
		{
			/* Theres no address here, default to ipv6 bind to all */
			((sockaddr_in6*)server)->sin6_family = AF_INET6;
			memset(&(((sockaddr_in6*)server)->sin6_addr), 0, sizeof(in6_addr));
			((sockaddr_in6*)server)->sin6_port = htons(port);
			size = sizeof(sockaddr_in6);
		}
	}
#else
	/* If we aren't built with ipv6, the choice becomes simple */
	((sockaddr_in*)server)->sin_family = AF_INET;
	if (*addr)
	{
		/* There is an address here. */
		in_addr addy;
		if (inet_pton(AF_INET, addr, &addy) < 1)
		{
			delete[] server;
			return false;
		}
		((sockaddr_in*)server)->sin_addr = addy;
	}
	else
	{
		/* Bind ipv4 to all */
		((sockaddr_in*)server)->sin_addr.s_addr = htonl(INADDR_ANY);
	}
	/* Bind ipv4 port number */
	((sockaddr_in*)server)->sin_port = htons(port);
	size = sizeof(sockaddr_in);
#endif
	ret = SE->Bind(sockfd, server, size);
	delete[] server;

	if (ret < 0)
	{
		return false;
	}
	else
	{
		if (dolisten)
		{
			if (SE->Listen(sockfd, Config->MaxConn) == -1)
			{
				this->Log(DEFAULT,"ERROR in listen(): %s",strerror(errno));
				return false;
			}
			else
			{
				this->Log(DEBUG,"New socket binding for %d with listen: %s:%d", sockfd, addr, port);
				SE->NonBlocking(sockfd);
				return true;
			}
		}
		else
		{
			this->Log(DEBUG,"New socket binding for %d without listen: %s:%d", sockfd, addr, port);
			return true;
		}
	}
}

// Open a TCP Socket
int utils::sockets::OpenTCPSocket(char* addr, int socktype)
{
	int sockfd;
	int on = 1;
	addr = addr;
	struct linger linger = { 0, 0 };
#ifdef IPV6
	if (strchr(addr,':') || (!*addr))
		sockfd = socket (PF_INET6, socktype, 0);
	else
		sockfd = socket (PF_INET, socktype, 0);
	if (sockfd < 0)
#else
	if ((sockfd = socket (PF_INET, socktype, 0)) < 0)
#endif
	{
		return ERROR;
	}
	else
	{
		setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(on));
		/* This is BSD compatible, setting l_onoff to 0 is *NOT* http://web.irc.org/mla/ircd-dev/msg02259.html */
		linger.l_onoff = 1;
		linger.l_linger = 1;
		setsockopt(sockfd, SOL_SOCKET, SO_LINGER, (char*)&linger,sizeof(linger));
		return (sockfd);
	}
}

int InspIRCd::BindPorts(bool, int &ports_found, FailedPortList &failed_ports)
{
	char configToken[MAXBUF], Addr[MAXBUF];
	int bound = 0;
	bool started_with_nothing = (Config->ports.size() == 0);
	std::vector<std::pair<std::string, int> > old_ports;

	/* XXX: Make a copy of the old ip/port pairs here */
	for (std::vector<ListenSocket*>::iterator o = Config->ports.begin(); o != Config->ports.end(); ++o)
		old_ports.push_back(make_pair((*o)->GetIP(), (*o)->GetPort()));

	for (int count = 0; count < Config->ConfValueEnum(Config->config_data, "bind"); count++)
	{
		Config->ConfValue(Config->config_data, "bind", "port", count, configToken, MAXBUF);
		Config->ConfValue(Config->config_data, "bind", "address", count, Addr, MAXBUF);
		
		if (strncmp(Addr, "::ffff:", 7) == 0)
			this->Log(DEFAULT, "Using 4in6 (::ffff:) isn't recommended. You should bind IPv4 addresses directly instead.");
		
		utils::portparser portrange(configToken, false);
		int portno = -1;
		while ((portno = portrange.GetToken()))
		{
			if (*Addr == '*')
				*Addr = 0;

			bool skip = false;
			for (std::vector<ListenSocket*>::iterator n = Config->ports.begin(); n != Config->ports.end(); ++n)
			{
				if (((*n)->GetIP() == Addr) && ((*n)->GetPort() == portno))
				{
					skip = true;
					/* XXX: Here, erase from our copy of the list */
					for (std::vector<std::pair<std::string, int> >::iterator k = old_ports.begin(); k != old_ports.end(); ++k)
					{
						if ((k->first == Addr) && (k->second == portno))
						{
							old_ports.erase(k);
							break;
						}
					}
				}
			}
			if (!skip)
			{
				ListenSocket* ll = new ListenSocket(this, portno, Addr);
				if (ll->GetFd() > -1)
				{
					bound++;
					Config->ports.push_back(ll);
				}
				else
				{
					failed_ports.push_back(std::make_pair(Addr, portno));
				}
				ports_found++;
			}
		}
	}

	/* XXX: Here, anything left in our copy list, close as removed */
	if (!started_with_nothing)
	{
		for (size_t k = 0; k < old_ports.size(); ++k)
		{
			for (std::vector<ListenSocket*>::iterator n = Config->ports.begin(); n != Config->ports.end(); ++n)
			{
				if (((*n)->GetIP() == old_ports[k].first) && ((*n)->GetPort() == old_ports[k].second))
				{
					this->Log(DEFAULT,"Port binding %s:%d was removed from the config file, closing.", old_ports[k].first.c_str(), old_ports[k].second);
					delete *n;
					Config->ports.erase(n);
					break;
				}
			}
		}
	}

	return bound;
}

const char* utils::sockets::insp_ntoa(insp_inaddr n)
{
	static char buf[1024];
	inet_ntop(AF_FAMILY, &n, buf, sizeof(buf));
	return buf;
}

int utils::sockets::insp_aton(const char* a, insp_inaddr* n)
{
	return inet_pton(AF_FAMILY, a, n);
}

