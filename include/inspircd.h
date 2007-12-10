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
#include "uid.h"
#include "users.h"
#include "channels.h"
#include "socket.h"
#include "mode.h"
#include "socketengine.h"
#include "command_parse.h"
#include "snomasks.h"
#include "cull_list.h"
#include "filelogger.h"
#include "caller.h"

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

/** This class contains various STATS counters
 * It is used by the InspIRCd class, which internally
 * has an instance of it.
 */
class serverstats : public classbase
{
  public:
	/** Number of accepted connections
	 */
	unsigned long statsAccept;
	/** Number of failed accepts
	 */
	unsigned long statsRefused;
	/** Number of unknown commands seen
	 */
	unsigned long statsUnknown;
	/** Number of nickname collisions handled
	 */
	unsigned long statsCollisions;
	/** Number of DNS queries sent out
	 */
	unsigned long statsDns;
	/** Number of good DNS replies received
	 * NOTE: This may not tally to the number sent out,
	 * due to timeouts and other latency issues.
	 */
	unsigned long statsDnsGood;
	/** Number of bad (negative) DNS replies received
	 * NOTE: This may not tally to the number sent out,
	 * due to timeouts and other latency issues.
	 */
	unsigned long statsDnsBad;
	/** Number of inbound connections seen
	 */
	unsigned long statsConnects;
	/** Total bytes of data transmitted
	 */
	double statsSent;
	/** Total bytes of data received
	 */
	double statsRecv;
	/** Cpu usage at last sample
	 */
	timeval LastCPU;
	/** Time last sample was read
	 */
	timeval LastSampled;
	/** The constructor initializes all the counts to zero
	 */
	serverstats()
		: statsAccept(0), statsRefused(0), statsUnknown(0), statsCollisions(0), statsDns(0),
		statsDnsGood(0), statsDnsBad(0), statsConnects(0), statsSent(0.0), statsRecv(0.0)
	{
	}
};

/** A list of failed port bindings, used for informational purposes on startup */
typedef std::vector<std::pair<std::string, long> > FailedPortList;

/** A list of ip addresses cross referenced against clone counts */
typedef std::map<irc::string, unsigned int> clonemap;

class InspIRCd;

DEFINE_HANDLER1(ProcessUserHandler, void, User*);
DEFINE_HANDLER1(IsNickHandler, bool, const char*);
DEFINE_HANDLER1(IsIdentHandler, bool, const char*);
DEFINE_HANDLER1(FindDescriptorHandler, User*, int);
DEFINE_HANDLER1(FloodQuitUserHandler, void, User*);

