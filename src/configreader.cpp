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

/* $Core: libIRCDconfigreader */
/* $CopyInstall: conf/inspircd.quotes.example $(CONPATH) */
/* $CopyInstall: conf/inspircd.rules.example $(CONPATH) */
/* $CopyInstall: conf/inspircd.motd.example $(CONPATH) */
/* $CopyInstall: conf/inspircd.helpop-full.example $(CONPATH) */
/* $CopyInstall: conf/inspircd.helpop.example $(CONPATH) */
/* $CopyInstall: conf/inspircd.censor.example $(CONPATH) */
/* $CopyInstall: conf/inspircd.filter.example $(CONPATH) */
/* $CopyInstall: docs/inspircd.conf.example $(CONPATH) */

#include "inspircd.h"
#include <fstream>
#include "exitcodes.h"

std::vector<std::string> old_module_names, new_module_names, added_modules, removed_modules;

/* Needs forward declaration */
bool ValidateDnsServer(ServerConfig* conf, const char* tag, const char* value, ValueItem &data);
bool DoneELine(ServerConfig* conf, const char* tag);

ServerConfig::ServerConfig(InspIRCd* Instance) : ServerInstance(Instance)
{
	this->ClearStack();
	*CustomVersion = *DieValue = '\0';
	*ModPath = *MyExecutable = *PID = '\0';
	log_file = NULL;
	forcedebug = nofork = false;
	writelog = true;
	DieDelay = 5;
	NetBufferSize = 10240;
	SoftLimit = MAXCLIENTS;
	MaxConn = SOMAXCONN;
	debugging = 0;
	LogLevel = DEFAULT;
}

void ServerConfig::ClearStack()
{
	include_stack.clear();
}

bool ServerConfig::CheckOnce(char* tag)
{
	int count = ConfValueEnum(this->config_data, tag);

	if (count > 1)
	{
		throw CoreException("You have more than one <"+std::string(tag)+"> tag, this is not permitted.");
		return false;
	}
	if (count < 1)
	{
		throw CoreException("You have not defined a <"+std::string(tag)+"> tag, this is required.");
		return false;
	}
	return true;
}

bool NoValidation(ServerConfig*, const char*, const char*, ValueItem&)
{
	return true;
}

bool DoneConfItem(ServerConfig* conf, const char* tag)
{
	return true;
}

void ServerConfig::ValidateNoSpaces(const char* p, const std::string &tag, const std::string &val)
{
	for (const char* ptr = p; *ptr; ++ptr)
	{
		if (*ptr == ' ')
			throw CoreException("The value of <"+tag+":"+val+"> cannot contain spaces");
	}
}

/* NOTE: Before anyone asks why we're not using inet_pton for this, it is because inet_pton and friends do not return so much detail,
 * even in strerror(errno). They just return 'yes' or 'no' to an address without such detail as to whats WRONG with the address.
 * Because ircd users arent as technical as they used to be (;)) we are going to give more of a useful error message.
 */
void ServerConfig::ValidateIP(const char* p, const std::string &tag, const std::string &val, bool wild)
{
	int num_dots = 0;
	int num_seps = 0;
	int not_numbers = false;
	int not_hex = false;

	if (*p)
	{
		if (*p == '.')
			throw CoreException("The value of <"+tag+":"+val+"> is not an IP address");

		for (const char* ptr = p; *ptr; ++ptr)
		{
			if (wild && (*ptr == '*' || *ptr == '?' || *ptr == '/'))
				continue;

			if (*ptr != ':' && *ptr != '.')
			{
				if (*ptr < '0' || *ptr > '9')
					not_numbers = true;
				if ((*ptr < '0' || *ptr > '9') && (toupper(*ptr) < 'A' || toupper(*ptr) > 'F'))
					not_hex = true;
			}
			switch (*ptr)
			{
				case ' ':
					throw CoreException("The value of <"+tag+":"+val+"> is not an IP address");
				case '.':
					num_dots++;
				break;
				case ':':
					num_seps++;
				break;
			}
		}

		if (num_dots > 3)
			throw CoreException("The value of <"+tag+":"+val+"> is an IPv4 address with too many fields!");

		if (num_seps > 8)
			throw CoreException("The value of <"+tag+":"+val+"> is an IPv6 address with too many fields!");

		if (num_seps == 0 && num_dots < 3 && !wild)
			throw CoreException("The value of <"+tag+":"+val+"> looks to be a malformed IPv4 address");

		if (num_seps == 0 && num_dots == 3 && not_numbers)
			throw CoreException("The value of <"+tag+":"+val+"> contains non-numeric characters in an IPv4 address");

		if (num_seps != 0 && not_hex)
			throw CoreException("The value of <"+tag+":"+val+"> contains non-hexdecimal characters in an IPv6 address");

		if (num_seps != 0 && num_dots != 3 && num_dots != 0 && !wild)
			throw CoreException("The value of <"+tag+":"+val+"> is a malformed IPv6 4in6 address");
	}
}

void ServerConfig::ValidateHostname(const char* p, const std::string &tag, const std::string &val)
{
	int num_dots = 0;
	if (*p)
	{
		if (*p == '.')
			throw CoreException("The value of <"+tag+":"+val+"> is not a valid hostname");
		for (const char* ptr = p; *ptr; ++ptr)
		{
			switch (*ptr)
			{
				case ' ':
					throw CoreException("The value of <"+tag+":"+val+"> is not a valid hostname");
				case '.':
					num_dots++;
				break;
			}
		}
		if (num_dots == 0)
			throw CoreException("The value of <"+tag+":"+val+"> is not a valid hostname");
	}
}

bool ValidateMaxTargets(ServerConfig* conf, const char*, const char*, ValueItem &data)
{
	if ((data.GetInteger() < 0) || (data.GetInteger() > 31))
	{
		conf->GetInstance()->Log(DEFAULT,"WARNING: <options:maxtargets> value is greater than 31 or less than 0, set to 20.");
		data.Set(20);
	}
	return true;
}

