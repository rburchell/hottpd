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

#ifndef __MODULES_H
#define __MODULES_H

#include "globals.h"
#include "dynamic.h"
#include "base.h"
#include "timer.h"

/** Used to define a set of behavior bits for a module
 */
enum ModuleFlags {
	VF_STATIC = 1,		// module is static, cannot be /unloadmodule'd
	VF_VENDOR = 2,		// module is a vendor module (came in the original tarball, not 3rd party)
	VF_SERVICEPROVIDER = 4,	// module provides a service to other modules (can be a dependency)
	VF_COMMON = 8		// module needs to be common on all servers in a network to link
};

/** Used to represent an event type, for user, channel or server
 */
enum TargetTypeFlags {
	TYPE_USER = 1,
	TYPE_SERVER,
	TYPE_OTHER
};

/** If you change the module API, change this value.
 * If you have enabled ipv6, the sizes of structs is
 * different, and modules will be incompatible with
 * ipv4 servers, so this value will be ten times as
 * high on ipv6 servers.
 */
#define NATIVE_API_VERSION 10000
#ifdef IPV6
#define API_VERSION (NATIVE_API_VERSION * 10)
#else
#define API_VERSION (NATIVE_API_VERSION * 1)
#endif

/* Forward-declare module for ModuleMessage etc
 */
class ServerConfig;
class Module;
class InspIRCd;

/** Low level definition of a FileReader classes file cache area -
 * a text file seperated into lines.
 */
typedef std::deque<std::string> file_cache;

/** A set of strings.
 */
typedef file_cache string_list;

/** Holds a list of 'published features' for modules.
 */
typedef std::map<std::string,Module*> featurelist;

/** Holds a list of modules which implement an interface
 */
typedef std::deque<Module*> modulelist;

/** Holds a list of all modules which implement interfaces, by interface name
 */
typedef std::map<std::string, std::pair<int, modulelist> > interfacelist;

/**
 * This #define allows us to call a method in all
 * loaded modules in a readable simple way, e.g.:
 * 'FOREACH_MOD(I_OnConnect,OnConnect(user));'
 */
#define FOREACH_MOD(y,x) do { \
	EventHandlerIter safei; \
	for (EventHandlerIter _i = ServerInstance->Modules->EventHandlers[y].begin(); _i != ServerInstance->Modules->EventHandlers[y].end(); ) \
	{ \
		safei = _i; \
		++safei; \
		try \
		{ \
			(*_i)->x ; \
		} \
		catch (CoreException& modexcept) \
		{ \
			ServerInstance->Log(DEFAULT,"Exception caught: %s",modexcept.GetReason()); \
		} \
		_i = safei; \
	} \
} while (0);

/**
 * This #define allows us to call a method in all
 * loaded modules in a readable simple way and pass
 * an instance pointer to the macro. e.g.:
 * 'FOREACH_MOD_I(Instance, OnConnect, OnConnect(user));'
 */
#define FOREACH_MOD_I(z,y,x) do { \
	EventHandlerIter safei; \
	for (EventHandlerIter _i = z->Modules->EventHandlers[y].begin(); _i != z->Modules->EventHandlers[y].end(); ) \
	{ \
		safei = _i; \
		++safei; \
		try \
		{ \
			(*_i)->x ; \
		} \
		catch (CoreException& modexcept) \
		{ \
			z->Log(DEFAULT,"Exception caught: %s",modexcept.GetReason()); \
		} \
		_i = safei; \
	} \
} while (0);

/**
 * This define is similar to the one above but returns a result in MOD_RESULT.
 * The first module to return a nonzero result is the value to be accepted,
 * and any modules after are ignored.
 */
#define FOREACH_RESULT(y,x) \
do { \
	EventHandlerIter safei; \
	MOD_RESULT = 0; \
	for (EventHandlerIter _i = ServerInstance->Modules->EventHandlers[y].begin(); _i != ServerInstance->Modules->EventHandlers[y].end(); ) \
	{ \
		safei = _i; \
		++safei; \
		try \
		{ \
			int res = (*_i)->x ; \
			if (res != 0) { \
				MOD_RESULT = res; \
				break; \
			} \
		} \
		catch (CoreException& modexcept) \
		{ \
			ServerInstance->Log(DEFAULT,"Exception caught: %s",modexcept.GetReason()); \
		} \
		_i = safei; \
	} \
} while(0);


/**
 * This define is similar to the one above but returns a result in MOD_RESULT.
 * The first module to return a nonzero result is the value to be accepted,
 * and any modules after are ignored.
 */
