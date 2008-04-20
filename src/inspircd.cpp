/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* $Install: src/hottpd $(BINPATH) */

#include "inspircd.h"
#include <signal.h>

#ifndef WIN32
	#include <dirent.h>
	#include <unistd.h>
	#include <sys/resource.h>
	#include <dlfcn.h>
	#include <getopt.h>
	#include <pwd.h> // setuid
	#include <grp.h> // setgid

	/* Some systems don't define RUSAGE_SELF. This should fix them. */
	#ifndef RUSAGE_SELF
		#define RUSAGE_SELF 0
	#endif
#endif

#include <fstream>
#include "socketengine.h"
#include "inspircd_se_config.h"
#include "socket.h"
#include "exitcodes.h"

using utils::sockets::insp_ntoa;
using utils::sockets::insp_inaddr;
using utils::sockets::insp_sockaddr;

InspIRCd* SI = NULL;
int* mysig = NULL;


/* Burlex: Moved from exitcodes.h -- due to duplicate symbols */
const char* ExitCodes[] =
{
		"No error", /* 0 */
		"DIE command", /* 1 */
		"execv() failed", /* 2 */
		"Internal error", /* 3 */
		"Config file error", /* 4 */
		"Logfile error", /* 5 */
		"POSIX fork failed", /* 6 */
		"Bad commandline parameters", /* 7 */
		"No ports could be bound", /* 8 */
		"Can't write PID file", /* 9 */
		"SocketEngine could not initialize", /* 10 */
		"Refusing to start up as root", /* 11 */
		"Found a <die> tag!", /* 12 */
		"Couldn't load module on startup", /* 13 */
		"Could not create windows forked process", /* 14 */
		"Received SIGTERM", /* 15 */
};

void InspIRCd::Cleanup()
{
	if (Config)
	{
		for (unsigned int i = 0; i < Config->ports.size(); i++)
		{
			/* This calls the constructor and closes the listening socket */
			delete Config->ports[i];
		}

		Config->ports.clear();
	}

	/* Close all client sockets, or the new process inherits them */
	for (std::vector<Connection*>::const_iterator i = this->local_connections.begin(); i != this->local_connections.end(); i++)
	{
		(*i)->CloseSocket();
	}

	/* We do this more than once, so that any service providers get a
	 * chance to be unhooked by the modules using them, but then get
	 * a chance to be removed themsleves.
	 */
	for (int tries = 0; tries < 3; tries++)
	{
		std::vector<std::string> module_names = Modules->GetAllModuleNames(0);
		for (std::vector<std::string>::iterator k = module_names.begin(); k != module_names.end(); ++k)
		{
			/* Unload all modules, so they get a chance to clean up their listeners */
			this->Modules->Unload(k->c_str());
		}
	}

	/* Close logging */
	if (this->Logger)
		this->Logger->Close();
}

void InspIRCd::Restart(const std::string &reason)
{
	this->Log(DEFAULT, reason);
	this->Cleanup();

	/* Figure out our filename (if theyve renamed it, we're boned) */
	std::string me;

#ifdef WINDOWS
	char module[MAX_PATH];
	if (GetModuleFileName(NULL, module, MAX_PATH))
		me = module;
#else
	me = Config->MyDir + "/inspircd";
#endif

	if (execv(me.c_str(), Config->argv) == -1)
	{
		/* Will raise a SIGABRT if not trapped */
		throw CoreException(std::string("Failed to execv()! error: ") + strerror(errno));
	}
}

void InspIRCd::CloseLog()
{
	if (this->Logger)
		this->Logger->Close();
}

void InspIRCd::SetSignals()
{
#ifndef WIN32
	signal(SIGALRM, SIG_IGN);
	signal(SIGHUP,  InspIRCd::SetSignal);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGCHLD, SIG_IGN);
#endif
	signal(SIGTERM, InspIRCd::SetSignal);
}

void InspIRCd::QuickExit(int status)
{
	exit(0);
}