bool ValidateSoftLimit(ServerConfig* conf, const char*, const char*, ValueItem &data)
{
	if ((data.GetInteger() < 1) || (data.GetInteger() > MAXCLIENTS))
	{
		conf->GetInstance()->Log(DEFAULT,"WARNING: <options:softlimit> value is greater than %d or less than 0, set to %d.",MAXCLIENTS,MAXCLIENTS);
		data.Set(MAXCLIENTS);
	}
	return true;
}

bool ValidateMaxConn(ServerConfig* conf, const char*, const char*, ValueItem &data)
{
	if (data.GetInteger() > SOMAXCONN)
		conf->GetInstance()->Log(DEFAULT,"WARNING: <options:somaxconn> value may be higher than the system-defined SOMAXCONN value!");
	return true;
}

bool ValidateDnsServer(ServerConfig* conf, const char*, const char*, ValueItem &data)
{
	if (!*(data.GetString()))
	{
		std::string nameserver;
		// attempt to look up their nameserver from /etc/resolv.conf
		conf->GetInstance()->Log(DEFAULT,"WARNING: <dns:server> not defined, attempting to find working server in /etc/resolv.conf...");
		ifstream resolv("/etc/resolv.conf");
		bool found_server = false;

		if (resolv.is_open())
		{
			while (resolv >> nameserver)
			{
				if ((nameserver == "nameserver") && (!found_server))
				{
					resolv >> nameserver;
					data.Set(nameserver.c_str());
					found_server = true;
					conf->GetInstance()->Log(DEFAULT,"<dns:server> set to '%s' as first resolver in /etc/resolv.conf.",nameserver.c_str());
				}
			}

			if (!found_server)
			{
				conf->GetInstance()->Log(DEFAULT,"/etc/resolv.conf contains no viable nameserver entries! Defaulting to nameserver '127.0.0.1'!");
				data.Set("127.0.0.1");
			}
		}
		else
		{
			conf->GetInstance()->Log(DEFAULT,"/etc/resolv.conf can't be opened! Defaulting to nameserver '127.0.0.1'!");
			data.Set("127.0.0.1");
		}
	}
	return true;
}

bool ValidateNetBufferSize(ServerConfig* conf, const char*, const char*, ValueItem &data)
{
	if ((!data.GetInteger()) || (data.GetInteger() > 65535) || (data.GetInteger() < 1024))
	{
		conf->GetInstance()->Log(DEFAULT,"No NetBufferSize specified or size out of range, setting to default of 10240.");
		data.Set(10240);
	}
	return true;
}

bool ValidateLogLevel(ServerConfig* conf, const char*, const char*, ValueItem &data)
{
	std::string dbg = data.GetString();
	conf->LogLevel = DEFAULT;

	if (dbg == "debug")
		conf->LogLevel = DEBUG;
	else if (dbg  == "verbose")
		conf->LogLevel = VERBOSE;
	else if (dbg == "default")
		conf->LogLevel = DEFAULT;
	else if (dbg == "sparse")
		conf->LogLevel = SPARSE;
	else if (dbg == "none")
		conf->LogLevel = NONE;

	conf->debugging = (conf->LogLevel == DEBUG);

	return true;
}

bool ValidateNotEmpty(ServerConfig*, const char* tag, const char*, ValueItem &data)
{
	if (!*data.GetString())
		throw CoreException(std::string("The value for ")+tag+" cannot be empty!");
	return true;
}

/* Callback called before processing the first <module> tag
 */
bool InitModule(ServerConfig* conf, const char*)
{
	old_module_names = conf->GetInstance()->Modules->GetAllModuleNames(0);
	new_module_names.clear();
	added_modules.clear();
	removed_modules.clear();
	return true;
}

/* Callback called to process a single <module> tag
 */
bool DoModule(ServerConfig*, const char*, char**, ValueList &values, int*)
{
	const char* modname = values[0].GetString();
	new_module_names.push_back(modname);
	return true;
}

/* Callback called when there are no more <module> tags
 */
bool DoneModule(ServerConfig*, const char*)
{
	// now create a list of new modules that are due to be loaded
	// and a seperate list of modules which are due to be unloaded
	for (std::vector<std::string>::iterator _new = new_module_names.begin(); _new != new_module_names.end(); _new++)
	{
		bool added = true;

		for (std::vector<std::string>::iterator old = old_module_names.begin(); old != old_module_names.end(); old++)
		{
			if (*old == *_new)
				added = false;
		}

		if (added)
			added_modules.push_back(*_new);
	}

	for (std::vector<std::string>::iterator oldm = old_module_names.begin(); oldm != old_module_names.end(); oldm++)
	{
		bool removed = true;
		for (std::vector<std::string>::iterator newm = new_module_names.begin(); newm != new_module_names.end(); newm++)
		{
			if (*newm == *oldm)
				removed = false;
		}

		if (removed)
			removed_modules.push_back(*oldm);
	}
	return true;
}

void ServerConfig::ReportConfigError(const std::string &errormessage, bool bail, User* user)
{
	ServerInstance->Log(DEFAULT, "There were errors in your configuration file: %s", errormessage.c_str());
	if (bail)
	{
		/* Unneeded because of the ServerInstance->Log() aboive? */
		printf("There were errors in your configuration:\n%s\n\n",errormessage.c_str());
		ServerInstance->Exit(EXIT_STATUS_CONFIG);
	}
	else
	{
		std::string errors = errormessage;
		std::string::size_type start;
		unsigned int prefixlen;
		start = 0;
		/* ":ServerInstance->Config->ServerName NOTICE user->nick :" */
//		ServerInstance->WriteOpers("There were errors in the configuration file:");
//		while (start < errors.length())
//		{
//			ServerInstance->WriteOpers(errors.substr(start, 360).c_str());
//			start += 360;
//		}
		return;
	}
}