#define FOREACH_RESULT_I(z,y,x) \
do { \
	EventHandlerIter safei; \
	MOD_RESULT = 0; \
	for (EventHandlerIter _i = z->Modules->EventHandlers[y].begin(); _i != z->Modules->EventHandlers[y].end(); ) \
	{ \
		safei = _i; \
		++safei; \
		try \
		{ \
			int res = (*_i)->x ; \
			if (res != 0) { \
				MOD_RESULT = res; \
				break; \
			} \
		} \
		catch (CoreException& modexcept) \
		{ \
			z->Log(DEBUG,"Exception caught: %s",modexcept.GetReason()); \
		} \
		_i = safei; \
	} \
} while (0);

/** Represents a non-local user.
 * (in fact, any FD less than -1 does)
 */
#define FD_MAGIC_NUMBER -42

/* Useful macros */
#ifdef WINDOWS
/** Is a local user */
#define IS_LOCAL(x) ((x->GetFd() > -1))
#else
/** Is a local user */
#define IS_LOCAL(x) ((x->GetFd() > -1) && (x->GetFd() <= MAX_DESCRIPTORS))
#endif
/** Is a remote user */
#define IS_REMOTE(x) (x->GetFd() < 0)
/** Is a module created user */
#define IS_MODULE_CREATED(x) (x->GetFd() == FD_MAGIC_NUMBER)
/** Is an oper */
#define IS_OPER(x) (*x->oper)
/** Is away */
#define IS_AWAY(x) (*x->awaymsg)

/** Holds a module's Version information.
 *  The four members (set by the constructor only) indicate details as to the version number
 *  of a module. A class of type Version is returned by the GetVersion method of the Module class.
 *  The flags and API values represent the module flags and API version of the module.
 *  The API version of a module must match the API version of the core exactly for the module to
 *  load successfully.
 */
class CoreExport Version : public classbase
{
 public:
	 /** Version numbers, build number, flags and API version
	  */
	 const int Major, Minor, Revision, Build, Flags, API;

	 /** Initialize version class
	  */
	 Version(int major, int minor, int revision, int build, int flags, int api_ver);
};

/** The ModuleMessage class is the base class of Request and Event
 * This class is used to represent a basic data structure which is passed
 * between modules for safe inter-module communications.
 */
class CoreExport ModuleMessage : public Extensible
{
 public:
	/** Destructor
	 */
	virtual ~ModuleMessage() {};
};

/** The Request class is a unicast message directed at a given module.
 * When this class is properly instantiated it may be sent to a module
 * using the Send() method, which will call the given module's OnRequest
 * method with this class as its parameter.
 */
class CoreExport Request : public ModuleMessage
{
 protected:
	/** This member holds a pointer to arbitary data set by the emitter of the message
	 */
	char* data;
 	/** This should be a null-terminated string identifying the type of request,
 	 * all modules should define this and use it to determine the nature of the
 	 * request before they attempt to cast the Request in any way.
 	 */
 	const char* id;
	/** This is a pointer to the sender of the message, which can be used to
	 * directly trigger events, or to create a reply.
	 */
	Module* source;
	/** The single destination of the Request
	 */
	Module* dest;
 public:
	/** Create a new Request
 	 * This is for the 'old' way of casting whatever the data is
 	 * to char* and hoping you get the right thing at the other end.
 	 * This is slowly being depreciated in favor of the 'new' way.
	 */
	Request(char* anydata, Module* src, Module* dst);
 	/** Create a new Request
 	 * This is for the 'new' way of defining a subclass
 	 * of Request and defining it in a common header,
	 * passing an object of your Request subclass through
 	 * as a Request* and using the ID string to determine
 	 * what to cast it back to and the other end. This is
 	 * much safer as there are no casts not confirmed by
 	 * the ID string, and all casts are child->parent and
 	 * can be checked at runtime with dynamic_cast<>()
 	 */
 	Request(Module* src, Module* dst, const char* idstr);
	/** Fetch the Request data
	 */
	char* GetData();
 	/** Fetch the ID string
	 */
	const char* GetId();
	/** Fetch the request source
	 */
	Module* GetSource();
	/** Fetch the request destination (should be 'this' in the receiving module)
	 */
	Module* GetDest();
	/** Send the Request.
	 * Upon returning the result will be arbitary data returned by the module you
	 * sent the request to. It is up to your module to know what this data is and
	 * how to deal with it.
	 */
	char* Send();
};


/** The Event class is a unicast message directed at all modules.
 * When the class is properly instantiated it may be sent to all modules
 * using the Send() method, which will trigger the OnEvent method in
 * all modules passing the object as its parameter.
 */
