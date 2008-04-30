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

#include "inspircd.h"

/* $ModDesc: Provides support for CGI applications */

static int total_cgi_processes = 0;

class CoreExport CGIRequest : public EventHandler
{
 private:
	InspIRCd *ServerInstance;
	Connection *c;
	std::string rbuf;

 public:
	bool done;

	CGIRequest(InspIRCd *Instance, Connection *parent) : ServerInstance(Instance), c(parent), done(false)
	{
		ServerInstance->Log(DEBUG, "Created CGI request");
		total_cgi_processes++;
	}

	~CGIRequest()
	{
		ServerInstance->Log(DEBUG, "Destroying CGI request");
		this->Close(false);
		total_cgi_processes--;
	}

	virtual void HandleEvent(EventType et, int errornum = 0)
	{
		switch (et)
		{
			case EVENT_READ:
				this->OnRead();
				break;
			case EVENT_WRITE:
				/* ignore */
				break;
			case EVENT_ERROR:
				ServerInstance->Log(DEBUG, "Error CGI socket %d (%d: %s)", GetFd(), errno, strerror(errno));
				this->Close(true);
				break;
		}
	}

	void Close(bool SendResponse)
	{
		/*
		 * Remove ident socket from engine, and close it, but dont detatch it
		 * from its parent user class, or attempt to delete its memory.
		 */
		if (GetFd() > -1)
		{
			ServerInstance->Log(DEBUG, "Close CGI socket %d", GetFd());
			ServerInstance->SE->DelFd(this);
			ServerInstance->SE->Close(GetFd());
			ServerInstance->SE->Shutdown(GetFd(), SHUT_WR);
			this->SetFd(-1);
			done = true;


			if (SendResponse)
			{
				// Send response
				ServerInstance->Log(DEBUG, "Sending response. Total request size: %d", rbuf.length());

				HTTPHeaders empty;
				c->SendHeaders(rbuf.length(), 200, "OK", empty);
				c->State = HTTP_SEND_DATA;
				c->Write(rbuf);
				c->ResponseBufferDone = true;
			}
		}
	}


	void OnRead()
	{
		static char ReadBuffer[65535]; // if you change this size, remember to change the read() call below!
		std::string res;
		int result = 1; // set to 1 so while executes once at least

		#ifndef WIN32
			result = read(this->fd, (char *)ReadBuffer, 65535);
		#else
			result = recv(this->fd, (char*)ReadBuffer, 65535, 0);
		#endif

		if (result == -1)
		{
			if (errno != EAGAIN)
			{
				throw "CGI read errored";
				ServerInstance->Log(DEBUG, "read returned error, %s", strerror(errno));
				return;
			}
		}
		else
		{
			ReadBuffer[result] = '\0';
			//ServerInstance->Log(DEBUG, "read returned %d bytes", result);
			rbuf += ReadBuffer;
		}

		if (result == 0)
		{
			ServerInstance->Log(DEBUG, "CGI returned EOF");

			if (rbuf.length() == 0)
			{
				throw "rbuf length 0 after reading CGI (how to handle this?)";
			}

			this->Close(true);
		}
	}
};

class ModuleCGI : public Module
{
 private:
	std::map<std::string, std::string> CGITypes;
	std::map<Connection *, CGIRequest *> CGIRequests;

 public:
	ModuleCGI(InspIRCd *Srv) : Module(Srv)
	{
		// Read config.
		ConfigReader Conf(ServerInstance);
		CGITypes.clear();

		for (int i = 0; i < Conf.Enumerate("cgi"); i++)
		{
			std::string extension = Conf.ReadValue("cgi", "extension", i);
			std::string executable = Conf.ReadValue("cgi", "executable", i);

			CGITypes[extension] = executable;
		}

		Implementation eventlist[] = { I_OnPreRequest, I_OnBufferFlushed, I_OnConnectionDisconnect };
		ServerInstance->Modules->Attach(eventlist, this, 3);
	}
	