void ServerConfig::Read(bool bail, User* user, int pass)
{
	int rem = 0, add = 0;           /* Number of modules added, number of modules removed */

	static char debug[MAXBUF];	/* Temporary buffer for debugging value */
	errstr.clear();

	/* These tags MUST occur and must ONLY occur once in the config file */
	static char* Once[] = { "server", "admin", "files", "options", NULL };

	/* These tags can occur ONCE or not at all */
	InitialConfig Values[] = {
		{"options",	"softlimit",	MAXCLIENTS_S,		new ValueContainerUInt (&this->SoftLimit),		DT_INTEGER,  ValidateSoftLimit},
		{"options",	"somaxconn",	SOMAXCONN_S,		new ValueContainerInt  (&this->MaxConn),		DT_INTEGER,  ValidateMaxConn},
		{"options",	"loglevel",	"default",		new ValueContainerChar (debug),				DT_CHARPTR,  ValidateLogLevel},
		{"options",	"netbuffersize","10240",		new ValueContainerInt  (&this->NetBufferSize),		DT_INTEGER,  ValidateNetBufferSize},
		{"options",	"moduledir",	MOD_PATH,		new ValueContainerChar (this->ModPath),			DT_CHARPTR,  NoValidation},
		{"options",	"customversion","",			new ValueContainerChar (this->CustomVersion),		DT_CHARPTR,  NoValidation},
		{"pid",		"file",		"",			new ValueContainerChar (this->PID),			DT_CHARPTR,  NoValidation},
		{"die",		"value",	"",			new ValueContainerChar (this->DieValue),		DT_CHARPTR,  NoValidation},
		{NULL,		NULL,		NULL,			NULL,							DT_NOTHING,  NoValidation}
	};

	/* These tags can occur multiple times, and therefore they have special code to read them
	 * which is different to the code for reading the singular tags listed above.
	 */
	MultiConfig MultiValues[] = {

		{"module",
				{"name",	NULL},
				{"",		NULL},
				{DT_CHARPTR},
				InitModule, DoModule, DoneModule},

		{NULL,
				{NULL},
				{NULL},
				{0},
				NULL, NULL, NULL}
	};

	/* Load and parse the config file, if there are any errors then explode */

	/* Make a copy here so if it fails then we can carry on running with an unaffected config */
	newconfig.clear();

	if (this->LoadConf(newconfig, ServerInstance->ConfigFileName, errstr, pass))
	{
		/* If we succeeded, set the ircd config to the new one */
		this->config_data = newconfig;
	}
	else
	{
		ReportConfigError(errstr.str(), bail, user);
		return;
	}

	/* The stuff in here may throw CoreException, be sure we're in a position to catch it. */
	try
	{
		/* Read the values of all the tags which occur once or not at all, and call their callbacks.
		 */
		for (int Index = 0; Values[Index].tag; Index++)
		{
			char item[MAXBUF];
			int dt = Values[Index].datatype;
			bool allow_newlines = ((dt & DT_ALLOW_NEWLINE) > 0);
			bool allow_wild = ((dt & DT_ALLOW_WILD) > 0);
			dt &= ~DT_ALLOW_NEWLINE;
			dt &= ~DT_ALLOW_WILD;

			ConfValue(this->config_data, Values[Index].tag, Values[Index].value, Values[Index].default_value, 0, item, MAXBUF, allow_newlines);
			ValueItem vi(item);
			
			if (!Values[Index].validation_function(this, Values[Index].tag, Values[Index].value, vi))
				throw CoreException("One or more values in your configuration file failed to validate. Please see your ircd.log for more information.");
	
			switch (Values[Index].datatype)
			{
				case DT_NOSPACES:
				{
					ValueContainerChar* vcc = (ValueContainerChar*)Values[Index].val;
					this->ValidateNoSpaces(vi.GetString(), Values[Index].tag, Values[Index].value);
					vcc->Set(vi.GetString(), strlen(vi.GetString()) + 1);
				}
				break;
				case DT_HOSTNAME:
				{
					ValueContainerChar* vcc = (ValueContainerChar*)Values[Index].val;
					this->ValidateHostname(vi.GetString(), Values[Index].tag, Values[Index].value);
					vcc->Set(vi.GetString(), strlen(vi.GetString()) + 1);
				}
				break;
				case DT_IPADDRESS:
				{
					ValueContainerChar* vcc = (ValueContainerChar*)Values[Index].val;
					this->ValidateIP(vi.GetString(), Values[Index].tag, Values[Index].value, allow_wild);
					vcc->Set(vi.GetString(), strlen(vi.GetString()) + 1);
				}
				break;
				case DT_CHARPTR:
				{
					ValueContainerChar* vcc = (ValueContainerChar*)Values[Index].val;
					/* Make sure we also copy the null terminator */
					vcc->Set(vi.GetString(), strlen(vi.GetString()) + 1);
				}
				break;
				case DT_INTEGER:
				{
					int val = vi.GetInteger();
					ValueContainerInt* vci = (ValueContainerInt*)Values[Index].val;
					vci->Set(&val, sizeof(int));
				}
				break;
				case DT_BOOLEAN:
				{
					bool val = vi.GetBool();
					ValueContainerBool* vcb = (ValueContainerBool*)Values[Index].val;
					vcb->Set(&val, sizeof(bool));
				}
				break;
				default:
					/* You don't want to know what happens if someones bad code sends us here. */
				break;
			}
			/* We're done with this now */
			delete Values[Index].val;
		}

		/* Read the multiple-tag items (class tags, connect tags, etc)
		 * and call the callbacks associated with them. We have three
		 * callbacks for these, a 'start', 'item' and 'end' callback.
		 */
		for (int Index = 0; MultiValues[Index].tag; Index++)
		{
			MultiValues[Index].init_function(this, MultiValues[Index].tag);

			int number_of_tags = ConfValueEnum(this->config_data, MultiValues[Index].tag);

			for (int tagnum = 0; tagnum < number_of_tags; tagnum++)
			{
				ValueList vl;
				for (int valuenum = 0; MultiValues[Index].items[valuenum]; valuenum++)
				{
					int dt = MultiValues[Index].datatype[valuenum];
					bool allow_newlines =  ((dt & DT_ALLOW_NEWLINE) > 0);
					bool allow_wild = ((dt & DT_ALLOW_WILD) > 0);
					dt &= ~DT_ALLOW_NEWLINE;
					dt &= ~DT_ALLOW_WILD;

					switch (dt)
					{
						case DT_NOSPACES:
						{
							char item[MAXBUF];
							if (ConfValue(this->config_data, MultiValues[Index].tag, MultiValues[Index].items[valuenum], MultiValues[Index].items_default[valuenum], tagnum, item, MAXBUF, allow_newlines))
								vl.push_back(ValueItem(item));
							else
								vl.push_back(ValueItem(""));
							this->ValidateNoSpaces(vl[vl.size()-1].GetString(), MultiValues[Index].tag, MultiValues[Index].items[valuenum]);
						}
						break;
						case DT_HOSTNAME:
						{
							char item[MAXBUF];
							if (ConfValue(this->config_data, MultiValues[Index].tag, MultiValues[Index].items[valuenum], MultiValues[Index].items_default[valuenum], tagnum, item, MAXBUF, allow_newlines))
								vl.push_back(ValueItem(item));
							else
								vl.push_back(ValueItem(""));
							this->ValidateHostname(vl[vl.size()-1].GetString(), MultiValues[Index].tag, MultiValues[Index].items[valuenum]);
						}
						break;
						case DT_IPADDRESS:
						{
							char item[MAXBUF];
							if (ConfValue(this->config_data, MultiValues[Index].tag, MultiValues[Index].items[valuenum], MultiValues[Index].items_default[valuenum], tagnum, item, MAXBUF, allow_newlines))
								vl.push_back(ValueItem(item));
							else
								vl.push_back(ValueItem(""));
							this->ValidateIP(vl[vl.size()-1].GetString(), MultiValues[Index].tag, MultiValues[Index].items[valuenum], allow_wild);
						}
						break;
						case DT_CHARPTR:
						{
							char item[MAXBUF];
							if (ConfValue(this->config_data, MultiValues[Index].tag, MultiValues[Index].items[valuenum], MultiValues[Index].items_default[valuenum], tagnum, item, MAXBUF, allow_newlines))
								vl.push_back(ValueItem(item));
							else
								vl.push_back(ValueItem(""));
						}
						break;
						case DT_INTEGER:
						{
							int item = 0;
							if (ConfValueInteger(this->config_data, MultiValues[Index].tag, MultiValues[Index].items[valuenum], MultiValues[Index].items_default[valuenum], tagnum, item))
								vl.push_back(ValueItem(item));
							else
								vl.push_back(ValueItem(0));
						}
						break;
						case DT_BOOLEAN:
						{
							bool item = ConfValueBool(this->config_data, MultiValues[Index].tag, MultiValues[Index].items[valuenum], MultiValues[Index].items_default[valuenum], tagnum);
							vl.push_back(ValueItem(item));
						}
						break;
						default:
							/* Someone was smoking craq if we got here, and we're all gonna die. */
						break;
					}
				}
					MultiValues[Index].validation_function(this, MultiValues[Index].tag, (char**)MultiValues[Index].items, vl, MultiValues[Index].datatype);
			}
				MultiValues[Index].finish_function(this, MultiValues[Index].tag);
		}

	}

	catch (CoreException &ce)
	{
		ReportConfigError(ce.GetReason(), bail, user);
		return;
	}

	/** XXX END PASS **/
	ServerInstance->Log(DEBUG,"End config pass %d", pass);

	if (pass == 0)
	{
		/* FIRST PASS: Set up commands, load modules.
		 * We cannot gaurantee that all config is correct
		 * at this point
		 */

		if (pass == 0)
		{
			if (isatty(0) && isatty(1) && isatty(2))
				printf("Downloading configuration ");

			TotalDownloaded = 0;
			FileErrors = 0;
		}

	        /** Note: This is safe, the method checks for user == NULL */
		ServerInstance->Modules->LoadAll();
	}
	else
	{
		/* SECOND PASS: Call modules to read configs, finalize
		 * stuff. Check that we have at least the required number
		 * of whichever items. This is no longer done first.
		 */
		ConfigReader* n = new ConfigReader(ServerInstance);
		FOREACH_MOD(I_OnReadConfig,OnReadConfig(this, n));

		for (int Index = 0; Once[Index]; Index++)
			if (!CheckOnce(Once[Index]))
				return;
	}

	// write once here, to try it out and make sure its ok
	ServerInstance->WritePID(this->PID);

	ServerInstance->Log(DEFAULT,"Done reading configuration file.");

	/* If we're rehashing, let's load any new modules, and unload old ones
	 */
	if (!bail)
	{
		int found_ports = 0;
		FailedPortList pl;
		ServerInstance->BindPorts(false, found_ports, pl);

		if (pl.size() && user)
		{
//			user->WriteServ("NOTICE %s :*** Not all your client ports could be bound.", user->nick);
//			user->WriteServ("NOTICE %s :*** The following port(s) failed to bind:", user->nick);
//			int j = 1;
//			for (FailedPortList::iterator i = pl.begin(); i != pl.end(); i++, j++)
//			{
//				user->WriteServ("NOTICE %s :*** %d.   IP: %s     Port: %lu", user->nick, j, i->first.empty() ? "<all>" : i->first.c_str(), (unsigned long)i->second);
//			}
		}
	}

	if (!removed_modules.empty())
	{
		for (std::vector<std::string>::iterator removing = removed_modules.begin(); removing != removed_modules.end(); removing++)
		{
			if (ServerInstance->Modules->Unload(removing->c_str()))
			{
//				ServerInstance->WriteOpers("*** REHASH UNLOADED MODULE: %s",removing->c_str());
//				if (user)
//					user->WriteServ("973 %s %s :Module %s successfully unloaded.",user->nick, removing->c_str(), removing->c_str());
				rem++;
			}
			else
			{
//				if (user)
//					user->WriteServ("972 %s %s :%s",user->nick, removing->c_str(), ServerInstance->Modules->LastError().c_str());
			}
		}
	}

	if (!added_modules.empty())
	{
		for (std::vector<std::string>::iterator adding = added_modules.begin(); adding != added_modules.end(); adding++)
		{
			/* Skip over modules that are aleready loaded for some reason */
			if (ServerInstance->Modules->Find(*adding))
				continue;

			if (bail)
				printf_c("[\033[1;32m*\033[0m] Loading module:\t\033[1;32m%s\033[0m\n", adding->c_str());

			if (ServerInstance->Modules->Load(adding->c_str()))
			{
//				if (user)
//					user->WriteServ("975 %s %s :Module %s successfully loaded.",user->nick, adding->c_str(), adding->c_str());

				add++;
			}
			else
			{
//				if (user)
//					user->WriteServ("974 %s %s :%s",user->nick, adding->c_str(), ServerInstance->Modules->LastError().c_str());

				if (bail)
				{
					printf_c("\n[\033[1;31m*\033[0m] %s\n\n", ServerInstance->Modules->LastError().c_str());
					ServerInstance->Exit(EXIT_STATUS_MODULE);
				}
			}
		}
	}

	ServerInstance->Log(DEFAULT,"Successfully unloaded %lu of %lu modules and loaded %lu of %lu modules.",(unsigned long)rem,(unsigned long)removed_modules.size(),(unsigned long)add,(unsigned long)added_modules.size());
}