class CoreExport Event : public ModuleMessage
{
 protected:
	/** This member holds a pointer to arbitary data set by the emitter of the message
	 */
	char* data;
	/** This is a pointer to the sender of the message, which can be used to
	 * directly trigger events, or to create a reply.
	 */
	Module* source;
	/** The event identifier.
	 * This is arbitary text which should be used to distinguish
	 * one type of event from another.
	 */
	std::string id;
 public:
	/** Create a new Event
	 */
	Event(char* anydata, Module* src, const std::string &eventid);
	/** Get the Event data
	 */
	char* GetData();
	/** Get the event Source
	 */
	Module* GetSource();
	/** Get the event ID.
	 * Use this to determine the event type for safe casting of the data
	 */
	std::string GetEventID();
	/** Send the Event.
	 * The return result of an Event::Send() will always be NULL as
	 * no replies are expected.
	 */
	char* Send(InspIRCd* ServerInstance);
};

/** Priority types which can be returned from Module::Prioritize()
 */
enum Priority { PRIORITY_FIRST, PRIORITY_DONTCARE, PRIORITY_LAST, PRIORITY_BEFORE, PRIORITY_AFTER };

/** Implementation-specific flags which may be set in Module::Implements()
 */
enum Implementation
{
	I_BEGIN,
	I_OnConnectionConnect, I_OnConnectionDisconnect, I_OnRehash,
	I_OnCleanup, I_OnLoadModule, I_OnUnloadModule,
	I_OnBackgroundTimer,
	I_OnEvent, I_OnRequest,
	I_OnRawSocketAccept, I_OnRawSocketClose, I_OnRawSocketWrite, I_OnRawSocketRead,
	I_OnRawSocketConnect, I_OnGarbageCollect, I_OnBufferFlushed,
	I_OnReadConfig,
	I_END
};

class ConfigReader;

/** Base class for all InspIRCd modules
 *  This class is the base class for InspIRCd modules. All modules must inherit from this class,
 *  its methods will be called when irc server events occur. class inherited from module must be
 *  instantiated by the ModuleFactory class (see relevent section) for the module to be initialised.
 */
class CoreExport Module : public Extensible
{
 protected:
	/** Creator/owner pointer
	 */
	InspIRCd* ServerInstance;
 public:

	/** Default constructor.
	 * Creates a module class.
	 * @param Me An instance of the InspIRCd class which will be saved into ServerInstance for your use
	 * \exception ModuleException Throwing this class, or any class derived from ModuleException, causes loading of the module to abort.
	 */
	Module(InspIRCd* Me);

	/** Default destructor.
	 * destroys a module class
	 */
	virtual ~Module();

	virtual void Prioritize()
	{
	}

	virtual void OnReadConfig(ServerConfig* config, ConfigReader* coreconf);

	virtual int OnDownloadFile(const std::string &filename, std::istream* &filedata);

	/** Returns the version number of a Module.
	 * The method should return a Version object with its version information assigned via
	 * Version::Version
	 */
	virtual Version GetVersion();

	/** Called when a user connects.
	 * The details of the connecting user are available to you in the parameter Connection *user
	 * @param user The user who is connecting
	 */
	virtual void OnConnectionConnect(Connection* user);

	/** Called whenever a user's socket is closed.
	 * The details of the exiting user are available to you in the parameter Connection *user
	 * This event is called for all users, registered or not, as a cleanup method for modules
	 * which might assign resources to user, such as dns lookups, objects and sockets.
	 * @param user The user who is disconnecting
	 */
	virtual void OnConnectionDisconnect(Connection* user);

	/** Called on rehash.
	 * This method is called when a SIGHUP is received from the operating system.
	 * You should use it to reload any files so that your module keeps in step with the
	 * rest of the application.
	 */
 	virtual void OnRehash();

	/** Called before your module is unloaded to clean up Extensibles.
	 * This method is called once for every connection,
	 * so that when your module unloads it may clear up any remaining data
	 * in the form of Extensibles added using Extensible::Extend().
	 */
	virtual void OnCleanup(Connection *);

	/** Called whenever a module is loaded.
	 * mod will contain a pointer to the module, and string will contain its name,
	 * for example m_widgets.so. This function is primary for dependency checking,
	 * your module may decide to enable some extra features if it sees that you have
	 * for example loaded "m_killwidgets.so" with "m_makewidgets.so". It is highly
	 * recommended that modules do *NOT* bail if they cannot satisfy dependencies,
	 * but instead operate under reduced functionality, unless the dependency is
	 * absolutely neccessary (e.g. a module that extends the features of another
	 * module).
	 * @param mod A pointer to the new module
	 * @param name The new module's filename
	 */
	virtual void OnLoadModule(Module* mod,const std::string &name);

