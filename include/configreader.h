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

#ifndef INSPIRCD_CONFIGREADER
#define INSPIRCD_CONFIGREADER

/* handy defines */

#include <sstream>
#include <string>
#include <vector>
#include <map>
#include "inspircd.h"
#include "globals.h"
#include "modules.h"
#include "socketengine.h"
#include "socket.h"

/* Required forward definitions */
class ServerConfig;
class InspIRCd;

/** Types of data in the core config
 */
enum ConfigDataType
{
	DT_NOTHING       = 0,		/* No data */
	DT_INTEGER       = 1,		/* Integer */
	DT_CHARPTR       = 2,		/* Char pointer */
	DT_BOOLEAN       = 3,		/* Boolean */
	DT_HOSTNAME	 = 4,		/* Hostname syntax */
	DT_NOSPACES	 = 5,		/* No spaces */
	DT_IPADDRESS	 = 6,		/* IP address (v4, v6) */
	DT_ALLOW_WILD	 = 64,		/* Allow wildcards/CIDR in DT_IPADDRESS */
	DT_ALLOW_NEWLINE = 128		/* New line characters allowed in DT_CHARPTR */
};

/** Holds a config value, either string, integer or boolean.
 * Callback functions receive one or more of these, either on
 * their own as a reference, or in a reference to a deque of them.
 * The callback function can then alter the values of the ValueItem
 * classes to validate the settings.
 */
class ValueItem
{
	/** Actual data */
	std::string v;
 public:
	/** Initialize with an int */
	ValueItem(int value);
	/** Initialize with a bool */
	ValueItem(bool value);
	/** Initialize with a char pointer */
	ValueItem(char* value);
	/** Change value to a char pointer */
	void Set(char* value);
	/** Change value to a const char pointer */
	void Set(const char* val);
	/** Change value to an int */
	void Set(int value);
	/** Get value as an int */
	int GetInteger();
	/** Get value as a string */
	char* GetString();
	/** Get value as a bool */
	bool GetBool();
};

/** The base class of the container 'ValueContainer'
 * used internally by the core to hold core values.
 */
class ValueContainerBase
{
 public:
	/** Constructor */
	ValueContainerBase() { }
	/** Destructor */
	virtual ~ValueContainerBase() { }
};

/** ValueContainer is used to contain pointers to different
 * core values such as the server name, maximum number of
 * clients etc.
 * It is specialized to hold a data type, then pointed at
 * a value in the ServerConfig class. When the value has been
 * read and validated, the Set method is called to write the
 * value safely in a type-safe manner.
 */
template<typename T> class ValueContainer : public ValueContainerBase
{
	/** Contained item */
	T val;
 public:

	/** Initialize with nothing */
	ValueContainer()
	{
		val = NULL;
	}

	/** Initialize with a value of type T */
	ValueContainer(T Val)
	{
		val = Val;
	}

	/** Change value to type T of size s */
	void Set(T newval, size_t s)
	{
		memcpy(val, newval, s);
	}
};

/** A specialization of ValueContainer to hold a pointer to a bool
 */
typedef ValueContainer<bool*> ValueContainerBool;

/** A specialization of ValueContainer to hold a pointer to
 * an unsigned int
 */
typedef ValueContainer<unsigned int*> ValueContainerUInt;

/** A specialization of ValueContainer to hold a pointer to
 * a char array.
 */
typedef ValueContainer<char*> ValueContainerChar;

/** A specialization of ValueContainer to hold a pointer to
 * an int
 */
typedef ValueContainer<int*> ValueContainerInt;

/** A set of ValueItems used by multi-value validator functions
 */
typedef std::deque<ValueItem> ValueList;

/** A callback for validating a single value
 */
typedef bool (*Validator)(ServerConfig* conf, const char*, const char*, ValueItem&);
/** A callback for validating multiple value entries
 */
typedef bool (*MultiValidator)(ServerConfig* conf, const char*, char**, ValueList&, int*);
/** A callback indicating the end of a group of entries
 */
typedef bool (*MultiNotify)(ServerConfig* conf, const char*);

/** Holds a core configuration item and its callbacks
 */
struct InitialConfig
{
	/** Tag name */
	char* tag;
	/** Value name */
	char* value;
	/** Default, if not defined */
	char* default_value;
	/** Value containers */
	ValueContainerBase* val;
	/** Data types */
	ConfigDataType datatype;
	/** Validation function */
	Validator validation_function;
};

/** Holds a core configuration item and its callbacks
 * where there may be more than one item
 */
struct MultiConfig
{
	/** Tag name */
	const char*	tag;
	/** One or more items within tag */
	char*		items[17];
	/** One or more defaults for items within tags */
	char*		items_default[17];
	/** One or more data types */
	int		datatype[17];
	/** Initialization function */
	MultiNotify	init_function;
	/** Validation function */
	MultiValidator	validation_function;
	/** Completion function */
	MultiNotify	finish_function;
};