/* XXX: This can and will block! */
void ServerConfig::DoDownloads()
{
	ServerInstance->Log(DEBUG,"In DoDownloads()");

	/* Reads all local files into the IncludedFiles map, then initiates sockets for the remote ones */
	for (std::map<std::string, std::istream*>::iterator x = IncludedFiles.begin(); x != IncludedFiles.end(); ++x)
	{
		if (CompletedFiles.find(x->first) != CompletedFiles.end())
			continue;

		ServerInstance->Log(DEBUG,"StartDownloads File: %s", x->first.c_str());

		std::string file = x->first;
		if ((file[0] == '/') || (file.substr(0, 7) == "file://"))
		{
			/* For file:// schema files, we use std::ifstream which is a derivative of std::istream.
			 * For all other file schemas, we use a std::stringstream.
			 */

			/* Add our own ifstream */
			std::ifstream* conf = new std::ifstream(file.c_str());
			if (!conf->fail())
			{
				ServerInstance->Log(DEBUG,"file:// schema file %s loaded OK", file.c_str());
				delete x->second;
				x->second = conf;
			}
			else
			{
				delete x->second;
				x->second = NULL;
				FileErrors++;
			}
			TotalDownloaded++;
		}

		CompletedFiles[x->first] = true;
	}
}

bool ServerConfig::LoadConf(ConfigDataHash &target, const char* filename, std::ostringstream &errorstream, int pass, std::istream *scan_for_includes_only)
{
	std::string line;
	std::istream* conf = NULL;
	char ch;
	long linenumber;
	bool in_tag;
	bool in_quote;
	bool in_comment;
	int character_count = 0;

	linenumber = 1;
	in_tag = false;
	in_quote = false;
	in_comment = false;

	if (scan_for_includes_only)
	{
		ServerInstance->Log(DEBUG,"scan_for_includes_only set");
		conf = scan_for_includes_only;
	}

	if (std::string(filename) == CONFIG_FILE)
	{
		if (!scan_for_includes_only)
		{
			conf = new std::ifstream(filename);
			if (conf->fail())
			{
				errorstream << "File " << filename << " could not be opened." << std::endl;
				return false;
			}
		}
	}
	else
	{
		std::map<std::string, std::istream*>::iterator x = IncludedFiles.find(filename);
		if (x == IncludedFiles.end())
		{
			if (pass == 0)
			{
				ServerInstance->Log(DEBUG,"Push include file %s onto map", filename);
				/* First pass, we insert the file into a map, and just return true */
				IncludedFiles.insert(std::make_pair(filename,new std::stringstream));
				return true;
			}
			else
			{
				/* Second pass, look for the file in the map */
				ServerInstance->Log(DEBUG,"We are in the second pass, and %s is not in the map!", filename);
				errorstream << "File " << filename << " could not be opened." << std::endl;
				return false;
			}
		}
		else
		{
			if (!scan_for_includes_only)
			{
				if (x->second)
					conf = IncludedFiles.find(filename)->second;
				else
				{
					errorstream << "File " << filename << " could not be opened." << std::endl;
					return false;
				}
			}
		}
	}

	ServerInstance->Log(DEBUG,"Start to read conf %s %08lx", filename, conf);

	/* Start reading characters... */
	while (conf->get(ch))
	{

		/*
		 * Fix for moronic windows issue spotted by Adremelech.
		 * Some windows editors save text files as utf-16, which is
		 * a total pain in the ass to parse. Users should save in the
		 * right config format! If we ever see a file where the first
		 * byte is 0xFF or 0xFE, or the second is 0xFF or 0xFE, then
		 * this is most likely a utf-16 file. Bail out and insult user.
		 */
		if ((character_count++ < 2) && (ch == '\xFF' || ch == '\xFE'))
		{
			errorstream << "File " << filename << " cannot be read, as it is encoded in braindead UTF-16. Save your file as plain ASCII!" << std::endl;
			if (!scan_for_includes_only)
				delete conf;
			return false;
		}

		/*
		 * Here we try and get individual tags on separate lines,
		 * this would be so easy if we just made people format
		 * their config files like that, but they don't so...
		 * We check for a '<' and then know the line is over when
		 * we get a '>' not inside quotes. If we find two '<' and
		 * no '>' then die with an error.
		 */

		if ((ch == '#') && !in_quote)
			in_comment = true;

		switch (ch)
		{
			case '\n':
				if (in_quote)
					line += '\n';
				linenumber++;
			case '\r':
				if (!in_quote)
					in_comment = false;
			case '\0':
				continue;
			case '\t':
				ch = ' ';
		}

		if(in_comment)
			continue;

		/* XXX: Added by Brain, May 1st 2006 - Escaping of characters.
		 * Note that this WILL NOT usually allow insertion of newlines,
		 * because a newline is two characters long. Use it primarily to
		 * insert the " symbol.
		 *
		 * Note that this also involves a further check when parsing the line,
		 * which can be found below.
		 */
		if ((ch == '\\') && (in_quote) && (in_tag))
		{
			line += ch;
			char real_character;
			if (conf->get(real_character))
			{
				if (real_character == 'n')
					real_character = '\n';
				line += real_character;
				continue;
			}
			else
			{
				errorstream << "End of file after a \\, what did you want to escape?: " << filename << ":" << linenumber << std::endl;
				if (!scan_for_includes_only)
					delete conf;
				return false;
			}
		}

		if (ch != '\r')
			line += ch;

		if (ch == '<')
		{
			if (in_tag)
			{
				if (!in_quote)
				{
					errorstream << "Got another opening < when the first one wasn't closed: " << filename << ":" << linenumber << std::endl;
					if (!scan_for_includes_only)
						delete conf;
					return false;
				}
			}
			else
			{
				if (in_quote)
				{
					errorstream << "We're in a quote but outside a tag, interesting. " << filename << ":" << linenumber << std::endl;
					if (!scan_for_includes_only)
						delete conf;
					return false;
				}
				else
				{
					// errorstream << "Opening new config tag on line " << linenumber << std::endl;
					in_tag = true;
				}
			}
		}
		else if (ch == '"')
		{
			if (in_tag)
			{
				if (in_quote)
				{
					// errorstream << "Closing quote in config tag on line " << linenumber << std::endl;
					in_quote = false;
				}
				else
				{
					// errorstream << "Opening quote in config tag on line " << linenumber << std::endl;
					in_quote = true;
				}
			}
			else
			{
				if (in_quote)
				{
					errorstream << "Found a (closing) \" outside a tag: " << filename << ":" << linenumber << std::endl;
				}
				else
				{
					errorstream << "Found a (opening) \" outside a tag: " << filename << ":" << linenumber << std::endl;
				}
			}
		}
		else if (ch == '>')
		{
			if (!in_quote)
			{
				if (in_tag)
				{
					// errorstream << "Closing config tag on line " << linenumber << std::endl;
					in_tag = false;

					/*
					 * If this finds an <include> then ParseLine can simply call
					 * LoadConf() and load the included config into the same ConfigDataHash
					 */

					if (!this->ParseLine(target, line, linenumber, errorstream, pass, scan_for_includes_only))
					{
						if (!scan_for_includes_only)
							delete conf;
						return false;
					}

					line.clear();
				}
				else
				{
					errorstream << "Got a closing > when we weren't inside a tag: " << filename << ":" << linenumber << std::endl;
					if (!scan_for_includes_only)
						delete conf;
					return false;
				}
			}
		}
	}

	/* Fix for bug #392 - if we reach the end of a file and we are still in a quote or comment, most likely the user fucked up */
	if (in_comment || in_quote)
	{
		errorstream << "Reached end of file whilst still inside a quoted section or tag. This is most likely an error or there \
			is a newline missing from the end of the file: " << filename << ":" << linenumber << std::endl;
	}

	if (!scan_for_includes_only)
		delete conf;
	return true;
}

