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

/*
class CoreExport CGIRequest : public EventHandler
{
	CGIRequest(Connection *parent, 
};
*/

class ModuleCGI : public Module
{
 private:
	 std::map<std::string, std::string> CGITypes;

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

		Implementation eventlist[] = { I_OnPreRequest };
		ServerInstance->Modules->Attach(eventlist, this, 1);
	}
	
	virtual ~ModuleCGI()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1, 0, 0, 0, VF_VENDOR, API_VERSION);
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
			return 0;

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


				static char ReadBuffer[65535]; // if you change this size, remember to change the read() call below!
				std::string res;
				int result = EAGAIN;

				while (result != 0)
				{
					#ifndef WIN32
						result = read(from_child_fd[0], (char *)ReadBuffer, 65535);
					#else
						result = recv(from_child_fd[0], (char*)ReadBuffer, 65535, 0);
					#endif

					if (result == -1)
					{
						if (errno != EAGAIN)
						{
							ServerInstance->Log(DEBUG, "read returned error, %s", strerror(errno));
							break;
						}
					}
					else
					{
						ReadBuffer[result] = '\0';
						ServerInstance->Log(DEBUG, "read returned %d bytes", result);
						res += ReadBuffer;
					}
				}

				if (res.length() == 0)
				{
					return 0; // let core handle it, probably not executable
				}

				ServerInstance->Log(DEBUG, "total request size: %d", res.length());

				HTTPHeaders empty;
				c->SendHeaders(res.length(), 200, "OK", empty);
				c->State = HTTP_SEND_DATA;
				c->Write(res);
				c->ResponseBufferDone = true;

				close(from_child_fd[0]);
				return 1;
				break;
		}

		

		return 1;
		/*


		HTTPHeaders empty;
		c->SendHeaders(4, 200, "OK", empty);
		c->State = HTTP_SEND_DATA;
		c->Write("lawl");
		c->ResponseBufferDone = true;

		ServerInstance->Log(DEBUG, "Got a %s request from %d on %s for %s and file %s", method.c_str(), c->GetFd(), vhost.c_str(), dir.c_str(), file.c_str());
		return 1;
		*/
	}
};


MODULE_INIT(ModuleCGI)