/** This class holds the bulk of the runtime configuration for the ircd.
 * It allows for reading new config values, accessing configuration files,
 * and storage of the configuration data needed to run the ircd, such as
 * the servername, connect classes, /ADMIN data, MOTDs and filenames etc.
 */
class CoreExport ServerConfig : public Extensible
{
  private:
	/** Creator/owner pointer
	 */
	InspIRCd* ServerInstance;

	/** This variable holds the names of all
	 * files included from the main one. This
	 * is used to make sure that no files are
	 * recursively included.
	 */
	std::vector<std::string> include_stack;

	/** Process an include directive
	 */
	bool DoInclude(ConfigDataHash &target, const std::string &file, std::ostringstream &errorstream);

	/** Check that there is only one of each configuration item
	 */
	bool CheckOnce(char* tag);

 public:

	std::ostringstream errstr;

	ConfigDataHash newconfig;

	InspIRCd* GetInstance();

  	/** This holds all the information in the config file,
	 * it's indexed by tag name to a vector of key/values.
	 */
	ConfigDataHash config_data;

	/** The full path to the modules directory.
	 * This is either set at compile time, or
	 * overridden in the configuration file via
	 * the <options> tag.
	 */
	char ModPath[1024];

	/** The full pathname to the executable, as
	 * given in argv[0] when the program starts.
	 */
	char MyExecutable[1024];

	/** The file handle of the logfile. If this
	 * value is NULL, the log file is not open,
	 * probably due to a permissions error on
	 * startup (this should not happen in normal
	 * operation!).
	 */
	FILE *log_file;

	/** If this value is true, the owner of the
	 * server specified -nofork on the command
	 * line, causing the daemon to stay in the
	 * foreground.
	 */
	bool nofork;
	
	/** If this value if true then all log
	 * messages will be output, regardless of
	 * the level given in the config file.
	 * This is set with the -debug commandline
	 * option.
	 */
	bool forcedebug;
	
	/** If this is true then log output will be
	 * written to the logfile. This is the default.
	 * If you put -nolog on the commandline then
	 * the logfile will not be written.
	 * This is meant to be used in conjunction with
	 * -debug for debugging without filling up the
	 * hard disk.
	 */
	bool writelog;

	/** The size of the read() buffer
	 */
	int NetBufferSize;

	/** The value to be used for listen() backlogs
	 * as default.
	 */
	int MaxConn;

	/** The soft limit value
	 * The server will not allow more than this number of connections.
	 */
	unsigned int SoftLimit;

	/** True if the DEBUG loglevel is selected.
	 */
	int debugging;

	/** The loglevel in use by the IRC server
	 */
	int LogLevel;

	/** The full pathname and filename of the PID
	 * file as defined in the configuration.
	 */
	char PID[MAXBUF];

	/** A list of the classes for listening client ports
	 */
	std::vector<ListenSocket*> ports;

	/** The path and filename of the ircd.log file
	 */
	std::string logpath;

	/** Custom version string, which if defined can replace the system info in VERSION.
	 */
	char CustomVersion[MAXBUF];

	/** Directory where the inspircd binary resides
	 */
	std::string MyDir;
	
	/** Base directory for serving HTTP files
	 */
	char DocRoot[MAXBUF];

	/** Where to chroot() to.
	 */
	char ChRoot[MAXBUF];

	/** Maximum size of a POST body.
	 */
	int MaxPostBody;
	
	/** Duration to cache stat() calls (0 is disabled)
	 */
	int StatCacheDuration;

	/** How often to check socket list for timed out connections
	 */
	int TimeoutCullFrequency;

	/** How old a socket must exist at least to be timed out
	 */
	int TimeoutTotalLifetime;

	/** How long a socket must be eventless to be timed out
	 */
	int TimeoutIdleLifetime;

	/** Both for set(g|u)id.
	 */
	char SetUser[MAXBUF];
	char SetGroup[MAXBUF];

	/** If symlinks should be followed while serving files
	 */
	bool FollowSymLinks;
	
	/** Disable access time for served files on supporting systems
	 */
	bool NoAtime;
	
	/** Maximum number of keepalive requests per connection (0 = keepalive disabled)
	 */
	int KeepAliveMax;
	
	/** Saved argv from startup
	 */
	char** argv;

	/** Saved argc from startup
	 */
	int argc;

	/** Construct a new ServerConfig
	 */
	ServerConfig(InspIRCd* Instance);

	/** Clears the include stack in preperation for a Read() call.
	 */
	void ClearStack();

	/** Read the entire configuration into memory
	 * and initialize this class. All other methods
	 * should be used only by the core.
	 */
	void Read(bool bail);

	/** Read a file into a file_cache object
	 */
	bool ReadFile(file_cache &F, const char* fname);