bool InspIRCd::DaemonSeed()
{
#ifdef WINDOWS
	printf_c("hottpd pid: \033[1;32m%lu\033[0m\n", GetCurrentProcessId());
	return true;
#else
	signal(SIGTERM, InspIRCd::QuickExit);

	int childpid;
	if ((childpid = fork ()) < 0)
		return false;
	else if (childpid > 0)
	{
		/* We wait here for the child process to kill us,
		 * so that the shell prompt doesnt come back over
		 * the output.
		 * Sending a kill with a signal of 0 just checks
		 * if the child pid is still around. If theyre not,
		 * they threw an error and we should give up.
		 */
		while (kill(childpid, 0) != -1)
			sleep(1);
		exit(0);
	}
	setsid ();
	umask (007);
	printf("hottpd pid: \033[1;32m%lu\033[0m\n",(unsigned long)getpid());

	signal(SIGTERM, InspIRCd::SetSignal);

	rlimit rl;
	if (getrlimit(RLIMIT_CORE, &rl) == -1)
	{
		this->Log(DEFAULT,"Failed to getrlimit()!");
		return false;
	}
	else
	{
		rl.rlim_cur = rl.rlim_max;
		if (setrlimit(RLIMIT_CORE, &rl) == -1)
			this->Log(DEFAULT,"setrlimit() failed, cannot increase coredump size.");
	}

	return true;
#endif
}

void InspIRCd::WritePID(const std::string &filename)
{
	std::string fname = (filename.empty() ? "inspircd.pid" : filename);
	if (*(fname.begin()) != '/')
	{
		std::string::size_type pos;
		std::string confpath = this->ConfigFileName;
		if ((pos = confpath.rfind("/")) != std::string::npos)
		{
			/* Leaves us with just the path */
			fname = confpath.substr(0, pos) + std::string("/") + fname;
		}
	}
	std::ofstream outfile(fname.c_str());
	if (outfile.is_open())
	{
		outfile << getpid();
		outfile.close();
	}
	else
	{
		printf("Failed to write PID-file '%s', exiting.\n",fname.c_str());
		this->Log(DEFAULT,"Failed to write PID-file '%s', exiting.",fname.c_str());
		Exit(EXIT_STATUS_PID);
	}
}