	/** Called whenever a module is unloaded.
	 * mod will contain a pointer to the module, and string will contain its name,
	 * for example m_widgets.so. This function is primary for dependency checking,
	 * your module may decide to enable some extra features if it sees that you have
	 * for example loaded "m_killwidgets.so" with "m_makewidgets.so". It is highly
	 * recommended that modules do *NOT* bail if they cannot satisfy dependencies,
	 * but instead operate under reduced functionality, unless the dependency is
	 * absolutely neccessary (e.g. a module that extends the features of another
	 * module).
	 * @param mod Pointer to the module being unloaded (still valid)
	 * @param name The filename of the module being unloaded
	 */
	virtual void OnUnloadModule(Module* mod,const std::string &name);

	/** Called once every five seconds for background processing.
	 * This timer can be used to control timed features. Its period is not accurate
	 * enough to be used as a clock, but it is gauranteed to be called at least once in
	 * any five second period, directly from the main loop of the server.
	 * @param curtime The current timer derived from time(2)
	 */
	virtual void OnBackgroundTimer(time_t curtime);

	/** Called whenever an Event class is sent to all module by another module.
	 * Please see the documentation of Event::Send() for further information. The Event sent can
	 * always be assumed to be non-NULL, you should *always* check the value of Event::GetEventID()
	 * before doing anything to the event data, and you should *not* change the event data in any way!
	 * @param event The Event class being received
	 */
	virtual void OnEvent(Event* event);

	/** Called whenever a Request class is sent to your module by another module.
	 * Please see the documentation of Request::Send() for further information. The Request sent
	 * can always be assumed to be non-NULL, you should not change the request object or its data.
	 * Your method may return arbitary data in the char* result which the requesting module
	 * may be able to use for pre-determined purposes (e.g. the results of an SQL query, etc).
	 * @param request The Request class being received
	 */
	virtual char* OnRequest(Request* request);

	/** Called immediately after any  connection is accepted. This is intended for raw socket
	 * processing (e.g. modules which wrap the tcp connection within another library) and provides
	 * no information relating to a user record as the connection has not been assigned yet.
	 * There are no return values from this call as all modules get an opportunity if required to
	 * process the connection.
	 * @param fd The file descriptor returned from accept()
	 * @param ip The IP address of the connecting user
	 * @param localport The local port number the user connected to
	 */
	virtual void OnRawSocketAccept(int fd, const std::string &ip, int localport);

	/** Called immediately before any write() operation on a user's socket in the core. Because
	 * this event is a low level event no user information is associated with it. It is intended
	 * for use by modules which may wrap connections within another API such as SSL for example.
	 * return a non-zero result if you have handled the write operation, in which case the core
	 * will not call write().
	 * @param fd The file descriptor of the socket
	 * @param buffer A char* buffer being written
	 * @param Number of characters to write
	 * @return Number of characters actually written or 0 if you didn't handle the operation
	 */
	virtual int OnRawSocketWrite(int fd, const char* buffer, int count);

	/** Called immediately before any socket is closed. When this event is called, shutdown()
	 * has not yet been called on the socket.
	 * @param fd The file descriptor of the socket prior to close()
	 */
	virtual void OnRawSocketClose(int fd);

	/** Called immediately upon connection of an outbound BufferedSocket which has been hooked
	 * by a module.
	 * @param fd The file descriptor of the socket immediately after connect()
	 */
	virtual void OnRawSocketConnect(int fd);

	/** Called immediately before any read() operation on a client socket in the core.
	 * This occurs AFTER the select() or poll() so there is always data waiting to be read
	 * when this event occurs.
	 * Your event should return 1 if it has handled the reading itself, which prevents the core
	 * just using read(). You should place any data read into buffer, up to but NOT GREATER THAN
	 * the value of count. The value of readresult must be identical to an actual result that might
	 * be returned from the read() system call, for example, number of bytes read upon success,
	 * 0 upon EOF or closed socket, and -1 for error. If your function returns a nonzero value,
	 * you MUST set readresult.
	 * @param fd The file descriptor of the socket
	 * @param buffer A char* buffer being read to
	 * @param count The size of the buffer
	 * @param readresult The amount of characters read, or 0
	 * @return nonzero if the event was handled, in which case readresult must be valid on exit
	 */
	virtual int OnRawSocketRead(int fd, char* buffer, unsigned int count, int &readresult);

	/** Called at intervals for modules to garbage-collect any hashes etc.
	 * Certain data types such as hash_map 'leak' buckets, which must be
	 * tidied up and freed by copying into a new item every so often. This
	 * method is called when it is time to do that.
	 */
	virtual void OnGarbageCollect();