	virtual ~ModuleCGI()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1, 0, 0, 0, VF_VENDOR, API_VERSION);
	}

	virtual void OnBufferFlushed(Connection *c)
	{
		std::map<Connection *, CGIRequest *>::iterator i = CGIRequests.find(c);

		ServerInstance->Log(DEBUG, "Buffer flushed for %d", c->GetFd());

		if (i != CGIRequests.end())
		{
			ServerInstance->Log(DEBUG, "Deleted");
			delete i->second;
			CGIRequests.erase(i);
		}
	}

	virtual void OnConnectionDisconnect(Connection *c)
	{
		std::map<Connection *, CGIRequest *>::iterator i = CGIRequests.find(c);

		ServerInstance->Log(DEBUG, "Disconnect for %d", c->GetFd());
		if (i != CGIRequests.end())
		{
			ServerInstance->Log(DEBUG, "Deleted");
			delete i->second;
			CGIRequests.erase(i);
		}
	}

	// Also cull completed requests that are just hanging around..
	virtual void OnBackgroundTimer(time_t now)
	{
		bool go_again = true;

		while (go_again)
		{
			go_again = false;

			std::map<Connection *, CGIRequest *>::iterator i;

			ServerInstance->Log(DEBUG, "Going again");
			
			for (i = CGIRequests.begin(); i != CGIRequests.end(); i++)
			{
				if (i->second->done)
				{
					ServerInstance->Log(DEBUG, "Deleting %d", i->first->GetFd());
					delete i->second;
					CGIRequests.erase(i);
					go_again = true;
					break;
				}
			}
		}
	}

	virtual int OnPreRequest(Connection *c, const std::string &method, const std::string &vhost, const std::string &dir, const std::string &file)
	{
		/*
		 * so, we have a request!
		 * for now, we're going to treat all requests as CGI requests, simply because
		 * I really don't want to have to deal with this in other ways just now.
		 *
		 * step 1: create two sets of FDs via pipe(): one to recieve from child, one to send from parent
		 * step 2: fork() (see step #3 for why)
		 * step 3: set up environment in child
		 * step 4: exec*() new process (NOTE: this replaces the calling process, which is why we need to fork)
		 * step 5: ???
		 * step 6: profit.
		 */
		int to_child_fd[2];
		int from_child_fd[2];
		pid_t forkres;
		struct stat *fst = NULL;
		std::string upath;
		std::string exe;

		// probably requesting a dir, drop it.
		if (file.empty())
			return 0;

		size_t pos;

		pos = file.rfind('.');

		// we can't touch something with no extension.
		if (pos == std::string::npos)
			return 0;

		std::string ext = file.substr((pos + 1), file.length());

		// . on the end of the file - what madness is this?
		if (ext.empty())
			return 0;

		std::map<std::string, std::string>::iterator i = CGITypes.find(ext);

		// defined CGI type?
		if (i == CGITypes.end())
		{
			return 0;
		}

		if (total_cgi_processes + 1 > ServerInstance->Config->MaximumDynamicProcesses)
		{
			// error 500
			c->SendError(500, "Internal error", true);
			return 1;
		}

		exe = i->second;

		/* before anything, get the full path and make sure we can access it! (XXX copy paste :() */
		upath = ServerInstance->FileSys->CheckFilePath(ServerInstance->Config->DocRoot, c->uri, fst);

		if (upath.empty())
		{
			switch (errno)
			{
				case EACCES:
					c->SendError(403, "Forbidden", true);
					break;
				case ENOENT:
				case ELOOP:
				case ENAMETOOLONG:
				case ENOTDIR:
					c->SendError(404, "File Not Found", true);
					break;
				default:
					c->SendError(500, "Internal Server Error", true);
					break;
			}

			return 1;
		}

		/* step 1. */
		if (pipe(to_child_fd) == -1)
		{
			c->SendError(500, "Internal error", true);
			return 1;
		}

		if (pipe(from_child_fd) == -1)
		{
			c->SendError(500, "Internal error", true);
			return 1;
		}

		/* step 2. */
 		forkres = fork();

		if (forkres == -1)
		{
			/* error, fuck .. */
			c->SendError(500, "Internal error", true);
			return 1;
		}

		switch (forkres)
		{
			case 0:
				/* we're the child */

				/*
				 * step 3.
				 *  first, we need to move STDIN and STDOUT to the FDs we created pre-fork.
				 */

				// Create stdout: we want to write to from_child. #1 can be written to, so dup2() it to stdout.
				dup2(from_child_fd[1], STDOUT_FILENO);

				// And move stdin to to_child, so parent can write to us.
				dup2(to_child_fd[0], STDIN_FILENO);

				// Close unneeded
				close(from_child_fd[0]);
				close(to_child_fd[1]);

				// Set environment variables: this doesn't cause problems, because we fork()ed. (we probably *should not* assume this succeeds)
				setenv("SERVER_SOFTWARE", "hottpd", 1);
				setenv("GATEWAY_INTERFACE", "CGI/1.1", 1);
				setenv("SCRIPT_FILENAME", upath.c_str(), 1);
				// TODO: set moar here.

				if (exe.empty())
				{
					// no exe defined, invoke the proc itself
					execvp(upath.c_str(), NULL);
				}
				else
				{
					// Custom handler defined for this type, run it with the file as a param
					// XXX: we really should copy these strings rather than casting.
					char *argv[3];
					argv[0] = (char *)exe.c_str();
					argv[1] = (char *)upath.c_str();
					argv[2] = NULL;

					execvp(exe.c_str(), argv);
				}

				exit(0);
				break;
			default:
				/* parent */

				// Close unneeded
				close(from_child_fd[1]);
				close(to_child_fd[0]);

				// We may need to write stuff to CGI here. Let's assume we don't.
				close(to_child_fd[1]);

				// read from_child_fd[0].
				CGIRequest *cr = new CGIRequest(ServerInstance, c);
				cr->SetFd(from_child_fd[0]);

				if (!ServerInstance->SE->AddFd(cr))
				{
					ServerInstance->Log(DEBUG,"Internal error on CGI connection(!)");
					delete cr;
					return 1;
				}

				CGIRequests[c] = cr;

				return 1;
				break;
		}

		return 1;
	}
};


MODULE_INIT(ModuleCGI)