bool ServerConfig::LoadConf(ConfigDataHash &target, const std::string &filename, std::ostringstream &errorstream, int pass, std::istream* scan_for_includs_only)
{
	return this->LoadConf(target, filename.c_str(), errorstream, pass, scan_for_includs_only);
}

bool ServerConfig::ParseLine(ConfigDataHash &target, std::string &line, long &linenumber, std::ostringstream &errorstream, int pass, std::istream* scan_for_includes_only)
{
	std::string tagname;
	std::string current_key;
	std::string current_value;
	KeyValList results;
	bool got_name;
	bool got_key;
	bool in_quote;

	got_name = got_key = in_quote = false;

	for(std::string::iterator c = line.begin(); c != line.end(); c++)
	{
		if (!got_name)
		{
			/* We don't know the tag name yet. */

			if (*c != ' ')
			{
				if (*c != '<')
				{
					tagname += *c;
				}
			}
			else
			{
				/* We got to a space, we should have the tagname now. */
				if(tagname.length())
				{
					got_name = true;
				}
			}
		}
		else
		{
			/* We have the tag name */
			if (!got_key)
			{
				/* We're still reading the key name */
				if (*c != '=')
				{
					if (*c != ' ')
					{
						current_key += *c;
					}
				}
				else
				{
					/* We got an '=', end of the key name. */
					got_key = true;
				}
			}
			else
			{
				/* We have the key name, now we're looking for quotes and the value */

				/* Correctly handle escaped characters here.
				 * See the XXX'ed section above.
				 */
				if ((*c == '\\') && (in_quote))
				{
					c++;
					if (*c == 'n')
						current_value += '\n';
					else
						current_value += *c;
					continue;
				}
				else if ((*c == '\n') && (in_quote))
				{
					/* Got a 'real' \n, treat it as part of the value */
					current_value += '\n';
					linenumber++;
					continue;
				}
				else if ((*c == '\r') && (in_quote))
					/* Got a \r, drop it */
					continue;

				if (*c == '"')
				{
					if (!in_quote)
					{
						/* We're not already in a quote. */
						in_quote = true;
					}
					else
					{
						/* Leaving quotes, we have the value */
						if (!scan_for_includes_only)
							results.push_back(KeyVal(current_key, current_value));

						// std::cout << "<" << tagname << ":" << current_key << "> " << current_value << std::endl;

						in_quote = false;
						got_key = false;

						if ((tagname == "include") && (current_key == "file"))
						{	
							if (!this->DoInclude(target, current_value, errorstream, pass, scan_for_includes_only))
								return false;
						}

						current_key.clear();
						current_value.clear();
					}
				}
				else
				{
					if (in_quote)
					{
						current_value += *c;
					}
				}
			}
		}
	}

	/* Finished parsing the tag, add it to the config hash */
	if (!scan_for_includes_only)
		target.insert(std::pair<std::string, KeyValList > (tagname, results));

	return true;
}