	/** Called whenever a user's write buffer has been completely sent.
	 * This is called when the user's write buffer is completely empty, and
	 * there are no more pending bytes to be written and no pending write events
	 * in the socket engine's queue. This may be used to refill the buffer with
	 * data which is being spooled in a controlled manner, e.g. LIST lines.
	 * @param user The user who's buffer is now empty.
	 */
	virtual void OnBufferFlushed(Connection* user);
};


#define CONF_NO_ERROR		0x000000
#define CONF_NOT_A_NUMBER	0x000010
#define CONF_INT_NEGATIVE	0x000080
#define CONF_VALUE_NOT_FOUND	0x000100
#define CONF_FILE_NOT_FOUND	0x000200


/** Allows reading of values from configuration files
 * This class allows a module to read from either the main configuration file (inspircd.conf) or from
 * a module-specified configuration file. It may either be instantiated with one parameter or none.
 * Constructing the class using one parameter allows you to specify a path to your own configuration
 * file, otherwise, inspircd.conf is read.
 */
class CoreExport ConfigReader : public classbase
{
  protected:
	InspIRCd* ServerInstance;
	/** The contents of the configuration file
	 * This protected member should never be accessed by a module (and cannot be accessed unless the
	 * core is changed). It will contain a pointer to the configuration file data with unneeded data
	 * (such as comments) stripped from it.
	 */
	ConfigDataHash* data;
	/** Used to store errors
	 */
	std::ostringstream* errorlog;
	/** If we're using our own config data hash or not
	 */
	bool privatehash;
	/** True if an error occured reading the config file
	 */
	bool readerror;
	/** Error code
	 */
	long error;
	
  public:
	/** Default constructor.
	 * This constructor initialises the ConfigReader class to read the inspircd.conf file
	 * as specified when running ./configure.
	 */
	ConfigReader(InspIRCd* Instance);
	/** Overloaded constructor.
	 * This constructor initialises the ConfigReader class to read a user-specified config file
	 */
	ConfigReader(InspIRCd* Instance, const std::string &filename);
	/** Default destructor.
	 * This method destroys the ConfigReader class.
	 */
	~ConfigReader();

	/** Retrieves a value from the config file.
	 * This method retrieves a value from the config file. Where multiple copies of the tag
	 * exist in the config file, index indicates which of the values to retrieve.
	 */
	std::string ReadValue(const std::string &tag, const std::string &name, int index, bool allow_linefeeds = false);
	/** Retrieves a value from the config file.
	 * This method retrieves a value from the config file. Where multiple copies of the tag
	 * exist in the config file, index indicates which of the values to retrieve. If the
	 * tag is not found the default value is returned instead.
	 */
	std::string ReadValue(const std::string &tag, const std::string &name, const std::string &default_value, int index, bool allow_linefeeds = false);

	/** Retrieves a boolean value from the config file.
	 * This method retrieves a boolean value from the config file. Where multiple copies of the tag
	 * exist in the config file, index indicates which of the values to retrieve. The values "1", "yes"
	 * and "true" in the config file count as true to ReadFlag, and any other value counts as false.
	 */
	bool ReadFlag(const std::string &tag, const std::string &name, int index);
	/** Retrieves a boolean value from the config file.
	 * This method retrieves a boolean value from the config file. Where multiple copies of the tag
	 * exist in the config file, index indicates which of the values to retrieve. The values "1", "yes"
	 * and "true" in the config file count as true to ReadFlag, and any other value counts as false.
	 * If the tag is not found, the default value is used instead.
	 */
	bool ReadFlag(const std::string &tag, const std::string &name, const std::string &default_value, int index);

	/** Retrieves an integer value from the config file.
	 * This method retrieves an integer value from the config file. Where multiple copies of the tag
	 * exist in the config file, index indicates which of the values to retrieve. Any invalid integer
	 * values in the tag will cause the objects error value to be set, and any call to GetError() will
	 * return CONF_INVALID_NUMBER to be returned. need_positive is set if the number must be non-negative.
	 * If a negative number is placed into a tag which is specified positive, 0 will be returned and GetError()
	 * will return CONF_INT_NEGATIVE. Note that need_positive is not suitable to get an unsigned int - you
	 * should cast the result to achieve that effect.
	 */
	int ReadInteger(const std::string &tag, const std::string &name, int index, bool need_positive);
	/** Retrieves an integer value from the config file.
	 * This method retrieves an integer value from the config file. Where multiple copies of the tag
	 * exist in the config file, index indicates which of the values to retrieve. Any invalid integer
	 * values in the tag will cause the objects error value to be set, and any call to GetError() will
	 * return CONF_INVALID_NUMBER to be returned. needs_unsigned is set if the number must be unsigned.
	 * If a signed number is placed into a tag which is specified unsigned, 0 will be returned and GetError()
	 * will return CONF_NOT_UNSIGNED. If the tag is not found, the default value is used instead.
	 */
	int ReadInteger(const std::string &tag, const std::string &name, const std::string &default_value, int index, bool need_positive);

