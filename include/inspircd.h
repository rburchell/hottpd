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

#ifndef __INSPIRCD_H__
#define __INSPIRCD_H__

#ifndef WIN32
#define DllExport 
#define CoreExport 
#define printf_c printf
#else
#include "inspircd_win32wrapper.h"
/** Windows defines these already */
#undef DELETE
#undef ERROR
#endif

#include <time.h>
#include <string>
#include <sstream>
#include <list>
#include "inspircd_config.h"
#include "connections.h"
#include "socket.h"
#include "socketengine.h"
#include "cull_list.h"
#include "filelogger.h"
#include "timer.h"
#include "modules.h"
#include "configreader.h"
#include "mimetypes.h"
#include "connectionmanager.h"

class FOpenBackend; // XXX shitty required forward dec, remove when fopen moves out of backend.h

/**
 * Used to define the maximum number of parameters a command may have.
 */
#define MAXPARAMETERS 127

/** Returned by some functions to indicate failure.
 */
#define ERROR -1

/** Support for librodent -
 * see http://www.chatspike.net/index.php?z=64
 */
#define ETIREDHAMSTERS EAGAIN

/** Template function to convert any input type to std::string
 */
template<typename T> inline std::string ConvNumeric(const T &in)
{
	if (in == 0) return "0";
	char res[MAXBUF];
	char* out = res;
	T quotient = in;
	while (quotient) {
		*out = "0123456789"[ std::abs( (long)quotient % 10 ) ];
		++out;
		quotient /= 10;
	}
	if (in < 0)
		*out++ = '-';
	*out = 0;
	std::reverse(res,out);
	return res;
}

/** Template function to convert any input type to std::string
 */
inline std::string ConvToStr(const int in)
{
	return ConvNumeric(in);
}

/** Template function to convert any input type to std::string
 */
inline std::string ConvToStr(const long in)
{
	return ConvNumeric(in);
}

/** Template function to convert any input type to std::string
 */
inline std::string ConvToStr(const char* in)
{
	return in;
}

/** Template function to convert any input type to std::string
 */
inline std::string ConvToStr(const bool in)
{
	return (in ? "1" : "0");
}

/** Template function to convert any input type to std::string
 */
inline std::string ConvToStr(char in)
{
	return std::string(in,1);
}

/** Template function to convert any input type to std::string
 */
template <class T> inline std::string ConvToStr(const T &in)
{
	std::stringstream tmp;
	if (!(tmp << in)) return std::string();
	return tmp.str();
}

/** Template function to convert any input type to any other type
 * (usually an integer or numeric type)
 */
template<typename T> inline long ConvToInt(const T &in)
{
	std::stringstream tmp;
	if (!(tmp << in)) return 0;
	return atoi(tmp.str().c_str());
}

/** Template function to convert integer to char, storing result in *res and
 * also returning the pointer to res. Based on Stuart Lowe's C/C++ Pages.
 * @param T input value
 * @param V result value
 * @param R base to convert to
 */
template<typename T, typename V, typename R> inline char* itoa(const T &in, V *res, R base)
{
	if (base < 2 || base > 16) { *res = 0; return res; }
	char* out = res;
	int quotient = in;
	while (quotient) {
		*out = "0123456789abcdef"[ std::abs( quotient % base ) ];
		++out;
		quotient /= base;
	}
	if ( in < 0 && base == 10) *out++ = '-';
	std::reverse( res, out );
	*out = 0;
	return res;
}

/** A list of failed port bindings, used for informational purposes on startup */
typedef std::vector<std::pair<std::string, long> > FailedPortList;

/** The main class of the irc server.
 * This class contains instances of all the other classes
 * in this software, with the exception of the base class,
 * classbase. Amongst other things, it contains a ModeParser,
 * a DNS object, a CommandParser object, and a list of active
 * Module objects, and facilities for Module objects to
 * interact with the core system it implements. You should
 * NEVER attempt to instantiate a class of type InspIRCd
 * yourself. If you do, this is equivalent to spawning a second
 * IRC server, and could have catastrophic consequences for the
 * program in terms of ram usage (basically, you could create
 * an obese forkbomb built from recursively spawning irc servers!)
 */
class CoreExport InspIRCd : public classbase
{
 private:
	/** Set up the signal handlers
	 */
	void SetSignals();

	/** Daemonize the ircd and close standard input/output streams
	 * @return True if the program daemonized succesfully
	 */
	bool DaemonSeed();

	/** Returns true when all modules have done pre-registration checks on a connection
	 * @param connection The connection to verify
	 * @return True if all modules have finished checking this connection
	 */
	bool AllModulesReportReady(Connection* connection);

	/** Logfile pathname specified on the commandline, or empty string
	 */
	char LogFileName[MAXBUF];

	/** The current time, updated in the mainloop
	 */
	time_t TIME;

	/** The time that was recorded last time around the mainloop
	 */
	time_t OLDTIME;

	/** A 64k buffer used to read client lines into
	 */
	char ReadBuffer[65535];

	/** Used when connecting clients
	 */
	socklen_t length;

	/** Nonblocking file writer
	 */
	FileLogger* Logger;

	/** Time offset in seconds
	 * This offset is added to all calls to Time(). Use SetTimeDelta() to update
	 */
	int time_delta;

#ifdef WIN32
	IPC* WindowsIPC;
#endif

 public:
	ConnectionManager *Connections;

	FOpenBackend *FOpen;

	MimeManager *MimeTypes;

	/** Global cull list, will be processed on next iteration
	 */
	CullList GlobalCulls;

	Connection *FindDescriptorHandler(int);

	/** Time this ircd was booted
	 */
	time_t startup_time;