bool ServerConfig::DoInclude(ConfigDataHash &target, const std::string &file, std::ostringstream &errorstream, int pass, std::istream* scan_for_includes_only)
{
	std::string confpath;
	std::string newfile;
	std::string::size_type pos;

	confpath = ServerInstance->ConfigFileName;
	newfile = file;

	std::replace(newfile.begin(),newfile.end(),'\\','/');
	std::replace(confpath.begin(),confpath.end(),'\\','/');

	if ((newfile[0] != '/') && (newfile.find("://") == std::string::npos))
	{
		if((pos = confpath.rfind("/")) != std::string::npos)
		{
			/* Leaves us with just the path */
			newfile = confpath.substr(0, pos) + std::string("/") + newfile;
		}
		else
		{
			errorstream << "Couldn't get config path from: " << ServerInstance->ConfigFileName << std::endl;
			return false;
		}
	}

	return LoadConf(target, newfile, errorstream, pass, scan_for_includes_only);
}

bool ServerConfig::ConfValue(ConfigDataHash &target, const char* tag, const char* var, int index, char* result, int length, bool allow_linefeeds)
{
	return ConfValue(target, tag, var, "", index, result, length, allow_linefeeds);
}

bool ServerConfig::ConfValue(ConfigDataHash &target, const char* tag, const char* var, const char* default_value, int index, char* result, int length, bool allow_linefeeds)
{
	std::string value;
	bool r = ConfValue(target, std::string(tag), std::string(var), std::string(default_value), index, value, allow_linefeeds);
	strlcpy(result, value.c_str(), length);
	return r;
}