	/** Returns the last error to occur.
	 * Valid errors can be found by looking in modules.h. Any nonzero value indicates an error condition.
	 * A call to GetError() resets the error flag back to 0.
	 */
	long GetError();
	/** Counts the number of times a given tag appears in the config file.
	 * This method counts the number of times a tag appears in a config file, for use where
	 * there are several tags of the same kind, e.g. with opers and connect types. It can be
	 * used with the index value of ConfigReader::ReadValue to loop through all copies of a
	 * multiple instance tag.
	 */
	int Enumerate(const std::string &tag);
	/** Returns true if a config file is valid.
	 * This method is partially implemented and will only return false if the config
	 * file does not exist or could not be opened.
	 */
	bool Verify();
	/** Dumps the list of errors in a config file to an output location. If bail is true,
	 * then the program will abort. If bail is false and user points to a valid user
	 * record, the error report will be spooled to the given user by means of NOTICE.
	 * if bool is false AND user is false, the error report will be spooled to all opers
	 * by means of a NOTICE to all opers.
	 */
	void DumpErrors(bool bail);

	/** Returns the number of items within a tag.
	 * For example if the tag was &lt;test tag="blah" data="foo"&gt; then this
	 * function would return 2. Spaces and newlines both qualify as valid seperators
	 * between values.
	 */
	int EnumerateValues(const std::string &tag, int index);
};



/** Caches a text file into memory and can be used to retrieve lines from it.
 * This class contains methods for read-only manipulation of a text file in memory.
 * Either use the constructor type with one parameter to load a file into memory
 * at construction, or use the LoadFile method to load a file.
 */
class CoreExport FileReader : public classbase
{
	InspIRCd* ServerInstance;
	/** The file contents
	 */
	file_cache fc;

	/** Content size in bytes
	 */
	unsigned long contentsize;

	/** Calculate content size in bytes
	 */
	void CalcSize();

 public:
	/** Default constructor.
	 * This method does not load any file into memory, you must use the LoadFile method
	 * after constructing the class this way.
	 */
	FileReader(InspIRCd* Instance);

	/** Secondary constructor.
	 * This method initialises the class with a file loaded into it ready for GetLine and
	 * and other methods to be called. If the file could not be loaded, FileReader::FileSize
	 * returns 0.
	 */
	FileReader(InspIRCd* Instance, const std::string &filename);

	/** Default destructor.
	 * This deletes the memory allocated to the file.
	 */
	~FileReader();

	/** Used to load a file.
	 * This method loads a file into the class ready for GetLine and
	 * and other methods to be called. If the file could not be loaded, FileReader::FileSize
	 * returns 0.
	 */
	void LoadFile(const std::string &filename);

	/** Returns the whole content of the file as std::string
	 */
	std::string Contents();

	/** Returns the entire size of the file as std::string
	 */
	unsigned long ContentSize();

	/** Returns true if the file exists
	 * This function will return false if the file could not be opened.
	 */
	bool Exists();
 
	/** Retrieve one line from the file.
	 * This method retrieves one line from the text file. If an empty non-NULL string is returned,
	 * the index was out of bounds, or the line had no data on it.
	 */
	std::string GetLine(int x);

	/** Returns the size of the file in lines.
	 * This method returns the number of lines in the read file. If it is 0, no lines have been
	 * read into memory, either because the file is empty or it does not exist, or cannot be
	 * opened due to permission problems.
	 */
	int FileSize();
};

/** A DLLFactory (designed to load shared objects) containing a
 * handle to a module's init_module() function. Unfortunately,
 * due to the design of shared object systems we must keep this
 * hanging around, as if we remove this handle, we remove the
 * shared object file from memory (!)
 */
typedef DLLFactory<Module> ircd_module;

/** A list of modules
 */
typedef std::vector<Module*> IntModuleList;

/** An event handler iterator
 */
typedef IntModuleList::iterator EventHandlerIter;

/** Module priority states
 */
enum PriorityState
{
	PRIO_DONTCARE,
	PRIO_FIRST,
	PRIO_LAST,
	PRIO_AFTER,
	PRIO_BEFORE
};

/** ModuleManager takes care of all things module-related
 * in the core.
 */
class CoreExport ModuleManager : public classbase
{
 private:
	/** Holds a string describing the last module error to occur
	 */
	std::string LastModuleError;
 
 	/** The feature names published by various modules
	 */
	featurelist Features;