	/** Config file pathname specified on the commandline or via ./configure
	 */
	char ConfigFileName[MAXBUF];

	/** Socket engine, handles socket activity events
	 */
	SocketEngine* SE;
	
	/** ModuleManager contains everything related to loading/unloading
	 * modules.
	 */
	ModuleManager* Modules;

	/**  Server Config class, holds configuration file data
	 */
	ServerConfig* Config;

	/** Local client list, a vector containing only local clients
	 */
	std::vector<Connection*> local_connections;

	/** Timer manager class, triggers Timer timer events
	 */
	TimerManager* Timers;

	/** Set to the current signal recieved
	 */
	int s_signal;

	/** Get the current time
	 * Because this only calls time() once every time around the mainloop,
	 * it is much faster than calling time() directly.
	 * @return The current time as an epoch value (time_t)
	 */
	time_t Time();

	/** Add a connection to the local clone map
	 * @param connection The connection to add
	 */
	void AddLocalClone(Connection *c);

	/** Bind all ports specified in the configuration file.
	 * @param bail True if the function should bail back to the shell on failure
	 * @param found_ports The actual number of ports found in the config, as opposed to the number actually bound
	 * @return The number of ports actually bound without error
	 */
	int BindPorts(bool bail, int &found_ports, FailedPortList &failed_ports);

	/** Binds a socket on an already open file descriptor
	 * @param sockfd A valid file descriptor of an open socket
	 * @param port The port number to bind to
	 * @param addr The address to bind to (IP only)
	 * @return True if the port was bound successfully
	 */
	bool BindSocket(int sockfd, int port, char* addr, bool dolisten = true);

	/** Determine the right path for, and open, the logfile
	 * @param argv The argv passed to main() initially, used to calculate program path
	 * @param argc The argc passed to main() initially, used to calculate program path
	 * @return True if the log could be opened, false if otherwise
	 */
	bool OpenLog(char** argv, int argc);

	/** Close the currently open log file
	 */
	void CloseLog();

	/** Rehash the local server
	 */
	void Rehash();

	/** Handles incoming signals after being set
	 * @param signal the signal recieved
	 */
	void SignalHandler(int signal);

	/** Sets the signal recieved	
	 * @param signal the signal recieved
	 */
	static void SetSignal(int signal);

	/** Causes the server to exit after unloading modules and
	 * closing all open file descriptors.
	 *
	 * @param The exit code to give to the operating system
	 * (See the ExitStatus enum for valid values)
	 */
	void Exit(int status);

	/** Causes the server to exit immediately with exit code 0.
	 * The status code is required for signal handlers, and ignored.
	 */
	static void QuickExit(int status);

	/** Match two strings using pattern matching.
	 * This operates identically to the global function match(),
	 * except for that it takes std::string arguments rather than
	 * const char* ones.
	 * @param sliteral The literal string to match against
	 * @param spattern The pattern to match against. CIDR and globs are supported.
	 */
	bool MatchText(const std::string &sliteral, const std::string &spattern);

	/** Calculate a duration in seconds from a string in the form 1y2w3d4h6m5s
	 * @param str A string containing a time in the form 1y2w3d4h6m5s
	 * (one year, two weeks, three days, four hours, six minutes and five seconds)
	 * @return The total number of seconds
	 */
	long Duration(const std::string &str);

	/** Returns the subversion revision ID of this ircd
	 * @return The revision ID or an empty string
	 */
	std::string GetRevision();

	/** Returns the full version string of this ircd
	 * @return The version string
	 */
	std::string GetVersionString();

	/** Attempt to write the process id to a given file
	 * @param filename The PID file to attempt to write to
	 * @return This function may bail if the file cannot be written
	 */
	void WritePID(const std::string &filename);

	/** This constructor initialises all the subsystems and reads the config file.
	 * @param argc The argument count passed to main()
	 * @param argv The argument list passed to main()
	 * @throw <anything> If anything is thrown from here and makes it to
	 * you, you should probably just give up and go home. Yes, really.
	 * It's that bad. Higher level classes should catch any non-fatal exceptions.
	 */
	InspIRCd(int argc, char** argv);

	/** Output a log message to the ircd.log file
	 * The text will only be output if the current loglevel
	 * is less than or equal to the level you provide
	 * @param level A log level from the DebugLevel enum
	 * @param text Format string of to write to the log
	 * @param ... Format arguments of text to write to the log
	 */
	void Log(int level, const char* text, ...);

	/** Output a log message to the ircd.log file
	 * The text will only be output if the current loglevel
	 * is less than or equal to the level you provide
	 * @param level A log level from the DebugLevel enum
	 * @param text Text to write to the log
	 */
	void Log(int level, const std::string &text);

	/** Restart the server.
	 * This function will not return. If an error occurs,
	 * it will throw an instance of CoreException.
	 * @param reason The restart reason to show to all clients
	 * @throw CoreException An instance of CoreException indicating the error from execv().
	 */
	void Restart(const std::string &reason);

	/** Prepare the ircd for restart or shutdown.
	 * This function unloads all modules which can be unloaded,
	 * closes all open sockets, and closes the logfile.
	 */
	void Cleanup();

	/** Return a time_t as a human-readable string.
	 */
	std::string TimeString(time_t curtime);

	/** Begin execution of the server.
	 * NOTE: this function NEVER returns. Internally,
	 * it will repeatedly loop.
	 * @return The return value for this function is undefined.
	 */
	int Run();

	char* GetReadBuffer()
	{
		return this->ReadBuffer;
	}
};


#include "backend.h" // XXX move this to the right place when fopen is moved out of it
#endif