InspIRCd::InspIRCd(int argc, char** argv) : GlobalCulls(this)
{

	int found_ports = 0;
	FailedPortList pl;
	int do_version = 0, do_nofork = 0, do_debug = 0, do_nolog = 0, do_root = 0;    /* flag variables */
	char c = 0;

	ShuttingDown = false;

	SocketEngineFactory* SEF = new SocketEngineFactory();
	SE = SEF->Create(this);
	delete SEF;

	this->s_signal = 0;

	this->Config = new ServerConfig(this);
	this->Modules = new ModuleManager(this);
	this->Timers = new TimerManager(this);
	this->MimeTypes = new MimeManager(this);
	this->Connections = new ConnectionManager(this);
	this->FileSys = new FileSystem(this);

	// XXX this is kinda an ugly way to do it, might want to move this to a config tag.
	this->MimeTypes->AddType("jpg", "image/jpeg");
	this->MimeTypes->AddType("gif", "image/gif");
	this->MimeTypes->AddType("css", "text/html");
	this->MimeTypes->AddType("php", "text/html");
	this->MimeTypes->AddType("htm", "text/html");
	this->MimeTypes->AddType("html", "text/html");

	this->Config->argv = argv;
	this->Config->argc = argc;

	if (chdir(Config->GetFullProgDir().c_str()))
	{
		printf("Unable to change to my directory: %s\nAborted.", strerror(errno));
		exit(0);
	}

	this->TIME = this->OLDTIME = this->startup_time = time(NULL);
	this->time_delta = 0;
	srand(this->TIME);

	*this->LogFileName = 0;
	strlcpy(this->ConfigFileName, CONFIG_FILE, MAXBUF);

	struct option longopts[] =
	{
		{ "nofork",	no_argument,		&do_nofork,	1	},
		{ "logfile",	required_argument,	NULL,		'f'	},
		{ "config",	required_argument,	NULL,		'c'	},
		{ "debug",	no_argument,		&do_debug,	1	},
		{ "nolog",	no_argument,		&do_nolog,	1	},
		{ "runasroot",	no_argument,		&do_root,	1	},
		{ "version",	no_argument,		&do_version,	1	},
		{ 0, 0, 0, 0 }
	};

	while ((c = getopt_long_only(argc, argv, ":f:", longopts, NULL)) != -1)
	{
		switch (c)
		{
			case 'f':
				/* Log filename was set */
				strlcpy(LogFileName, optarg, MAXBUF);
			break;
			case 'c':
				/* Config filename was set */
				strlcpy(ConfigFileName, optarg, MAXBUF);
			break;
			case 0:
				/* getopt_long_only() set an int variable, just keep going */
			break;
			default:
				/* Unknown parameter! DANGER, INTRUDER.... err.... yeah. */
				printf("Usage: %s [--nofork] [--nolog] [--debug] [--logfile <filename>] [--runasroot] [--version] [--config <config>]\n", argv[0]);
				Exit(EXIT_STATUS_ARGV);
			break;
		}
	}

	if (do_version)
	{
		printf("\n%s r%s\n", VERSION, REVISION);
		Exit(EXIT_STATUS_NOERROR);
	}

#ifdef WIN32

	// Handle forking
	if(!do_nofork)
	{
		DWORD ExitCode = WindowsForkStart(this);
		if(ExitCode)
			exit(ExitCode);
	}

	// Set up winsock
	WSADATA wsadata;
	WSAStartup(MAKEWORD(2,0), &wsadata);
	ChangeWindowsSpecificPointers(this);
#endif
	strlcpy(Config->MyExecutable,argv[0],MAXBUF);

	if (!this->OpenLog(argv, argc))
	{
		printf("ERROR: Could not open logfile %s: %s\n\n", Config->logpath.c_str(), strerror(errno));
		Exit(EXIT_STATUS_LOG);
	}

	if (!ServerConfig::FileExists(this->ConfigFileName))
	{
		printf("ERROR: Cannot open config file: %s\nExiting...\n", this->ConfigFileName);
		this->Log(DEFAULT,"Unable to open config file %s", this->ConfigFileName);
		Exit(EXIT_STATUS_CONFIG);
	}

	printf_c("\033[1;32mhottpd - compiled %s at %s\n",__DATE__,__TIME__);
	printf_c("(C) hottpd development team.\033[0m\n\n");
	printf_c("Developers:\t\t\033[1;32mw00t, Special\033[0m\n");

	/* Set the finished argument values */
	Config->nofork = do_nofork;
	Config->forcedebug = do_debug;
	Config->writelog = !do_nolog;	
	Config->ClearStack();

	this->SetSignals();

	if (!Config->nofork)
	{
		if (!this->DaemonSeed())
		{
			printf("ERROR: could not go into daemon mode. Shutting down.\n");
			Log(DEFAULT,"ERROR: could not go into daemon mode. Shutting down.");
			Exit(EXIT_STATUS_FORK);
		}
	}

	SE->RecoverFromFork();

	Config->Read(true);

	int bounditems = BindPorts(true, found_ports, pl);

	printf("\n");

	if ((Config->ports.size() == 0) && (found_ports > 0))
	{
		printf("\nERROR: I couldn't bind any ports! Are you sure you didn't start hottpd twice?\n");
		Log(DEFAULT,"ERROR: I couldn't bind any ports! Are you sure you didn't start hottpd twice?");
		Exit(EXIT_STATUS_BIND);
	}

	if (Config->ports.size() != (unsigned int)found_ports)
	{
		printf("\nWARNING: Not all your client ports could be bound --\nstarting anyway with %d of %d client ports bound.\n\n", bounditems, found_ports);
		printf("The following port(s) failed to bind:\n");
		int j = 1;
		for (FailedPortList::iterator i = pl.begin(); i != pl.end(); i++, j++)
		{
			printf("%d.\tIP: %s\tPort: %lu\n", j, i->first.empty() ? "<all>" : i->first.c_str(), (unsigned long)i->second);
		}
	}
#ifndef WINDOWS
	if (!Config->nofork)
	{
		if (kill(getppid(), SIGTERM) == -1)
		{
			printf("Error killing parent process: %s\n",strerror(errno));
			Log(DEFAULT,"Error killing parent process: %s",strerror(errno));
		}
	}

	if (isatty(0) && isatty(1) && isatty(2))
	{
		/* We didn't start from a TTY, we must have started from a background process -
		 * e.g. we are restarting, or being launched by cron. Dont kill parent, and dont
		 * close stdin/stdout
		 */
		if (!do_nofork)
		{
			fclose(stdin);
			fclose(stderr);
			fclose(stdout);
		}
		else
		{
			Log(DEFAULT,"Keeping pseudo-tty open as we are running in the foreground.");
		}
	}


	if (*(this->Config->ChRoot))
	{
		// chroot
		int ret = chroot(this->Config->ChRoot);

		if (ret == -1)
		{
			this->Log(DEFAULT, "chroot() failed (bad path?): %s", strerror(errno));
			this->QuickExit(0);
		}
	}

	if (*(this->Config->SetUser))
	{
		// setuid
		struct passwd *u;

		errno = 0;
		u = getpwnam(this->Config->SetUser);

		if (!u)
		{
			this->Log(DEFAULT, "getpwnam() failed (bad user?): %s", strerror(errno));
			this->QuickExit(0);
		}

		int ret = setuid(u->pw_uid);

		if (ret == -1)
		{
			this->Log(DEFAULT, "setuid() failed (bad user?): %s", strerror(errno));
			this->QuickExit(0);
		}
	}

	if (*(this->Config->SetGroup))
	{
		// setgid
		struct group *g;

		errno = 0;
		g = getgrnam(this->Config->SetGroup);

		if (!g)
		{
			this->Log(DEFAULT, "getgrnam() failed (bad user?): %s", strerror(errno));
			this->QuickExit(0);
		}

		int ret = setgid(g->gr_gid);

		if (ret == -1)
		{
			this->Log(DEFAULT, "setgid() failed (bad user?): %s", strerror(errno));
			this->QuickExit(0);
		}
	}

#else
	WindowsIPC = new IPC(this);
	if(!Config->nofork)
	{
		WindowsForkKillOwner(this);
		FreeConsole();
	}
#endif
	
	printf("\nhottpd is now running!\n");
	Log(DEFAULT,"Startup complete.");

	this->WritePID(Config->PID);
}