	/** Report a configuration error given in errormessage.
	 * @param bail If this is set to true, the error is sent to the console, and the program exits
	 * @param connection If this is set to a non-null value, and bail is false, the errors are spooled to
	 * this connection as SNOTICEs.
	 * If the parameter is NULL, the messages are spooled to all connections via WriteOpers as SNOTICEs.
	 */
	void ReportConfigError(const std::string &errormessage, bool bail);

	/** Load 'filename' into 'target', with the new config parser everything is parsed into
	 * tag/key/value at load-time rather than at read-value time.
	 */
	bool LoadConf(ConfigDataHash &target, const char* filename, std::ostringstream &errorstream);

	/** Load 'filename' into 'target', with the new config parser everything is parsed into
	 * tag/key/value at load-time rather than at read-value time.
	 */
	bool LoadConf(ConfigDataHash &target, const std::string &filename, std::ostringstream &errorstream);
	
	/* Both these return true if the value existed or false otherwise */
	
	/** Writes 'length' chars into 'result' as a string
	 */
	bool ConfValue(ConfigDataHash &target, const char* tag, const char* var, int index, char* result, int length, bool allow_linefeeds = false);
	/** Writes 'length' chars into 'result' as a string
	 */
	bool ConfValue(ConfigDataHash &target, const char* tag, const char* var, const char* default_value, int index, char* result, int length, bool allow_linefeeds = false);

	/** Writes 'length' chars into 'result' as a string
	 */
	bool ConfValue(ConfigDataHash &target, const std::string &tag, const std::string &var, int index, std::string &result, bool allow_linefeeds = false);
	/** Writes 'length' chars into 'result' as a string
	 */
	bool ConfValue(ConfigDataHash &target, const std::string &tag, const std::string &var, const std::string &default_value, int index, std::string &result, bool allow_linefeeds = false);
	
	/** Tries to convert the value to an integer and write it to 'result'
	 */
	bool ConfValueInteger(ConfigDataHash &target, const char* tag, const char* var, int index, int &result);
	/** Tries to convert the value to an integer and write it to 'result'
	 */
	bool ConfValueInteger(ConfigDataHash &target, const char* tag, const char* var, const char* default_value, int index, int &result);
	/** Tries to convert the value to an integer and write it to 'result'
	 */
	bool ConfValueInteger(ConfigDataHash &target, const std::string &tag, const std::string &var, int index, int &result);
	/** Tries to convert the value to an integer and write it to 'result'
	 */
	bool ConfValueInteger(ConfigDataHash &target, const std::string &tag, const std::string &var, const std::string &default_value, int index, int &result);
	
	/** Returns true if the value exists and has a true value, false otherwise
	 */
	bool ConfValueBool(ConfigDataHash &target, const char* tag, const char* var, int index);
	/** Returns true if the value exists and has a true value, false otherwise
	 */
	bool ConfValueBool(ConfigDataHash &target, const char* tag, const char* var, const char* default_value, int index);
	/** Returns true if the value exists and has a true value, false otherwise
	 */
	bool ConfValueBool(ConfigDataHash &target, const std::string &tag, const std::string &var, int index);
	/** Returns true if the value exists and has a true value, false otherwise
	 */
	bool ConfValueBool(ConfigDataHash &target, const std::string &tag, const std::string &var, const std::string &default_value, int index);
	
	/** Returns the number of occurences of tag in the config file
	 */
	int ConfValueEnum(ConfigDataHash &target, const char* tag);
	/** Returns the number of occurences of tag in the config file
	 */
	int ConfValueEnum(ConfigDataHash &target, const std::string &tag);
	
	/** Returns the numbers of vars inside the index'th 'tag in the config file
	 */
	int ConfVarEnum(ConfigDataHash &target, const char* tag, int index);
	/** Returns the numbers of vars inside the index'th 'tag in the config file
	 */
	int ConfVarEnum(ConfigDataHash &target, const std::string &tag, int index);

	void ValidateHostname(const char* p, const std::string &tag, const std::string &val);

	void ValidateIP(const char* p, const std::string &tag, const std::string &val, bool wild);

	void ValidateNoSpaces(const char* p, const std::string &tag, const std::string &val);
	
	/** Returns the fully qualified path to the inspircd directory
	 * @return The full program directory
	 */
	std::string GetFullProgDir();

	/** Returns true if a directory is valid (within the modules directory).
	 * @param dirandfile The directory and filename to check
	 * @return True if the directory is valid
	 */
	static bool DirValid(const char* dirandfile);

	/** Clean a filename, stripping the directories (and drives) from string.
	 * @param name Directory to tidy
	 * @return The cleaned filename
	 */
	static char* CleanFilename(char* name);

	/** Check if a file exists.
	 * @param file The full path to a file
	 * @return True if the file exists and is readable.
	 */
	static bool FileExists(const char* file);
	
};

#endif