/* Forward declaration - required */
class XLineManager;
class BanCacheManager;

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
	/** Holds the current UID. Used to generate the next one.
	 */
	char current_uid[UUID_LENGTH];

	/** Set up the signal handlers
	 */
	void SetSignals();

	/** Daemonize the ircd and close standard input/output streams
	 * @return True if the program daemonized succesfully
	 */
	bool DaemonSeed();

	/** Iterate the list of BufferedSocket objects, removing ones which have timed out
	 * @param TIME the current time
	 */
	void DoSocketTimeouts(time_t TIME);

	/** Sets up UID subsystem
	 */
	void InitialiseUID();

	/** Perform background user events such as PING checks
	 */
	void DoBackgroundUserStuff();

	/** Returns true when all modules have done pre-registration checks on a user
	 * @param user The user to verify
	 * @return True if all modules have finished checking this user
	 */
	bool AllModulesReportReady(User* user);

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
	insp_sockaddr client, server;

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

	/** Global cull list, will be processed on next iteration
	 */
	CullList GlobalCulls;

	/**** Functors ****/

	ProcessUserHandler HandleProcessUser;
	IsNickHandler HandleIsNick;
	IsIdentHandler HandleIsIdent;
	FindDescriptorHandler HandleFindDescriptor;
	FloodQuitUserHandler HandleFloodQuitUser;

	/** BufferedSocket classes pending deletion after being closed.
	 * We don't delete these immediately as this may cause a segmentation fault.
	 */
	std::map<BufferedSocket*,BufferedSocket*> SocketCull;

	/** Globally accessible fake user record. This is used to force mode changes etc across s2s, etc.. bit ugly, but.. better than how this was done in 1.1
	 * Reason for it:
	 * kludge alert!
	 * SendMode expects a User* to send the numeric replies
	 * back to, so we create it a fake user that isnt in the user
	 * hash and set its descriptor to FD_MAGIC_NUMBER so the data
	 * falls into the abyss :p
	 */
	User *FakeClient;

	/** Returns the next available UID for this server.
	 */
	std::string GetUID();

	/** Find a user in the UUID hash
	 * @param nick The nickname to find
	 * @return A pointer to the user, or NULL if the user does not exist
	 */
	User *FindUUID(const std::string &);

	/** Find a user in the UUID hash
	 * @param nick The nickname to find
	 * @return A pointer to the user, or NULL if the user does not exist
	 */
	User *FindUUID(const char *);

	/** Build the ISUPPORT string by triggering all modules On005Numeric events
	 */
	void BuildISupport();

	/** Number of unregistered users online right now.
	 * (Unregistered means before USER/NICK/dns)
	 */
	int unregistered_count;

	/** List of server names we've seen.
	 */
	servernamelist servernames;

	/** Time this ircd was booted
	 */
	time_t startup_time;

	/** Config file pathname specified on the commandline or via ./configure
	 */
	char ConfigFileName[MAXBUF];

	/** Mode handler, handles mode setting and removal
	 */
	ModeParser* Modes;

	/** Command parser, handles client to server commands
	 */
	CommandParser* Parser;

	/** Socket engine, handles socket activity events
	 */
	SocketEngine* SE;
	
	/** ModuleManager contains everything related to loading/unloading
	 * modules.
	 */
	ModuleManager* Modules;

	/** BanCacheManager is used to speed up checking of restrictions on connection
	 * to the IRCd.
	 */
	BanCacheManager *BanCache;

	/** Stats class, holds miscellaneous stats counters
	 */
	serverstats* stats;

	/**  Server Config class, holds configuration file data
	 */
	ServerConfig* Config;

	/** Snomask manager - handles routing of snomask messages
	 * to opers.
	 */
	SnomaskManager* SNO;

	/** Client list, a hash_map containing all clients, local and remote
	 */
	user_hash* clientlist;

	/** Client list stored by UUID. Contains all clients, and is updated
	 * automatically by the constructor and destructor of User.
	 */
	user_hash* uuidlist;

	/** Channel list, a hash_map containing all channels
	 */
	chan_hash* chanlist;

	/** Local client list, a vector containing only local clients
	 */
	std::vector<User*> local_users;

	/** Oper list, a vector containing all local and remote opered users
	 */
	std::list<User*> all_opers;

	/** Map of local ip addresses for clone counting
	 */
	clonemap local_clones;

	/** Map of global ip addresses for clone counting
	 */
	clonemap global_clones;

	/** DNS class, provides resolver facilities to the core and modules
	 */
	DNS* Res;

	/** Timer manager class, triggers Timer timer events
	 */
	TimerManager* Timers;

	/** X-Line manager. Handles G/K/Q/E line setting, removal and matching
	 */
	XLineManager* XLines;

	/** Set to the current signal recieved
	 */
	int s_signal;

	/** Get the current time
	 * Because this only calls time() once every time around the mainloop,
	 * it is much faster than calling time() directly.
	 * @param delta True to use the delta as an offset, false otherwise
	 * @return The current time as an epoch value (time_t)
	 */
	time_t Time(bool delta = false);

	/** Set the time offset in seconds
	 * This offset is added to Time() to offset the system time by the specified
	 * number of seconds.
	 * @param delta The number of seconds to offset
	 * @return The old time delta
	 */
	int SetTimeDelta(int delta);

	/** Add a user to the local clone map
	 * @param user The user to add
	 */
	void AddLocalClone(User* user);

	/** Add a user to the global clone map
	 * @param user The user to add
	 */
	void AddGlobalClone(User* user);
	
	/** Number of users with a certain mode set on them
	 */
	int ModeCount(const char mode);

	/** Get the time offset in seconds
	 * @return The current time delta (in seconds)
	 */
	int GetTimeDelta();

	/** Process a user whos socket has been flagged as active
	 * @param cu The user to process
	 * @return There is no actual return value, however upon exit, the user 'cu' may have been
	 * marked for deletion in the global CullList.
	 */
	caller1<void, User*> ProcessUser;

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

	/** Adds a server name to the list of servers we've seen
	 * @param The servername to add
	 */
	void AddServerName(const std::string &servername);

	/** Finds a cached char* pointer of a server name,
	 * This is used to optimize User by storing only the pointer to the name
	 * @param The servername to find
	 * @return A pointer to this name, gauranteed to never become invalid
	 */
	const char* FindServerNamePtr(const std::string &servername);

	/** Returns true if we've seen the given server name before
	 * @param The servername to find
	 * @return True if we've seen this server name before
	 */
	bool FindServerName(const std::string &servername);

	/** Gets the GECOS (description) field of the given server.
	 * If the servername is not that of the local server, the name
	 * is passed to handling modules which will attempt to determine
	 * the GECOS that bleongs to the given servername.
	 * @param servername The servername to find the description of
	 * @return The description of this server, or of the local server
	 */
	std::string GetServerDescription(const char* servername);

	/** Write text to all opers connected to this server
	 * @param text The text format string
	 * @param ... Format args
	 */
	void WriteOpers(const char* text, ...);

	/** Write text to all opers connected to this server
	 * @param text The text to send
	 */
	void WriteOpers(const std::string &text);

	/** Find a user in the nick hash.
	 * If the user cant be found in the nick hash check the uuid hash
	 * @param nick The nickname to find
	 * @return A pointer to the user, or NULL if the user does not exist
	 */
	User* FindNick(const std::string &nick);

	/** Find a user in the nick hash.
	 * If the user cant be found in the nick hash check the uuid hash
	 * @param nick The nickname to find
	 * @return A pointer to the user, or NULL if the user does not exist
	 */
	User* FindNick(const char* nick);

	/** Find a user in the nick hash ONLY
	 */
	User* FindNickOnly(const char* nick);

	/** Find a user in the nick hash ONLY
	 */
	User* FindNickOnly(const std::string &nick);

	/** Find a channel in the channels hash
	 * @param chan The channel to find
	 * @return A pointer to the channel, or NULL if the channel does not exist
	 */
	Channel* FindChan(const std::string &chan);

	/** Find a channel in the channels hash
	 * @param chan The channel to find
	 * @return A pointer to the channel, or NULL if the channel does not exist
	 */
	Channel* FindChan(const char* chan);

	/** Check for a 'die' tag in the config file, and abort if found
	 * @return Depending on the configuration, this function may never return
	 */
	void CheckDie();

	/** Check we aren't running as root, and exit if we are
	 * @return Depending on the configuration, this function may never return
	 */
	void CheckRoot();

	/** Determine the right path for, and open, the logfile
	 * @param argv The argv passed to main() initially, used to calculate program path
	 * @param argc The argc passed to main() initially, used to calculate program path
	 * @return True if the log could be opened, false if otherwise
	 */
	bool OpenLog(char** argv, int argc);

	/** Close the currently open log file
	 */
	void CloseLog();

	/** Send a server notice to all local users
	 * @param text The text format string to send
	 * @param ... The format arguments
	 */
	void ServerNoticeAll(char* text, ...);

	/** Send a server message (PRIVMSG) to all local users
	 * @param text The text format string to send
	 * @param ... The format arguments
	 */
	void ServerPrivmsgAll(char* text, ...);

	/** Send text to all users with a specific set of modes
	 * @param modes The modes to check against, without a +, e.g. 'og'
	 * @param flags one of WM_OR or WM_AND. If you specify WM_OR, any one of the
	 * mode characters in the first parameter causes receipt of the message, and
	 * if you specify WM_OR, all the modes must be present.
	 * @param text The text format string to send
	 * @param ... The format arguments
	 */
	void WriteMode(const char* modes, int flags, const char* text, ...);

	/** Return true if a channel name is valid
	 * @param chname A channel name to verify
	 * @return True if the name is valid
	 */
	bool IsChannel(const char *chname);

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

	/** Return a count of users, unknown and known connections
	 * @return The number of users
	 */
	int UserCount();

	/** Return a count of fully registered connections only
	 * @return The number of registered users
	 */
	int RegisteredUserCount();

	/** Return a count of opered (umode +o) users only
	 * @return The number of opers
	 */
	int OperCount();

	/** Return a count of unregistered (before NICK/USER) users only
	 * @return The number of unregistered (unknown) connections
	 */
	int UnregisteredUserCount();

	/** Return a count of channels on the network
	 * @return The number of channels
	 */
	long ChannelCount();

	/** Return a count of local users on this server only
	 * @return The number of local users
	 */
	long LocalUserCount();

	/** Send an error notice to all local users, opered and unopered
	 * @param s The error string to send
	 */
	void SendError(const std::string &s);

	/** Return true if a nickname is valid
	 * @param n A nickname to verify
	 * @return True if the nick is valid
	 */
	caller1<bool, const char*> IsNick;

	/** Return true if an ident is valid
	 * @param An ident to verify
	 * @return True if the ident is valid
	 */
	caller1<bool, const char*> IsIdent;

	/** Find a username by their file descriptor.
	 * It is preferred to use this over directly accessing the fd_ref_table array.
	 * @param socket The file descriptor of a user
	 * @return A pointer to the user if the user exists locally on this descriptor
	 */
	caller1<User*, int> FindDescriptor;

	/** Add a new mode to this server's mode parser
	 * @param mh The modehandler to add
	 * @return True if the mode handler was added
	 */
	bool AddMode(ModeHandler* mh);

	/** Add a new mode watcher to this server's mode parser
	 * @param mw The modewatcher to add
	 * @return True if the modewatcher was added
	 */
	bool AddModeWatcher(ModeWatcher* mw);

	/** Delete a mode watcher from this server's mode parser
	 * @param mw The modewatcher to delete
	 * @return True if the modewatcher was deleted
	 */
	bool DelModeWatcher(ModeWatcher* mw);

	/** Add a dns Resolver class to this server's active set
	 * @param r The resolver to add
	 * @param cached If this value is true, then the cache will
	 * be searched for the DNS result, immediately. If the value is
	 * false, then a request will be sent to the nameserver, and the
	 * result will not be immediately available. You should usually
	 * use the boolean value which you passed to the Resolver
	 * constructor, which Resolver will set appropriately depending
	 * on if cached results are available and haven't expired. It is
	 * however safe to force this value to false, forcing a remote DNS
	 * lookup, but not an update of the cache.
	 * @return True if the operation completed successfully. Note that
	 * if this method returns true, you should not attempt to access
	 * the resolver class you pass it after this call, as depending upon
	 * the request given, the object may be deleted!
	 */
	bool AddResolver(Resolver* r, bool cached);

	/** Add a command to this server's command parser
	 * @param f A Command command handler object to add
	 * @throw ModuleException Will throw ModuleExcption if the command already exists
	 */
	void AddCommand(Command *f);

	/** Send a modechange.
	 * The parameters provided are identical to that sent to the
	 * handler for class cmd_mode.
	 * @param parameters The mode parameters
	 * @param pcnt The number of items you have given in the first parameter
	 * @param user The user to send error messages to
	 */
	void SendMode(const char **parameters, int pcnt, User *user);

	/** Match two strings using pattern matching.
	 * This operates identically to the global function match(),
	 * except for that it takes std::string arguments rather than
	 * const char* ones.
	 * @param sliteral The literal string to match against
	 * @param spattern The pattern to match against. CIDR and globs are supported.
	 */
	bool MatchText(const std::string &sliteral, const std::string &spattern);

	/** Call the handler for a given command.
	 * @param commandname The command whos handler you wish to call
	 * @param parameters The mode parameters
	 * @param pcnt The number of items you have given in the first parameter
	 * @param user The user to execute the command as
	 * @return True if the command handler was called successfully
	 */
	CmdResult CallCommandHandler(const std::string &commandname, const char** parameters, int pcnt, User* user);

	/** Return true if the command is a module-implemented command and the given parameters are valid for it
	 * @param parameters The mode parameters
	 * @param pcnt The number of items you have given in the first parameter
	 * @param user The user to test-execute the command as
	 * @return True if the command handler is a module command, and there are enough parameters and the user has permission to the command
	 */
	bool IsValidModuleCommand(const std::string &commandname, int pcnt, User* user);

	/** Return true if the given parameter is a valid nick!user\@host mask
	 * @param mask A nick!user\@host masak to match against
	 * @return True i the mask is valid
	 */
	bool IsValidMask(const std::string &mask);

	/** Rehash the local server
	 */
	void RehashServer();

	/** Return the channel whos index number matches that provided
	 * @param The index number of the channel to fetch
	 * @return A channel record, or NUll if index < 0 or index >= InspIRCd::ChannelCount()
	 */
	Channel* GetChannelIndex(long index);

	/** Dump text to a user target, splitting it appropriately to fit
	 * @param User the user to dump the text to
	 * @param LinePrefix text to prefix each complete line with
	 * @param TextStream the text to send to the user
	 */
	void DumpText(User* User, const std::string &LinePrefix, stringstream &TextStream);

	/** Check if the given nickmask matches too many users, send errors to the given user
	 * @param nick A nickmask to match against
	 * @param user A user to send error text to
	 * @return True if the nick matches too many users
	 */
	bool NickMatchesEveryone(const std::string &nick, User* user);

	/** Check if the given IP mask matches too many users, send errors to the given user
	 * @param ip An ipmask to match against
	 * @param user A user to send error text to
	 * @return True if the ip matches too many users
	 */
	bool IPMatchesEveryone(const std::string &ip, User* user);

	/** Check if the given hostmask matches too many users, send errors to the given user
	 * @param mask A hostmask to match against
	 * @param user A user to send error text to
	 * @return True if the host matches too many users
	 */
	bool HostMatchesEveryone(const std::string &mask, User* user);

	/** Calculate a duration in seconds from a string in the form 1y2w3d4h6m5s
	 * @param str A string containing a time in the form 1y2w3d4h6m5s
	 * (one year, two weeks, three days, four hours, six minutes and five seconds)
	 * @return The total number of seconds
	 */
	long Duration(const std::string &str);

	/** Attempt to compare an oper password to a string from the config file.
	 * This will be passed to handling modules which will compare the data
	 * against possible hashed equivalents in the input string.
	 * @param data The data from the config file
	 * @param input The data input by the oper
	 * @param tagnum the tag number of the oper's tag in the config file
	 * @return 0 if the strings match, 1 or -1 if they do not
	 */
	int OperPassCompare(const char* data,const char* input, int tagnum);

	/** Check if a given server is a uline.
	 * An empty string returns true, this is by design.
	 * @param server The server to check for uline status
	 * @return True if the server is a uline OR the string is empty
	 */
	bool ULine(const char* server);

	/** Returns true if the uline is 'silent' (doesnt generate
	 * remote connect notices etc).
	 */
	bool SilentULine(const char* server);

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

	/** Send a line of WHOIS data to a user.
	 * @param user user to send the line to
	 * @param dest user being WHOISed
	 * @param numeric Numeric to send
	 * @param text Text of the numeric
	 */
	void SendWhoisLine(User* user, User* dest, int numeric, const std::string &text);

	/** Send a line of WHOIS data to a user.
	 * @param user user to send the line to
	 * @param dest user being WHOISed
	 * @param numeric Numeric to send
	 * @param format Format string for the numeric
	 * @param ... Parameters for the format string
	 */
	void SendWhoisLine(User* user, User* dest, int numeric, const char* format, ...);

	/** Quit a user for excess flood, and if they are not
	 * fully registered yet, temporarily zline their IP.
	 * @param current user to quit
	 */
	caller1<void, User*> FloodQuitUser;

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

	/** This copies the user and channel hash_maps into new hash maps.
	 * This frees memory used by the hash_map allocator (which it neglects
	 * to free, most of the time, using tons of ram)
	 */
	void RehashUsersAndChans();

	/** Resets the cached max bans value on all channels.
	 * Called by rehash.
	 */
	void ResetMaxBans();

	/** Return a time_t as a human-readable string.
	 */
	std::string TimeString(time_t curtime);

	/** Begin execution of the server.
	 * NOTE: this function NEVER returns. Internally,
	 * it will repeatedly loop.
	 * @return The return value for this function is undefined.
	 */
	int Run();

	/** Force all BufferedSockets to be removed which are due to
	 * be culled.
	 */
	void BufferedSocketCull();

	char* GetReadBuffer()
	{
		return this->ReadBuffer;
	}
};

#endif