int InspIRCd::Run()
{
	int times = 1;

	while (true)
	{
#ifndef WIN32
//		static rusage ru;
#else
		static time_t uptime;
		static struct tm * stime;
		static char window_title[100];
#endif

		/* time() seems to be a pretty expensive syscall, so avoid calling it too much.
		 * Once per loop iteration is pleanty.
		 */
		OLDTIME = TIME;
		TIME = time(NULL);

		times++;

		if (times > this->Config->TimeoutCullFrequency)
		{
			this->Log(DEBUG, "culling old sockets");
			times = 1;

			for (std::vector<Connection*>::const_iterator i = this->local_connections.begin(); i != this->local_connections.end(); i++)
			{
				Connection *c = (*i);

				if (TIME >= (c->age + this->Config->TimeoutTotalLifetime))
				{
					/*
					 * Socket's old. Kill the fuck out of it.
					 * We do this to avoid DDOS, and because if we *don't*,
					 * sockets may end up randomly existing for a very long fucking time.
					 *
					 * XXX: if a socket is old, but still writing, we should let it live.
					 */
					this->Log(DEBUG, "Culling %d because it's too old", c->GetFd());
					this->Connections->Delete(c);
				}
				else if (TIME >= (c->LastSocketEvent + this->Config->TimeoutIdleLifetime))
				{
					/*
					 * Socket hasn't been doing anything for quite a while.
					 * Kill the fuck out of it.
					 */
					this->Log(DEBUG, "Culling %d because it's too idle", c->GetFd());
					this->Connections->Delete(c);
				}
			}
		}

		/* Run background module timers every few seconds
		 * (the docs say modules shouldnt rely on accurate
		 * timing using this event, so we dont have to
		 * time this exactly).
		 */
		if (TIME != OLDTIME)
		{
			/* if any connections were quit, take them out */
			this->GlobalCulls.Apply();

			if (ShuttingDown && this->local_connections.size() == 0)
				this->Exit(0);

			if ((TIME % 3600) == 0)
			{
				FOREACH_MOD_I(this, I_OnGarbageCollect, OnGarbageCollect());
			}

			Timers->TickTimers(TIME);

			if ((TIME % 5) == 0)
			{
				FOREACH_MOD_I(this,I_OnBackgroundTimer,OnBackgroundTimer(TIME));
			}
#ifdef WIN32
			WindowsIPC->Check();
	
			if(Config->nofork)
			{
				uptime = Time() - startup_time;
				stime = gmtime(&uptime);
// XXX this needs fixing
//				snprintf(window_title, 100, "InspIRCd - %u clients, %u accepted connections - Up %u days, %.2u:%.2u:%.2u",
//					LocalUserCount(), stats->statsAccept, stime->tm_yday, stime->tm_hour, stime->tm_min, stime->tm_sec);
//				SetConsoleTitle(window_title);
			}
#endif
		}

		/* Call the socket engine to wait on the active
		 * file descriptors. The socket engine has everything's
		 * descriptors in its list... dns, modules, users,
		 * servers... so its nice and easy, just one call.
		 * This will cause any read or write events to be
		 * dispatched to their handlers.
		 */
		this->SE->DispatchEvents();

		if (this->s_signal)
		{
			this->SignalHandler(s_signal);
			this->s_signal = 0;
		}
	}

	return 0;
}

/**********************************************************************************/

/**
 * An ircd in five lines! bwahahaha. ahahahahaha. ahahah *cough*.
 */

int main(int argc, char ** argv)
{
	SI = new InspIRCd(argc, argv);
	mysig = &SI->s_signal;
	SI->Run();
	delete SI;
	return 0;
}

time_t InspIRCd::Time()
{
	return TIME;
}

void InspIRCd::SetSignal(int signal)
{
	*mysig = signal;
}