	/** The interface names published by various modules
	 */
	interfacelist Interfaces;
 
	/** Total number of modules loaded into the ircd
	 */
	int ModCount; 
	
	/** Our pointer to the main insp instance
	 */
	InspIRCd* Instance;

	/** List of loaded modules and shared object/dll handles
	 * keyed by module name
	 */
	std::map<std::string, std::pair<ircd_module*, Module*> > Modules;

 public:

	/** Event handler hooks.
	 * This needs to be public to be used by FOREACH_MOD and friends.
	 */
	IntModuleList EventHandlers[I_END];

	/** Simple, bog-standard, boring constructor.
	 */
	ModuleManager(InspIRCd* Ins);

	/** Destructor
	 */
	~ModuleManager(); 

	/** Change the priority of one event in a module.
	 * Each module event has a list of modules which are attached to that event type.
	 * If you wish to be called before or after other specific modules, you may use this
	 * method (usually within void Module::Prioritize()) to set your events priority.
	 * You may use this call in other methods too, however, this is not supported behaviour
	 * for a module.
	 * @param mod The module to change the priority of
	 * @param i The event to change the priority of
	 * @param s The state you wish to use for this event. Use one of
	 * PRIO_FIRST to set the event to be first called, PRIO_LAST to
	 * set it to be the last called, or PRIO_BEFORE and PRIO_AFTER
	 * to set it to be before or after one or more other modules.
	 * @param modules If PRIO_BEFORE or PRIO_AFTER is set in parameter 's',
	 * then this contains a list of one or more modules your module must be 
	 * placed before or after. Your module will be placed before the highest
	 * priority module in this list for PRIO_BEFORE, or after the lowest
	 * priority module in this list for PRIO_AFTER.
	 * @param sz The number of modules being passed for PRIO_BEFORE and PRIO_AFTER.
	 * Defaults to 1, as most of the time you will only want to prioritize your module
	 * to be before or after one other module.
	 */
	bool SetPriority(Module* mod, Implementation i, PriorityState s, Module** modules = NULL, size_t sz = 1);

	/** Change the priority of all events in a module.
	 * @param mod The module to set the priority of
	 * @param s The priority of all events in the module.
	 * Note that with this method, it is not possible to effectively use
	 * PRIO_BEFORE or PRIO_AFTER, you should use the more fine tuned
	 * SetPriority method for this, where you may specify other modules to
	 * be prioritized against.
	 */
	bool SetPriority(Module* mod, PriorityState s);

	/** Attach an event to a module.
	 * You may later detatch the event with ModuleManager::Detach().
	 * If your module is unloaded, all events are automatically detatched.
	 * @param i Event type to attach
	 * @param mod Module to attach event to
	 * @return True if the event was attached
	 */
	bool Attach(Implementation i, Module* mod);

	/** Detatch an event from a module.
	 * This is not required when your module unloads, as the core will
	 * automatically detatch your module from all events it is attached to.
	 * @param i Event type to detach
	 * @param mod Module to detach event from
	 * @param Detach true if the event was detached
	 */
	bool Detach(Implementation i, Module* mod);

	/** Attach an array of events to a module
	 * @param i Event types (array) to attach
	 * @param mod Module to attach events to
	 */
	void Attach(Implementation* i, Module* mod, size_t sz);

	/** Detach all events from a module (used on unload)
	 * @param mod Module to detach from
	 */
	void DetachAll(Module* mod);
 
	/** Returns text describing the last module error
	 * @return The last error message to occur
	 */
	std::string& LastError();

	/** Load a given module file
	 * @param filename The file to load
	 * @return True if the module was found and loaded
	 */
	bool Load(const char* filename);

	/** Unload a given module file
	 * @param filename The file to unload
	 * @return True if the module was unloaded
	 */
	bool Unload(const char* filename);
	
	/** Called by the InspIRCd constructor to load all modules from the config file.
	 */
	void LoadAll();
	
	/** Get the total number of currently loaded modules
	 * @return The number of loaded modules
	 */
	int GetCount()
	{
		return this->ModCount;
	}
	
	/** Find a module by name, and return a Module* to it.
	 * This is preferred over iterating the module lists yourself.
	 * @param name The module name to look up
	 * @return A pointer to the module, or NULL if the module cannot be found
	 */
	Module* Find(const std::string &name);
 
	/** Publish a 'feature'.
	 * There are two ways for a module to find another module it depends on.
	 * Either by name, using InspIRCd::FindModule, or by feature, using this
	 * function. A feature is an arbitary string which identifies something this
	 * module can do. For example, if your module provides SSL support, but other
	 * modules provide SSL support too, all the modules supporting SSL should
	 * publish an identical 'SSL' feature. This way, any module requiring use
	 * of SSL functions can just look up the 'SSL' feature using FindFeature,
	 * then use the module pointer they are given.
	 * @param FeatureName The case sensitive feature name to make available
	 * @param Mod a pointer to your module class
	 * @returns True on success, false if the feature is already published by
	 * another module.
	 */
	bool PublishFeature(const std::string &FeatureName, Module* Mod);