bool ServerConfig::ConfValue(ConfigDataHash &target, const std::string &tag, const std::string &var, int index, std::string &result, bool allow_linefeeds)
{
	return ConfValue(target, tag, var, "", index, result, allow_linefeeds);
}

bool ServerConfig::ConfValue(ConfigDataHash &target, const std::string &tag, const std::string &var, const std::string &default_value, int index, std::string &result, bool allow_linefeeds)
{
	ConfigDataHash::size_type pos = index;
	if (pos < target.count(tag))
	{
		ConfigDataHash::iterator iter = target.find(tag);

		for(int i = 0; i < index; i++)
			iter++;

		for(KeyValList::iterator j = iter->second.begin(); j != iter->second.end(); j++)
		{
			if(j->first == var)
			{
 				if ((!allow_linefeeds) && (j->second.find('\n') != std::string::npos))
				{
					ServerInstance->Log(DEFAULT, "Value of <" + tag + ":" + var+ "> contains a linefeed, and linefeeds in this value are not permitted -- stripped to spaces.");
					for (std::string::iterator n = j->second.begin(); n != j->second.end(); n++)
						if (*n == '\n')
							*n = ' ';
				}
				else
				{
					result = j->second;
					return true;
				}
			}
		}
		if (!default_value.empty())
		{
			result = default_value;
			return true;
		}
	}
	else if(pos == 0)
	{
		if (!default_value.empty())
		{
			result = default_value;
			return true;
		}
	}
	return false;
}

bool ServerConfig::ConfValueInteger(ConfigDataHash &target, const char* tag, const char* var, int index, int &result)
{
	return ConfValueInteger(target, std::string(tag), std::string(var), "", index, result);
}

bool ServerConfig::ConfValueInteger(ConfigDataHash &target, const char* tag, const char* var, const char* default_value, int index, int &result)
{
	return ConfValueInteger(target, std::string(tag), std::string(var), std::string(default_value), index, result);
}

bool ServerConfig::ConfValueInteger(ConfigDataHash &target, const std::string &tag, const std::string &var, int index, int &result)
{
	return ConfValueInteger(target, tag, var, "", index, result);
}

bool ServerConfig::ConfValueInteger(ConfigDataHash &target, const std::string &tag, const std::string &var, const std::string &default_value, int index, int &result)
{
	std::string value;
	std::istringstream stream;
	bool r = ConfValue(target, tag, var, default_value, index, value);
	stream.str(value);
	if(!(stream >> result))
		return false;
	else
	{
		if (!value.empty())
		{
			if (value.substr(0,2) == "0x")
			{
				char* endptr;

				value.erase(0,2);
				result = strtol(value.c_str(), &endptr, 16);

				/* No digits found */
				if (endptr == value.c_str())
					return false;
			}
			else
			{
				char denominator = *(value.end() - 1);
				switch (toupper(denominator))
				{
					case 'K':
						/* Kilobytes -> bytes */
						result = result * 1024;
					break;
					case 'M':
						/* Megabytes -> bytes */
						result = result * 1024 * 1024;
					break;
					case 'G':
						/* Gigabytes -> bytes */
						result = result * 1024 * 1024 * 1024;
					break;
				}
			}
		}
	}
	return r;
}


bool ServerConfig::ConfValueBool(ConfigDataHash &target, const char* tag, const char* var, int index)
{
	return ConfValueBool(target, std::string(tag), std::string(var), "", index);
}

bool ServerConfig::ConfValueBool(ConfigDataHash &target, const char* tag, const char* var, const char* default_value, int index)
{
	return ConfValueBool(target, std::string(tag), std::string(var), std::string(default_value), index);
}

bool ServerConfig::ConfValueBool(ConfigDataHash &target, const std::string &tag, const std::string &var, int index)
{
	return ConfValueBool(target, tag, var, "", index);
}