	/** Publish a module to an 'interface'.
	 * Modules which implement the same interface (the same way of communicating
	 * with other modules) can publish themselves to an interface, using this
	 * method. When they do so, they become part of a list of related or
	 * compatible modules, and a third module may then query for that list
	 * and know that all modules within that list offer the same API.
	 * A prime example of this is the hashing modules, which all accept the
	 * same types of Request class. Consider this to be similar to PublishFeature,
	 * except for that multiple modules may publish the same 'feature'.
	 * @param InterfaceName The case sensitive interface name to make available
	 * @param Mod a pointer to your module class
	 * @returns True on success, false on failure (there are currently no failure
	 * cases)
	 */
	bool PublishInterface(const std::string &InterfaceName, Module* Mod);

	/** Return a pair saying how many other modules are currently using the
	 * interfaces provided by module m.
	 * @param m The module to count usage for
	 * @return A pair, where the first value is the number of uses of the interface,
	 * and the second value is the interface name being used.
	 */
	std::pair<int,std::string> GetInterfaceInstanceCount(Module* m);

	/** Mark your module as using an interface.
	 * If you mark your module as using an interface, then that interface
	 * module may not unload until your module has unloaded first.
	 * This can be used to prevent crashes by ensuring code you depend on
	 * is always in memory while your module is active.
	 * @param InterfaceName The interface to use
	 */
	void UseInterface(const std::string &InterfaceName);

	/** Mark your module as finished with an interface.
	 * If you used UseInterface() above, you should use this method when
	 * your module is finished with the interface (usually in its destructor)
	 * to allow the modules which implement the given interface to be unloaded.
	 * @param InterfaceName The interface you are finished with using.
	 */
	void DoneWithInterface(const std::string &InterfaceName);

	/** Unpublish a 'feature'.
	 * When your module exits, it must call this method for every feature it
	 * is providing so that the feature table is cleaned up.
	 * @param FeatureName the feature to remove
	 */
	bool UnpublishFeature(const std::string &FeatureName);

	/** Unpublish your module from an interface
	 * When your module exits, it must call this method for every interface
	 * it is part of so that the interfaces table is cleaned up. Only when
	 * the last item is deleted from an interface does the interface get
	 * removed.
	 * @param InterfaceName the interface to be removed from
	 * @param Mod The module to remove from the interface list
	 */
	bool UnpublishInterface(const std::string &InterfaceName, Module* Mod);

	/** Find a 'feature'.
	 * There are two ways for a module to find another module it depends on.
	 * Either by name, using InspIRCd::FindModule, or by feature, using the
	 * InspIRCd::PublishFeature method. A feature is an arbitary string which
	 * identifies something this module can do. For example, if your module
	 * provides SSL support, but other modules provide SSL support too, all
	 * the modules supporting SSL should publish an identical 'SSL' feature.
	 * To find a module capable of providing the feature you want, simply
	 * call this method with the feature name you are looking for.
	 * @param FeatureName The feature name you wish to obtain the module for
	 * @returns A pointer to a valid module class on success, NULL on failure.
	 */
	Module* FindFeature(const std::string &FeatureName);

	/** Find an 'interface'.
	 * An interface is a list of modules which all implement the same API.
	 * @param InterfaceName The Interface you wish to obtain the module
	 * list of.
	 * @return A pointer to a deque of Module*, or NULL if the interface
	 * does not exist.
	 */
	modulelist* FindInterface(const std::string &InterfaceName);

	/** Given a pointer to a Module, return its filename
	 * @param m The module pointer to identify
	 * @return The module name or an empty string
	 */
	const std::string& GetModuleName(Module* m);

	/** Return a list of all modules matching the given filter
	 * @param filter This int is a bitmask of flags set in Module::Flags,
	 * such as VF_VENDOR or VF_STATIC. If you wish to receive a list of
	 * all modules with no filtering, set this to 0.
	 * @return The list of module names
	 */
	const std::vector<std::string> GetAllModuleNames(int filter);
};

/** This definition is used as shorthand for the various classes
 * and functions needed to make a module loadable by the OS.
 * It defines the class factory and external init_module function.
 */
#define MODULE_INIT(y) \
	extern "C" DllExport Module * init_module(InspIRCd* Me) \
	{ \
		return new y(Me); \
	}

#endif