bool ServerConfig::ConfValueBool(ConfigDataHash &target, const std::string &tag, const std::string &var, const std::string &default_value, int index)
{
	std::string result;
	if(!ConfValue(target, tag, var, default_value, index, result))
		return false;

	return ((result == "yes") || (result == "true") || (result == "1"));
}

int ServerConfig::ConfValueEnum(ConfigDataHash &target, const char* tag)
{
	return target.count(tag);
}

int ServerConfig::ConfValueEnum(ConfigDataHash &target, const std::string &tag)
{
	return target.count(tag);
}

int ServerConfig::ConfVarEnum(ConfigDataHash &target, const char* tag, int index)
{
	return ConfVarEnum(target, std::string(tag), index);
}

int ServerConfig::ConfVarEnum(ConfigDataHash &target, const std::string &tag, int index)
{
	ConfigDataHash::size_type pos = index;

	if (pos < target.count(tag))
	{
		ConfigDataHash::const_iterator iter = target.find(tag);

		for(int i = 0; i < index; i++)
			iter++;

		return iter->second.size();
	}

	return 0;
}

/** Read the contents of a file located by `fname' into a file_cache pointed at by `F'.
 */
bool ServerConfig::ReadFile(file_cache &F, const char* fname)
{
	if (!fname || !*fname)
		return false;

	FILE* file = NULL;
	char linebuf[MAXBUF];

	F.clear();

	if ((*fname != '/') && (*fname != '\\'))
	{
		std::string::size_type pos;
		std::string confpath = ServerInstance->ConfigFileName;
		std::string newfile = fname;

		if ((pos = confpath.rfind("/")) != std::string::npos)
			newfile = confpath.substr(0, pos) + std::string("/") + fname;
		else if ((pos = confpath.rfind("\\")) != std::string::npos)
			newfile = confpath.substr(0, pos) + std::string("\\") + fname;

		if (!FileExists(newfile.c_str()))
			return false;
		file =  fopen(newfile.c_str(), "r");
	}
	else
	{
		if (!FileExists(fname))
			return false;
		file =  fopen(fname, "r");
	}

	if (file)
	{
		while (!feof(file))
		{
			if (fgets(linebuf, sizeof(linebuf), file))
				linebuf[strlen(linebuf)-1] = 0;
			else
				*linebuf = 0;

			if (!feof(file))
			{
				F.push_back(*linebuf ? linebuf : " ");
			}
		}

		fclose(file);
	}
	else
		return false;

	return true;
}

bool ServerConfig::FileExists(const char* file)
{
	struct stat sb;
	if (stat(file, &sb) == -1)
		return false;

	if ((sb.st_mode & S_IFDIR) > 0)
		return false;
	     
	FILE *input;
	if ((input = fopen (file, "r")) == NULL)
		return false;
	else
	{
		fclose(input);
		return true;
	}
}

char* ServerConfig::CleanFilename(char* name)
{
	char* p = name + strlen(name);
	while ((p != name) && (*p != '/') && (*p != '\\')) p--;
	return (p != name ? ++p : p);
}


bool ServerConfig::DirValid(const char* dirandfile)
{
#ifdef WINDOWS
	return true;
#endif

	char work[1024];
	char buffer[1024];
	char otherdir[1024];
	int p;

	strlcpy(work, dirandfile, 1024);
	p = strlen(work);

	// we just want the dir
	while (*work)
	{
		if (work[p] == '/')
		{
			work[p] = '\0';
			break;
		}

		work[p--] = '\0';
	}

	// Get the current working directory
	if (getcwd(buffer, 1024 ) == NULL )
		return false;

	if (chdir(work) == -1)
		return false;

	if (getcwd(otherdir, 1024 ) == NULL )
		return false;

	if (chdir(buffer) == -1)
		return false;

	size_t t = strlen(work);

	if (strlen(otherdir) >= t)
	{
		otherdir[t] = '\0';
		if (!strcmp(otherdir,work))
		{
			return true;
		}

		return false;
	}
	else
	{
		return false;
	}
}

std::string ServerConfig::GetFullProgDir()
{
	char buffer[PATH_MAX+1];
#ifdef WINDOWS
	/* Windows has specific api calls to get the exe path that never fail.
	 * For once, windows has something of use, compared to the POSIX code
	 * for this, this is positively neato.
	 */
	if (GetModuleFileName(NULL, buffer, MAX_PATH))
	{
		std::string fullpath = buffer;
		std::string::size_type n = fullpath.rfind("\\inspircd.exe");
		return std::string(fullpath, 0, n);
	}
#else
	// Get the current working directory
	if (getcwd(buffer, PATH_MAX))
	{
		std::string remainder = this->argv[0];

		/* Does argv[0] start with /? its a full path, use it */
		if (remainder[0] == '/')
		{
			std::string::size_type n = remainder.rfind("/inspircd");
			return std::string(remainder, 0, n);
		}

		std::string fullpath = std::string(buffer) + "/" + remainder;
		std::string::size_type n = fullpath.rfind("/inspircd");
		return std::string(fullpath, 0, n);
	}
#endif
	return "/";
}

InspIRCd* ServerConfig::GetInstance()
{
	return ServerInstance;
}

ValueItem::ValueItem(int value)
{
	std::stringstream n;
	n << value;
	v = n.str();
}

ValueItem::ValueItem(bool value)
{
	std::stringstream n;
	n << value;
	v = n.str();
}

ValueItem::ValueItem(char* value)
{
	v = value;
}

void ValueItem::Set(char* value)
{
	v = value;
}

void ValueItem::Set(const char* value)
{
	v = value;
}

void ValueItem::Set(int value)
{
	std::stringstream n;
	n << value;
	v = n.str();
}

int ValueItem::GetInteger()
{
	if (v.empty())
		return 0;
	return atoi(v.c_str());
}

char* ValueItem::GetString()
{
	return (char*)v.c_str();
}

bool ValueItem::GetBool()
{
	return (GetInteger() || v == "yes" || v == "true");
}


