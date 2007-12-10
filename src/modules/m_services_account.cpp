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

#include "inspircd.h"

/* $ModDesc: Povides support for ircu-style services accounts, including chmode +R, etc. */

/** Channel mode +R - unidentified users cannot join
 */
class AChannel_R : public ModeHandler
{
 public:
	AChannel_R(InspIRCd* Instance) : ModeHandler(Instance, 'R', 0, 0, false, MODETYPE_CHANNEL, false) { }

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			if (!channel->IsModeSet('R'))
			{
				channel->SetMode('R',true);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (channel->IsModeSet('R'))
			{
				channel->SetMode('R',false);
				return MODEACTION_ALLOW;
			}
		}

		return MODEACTION_DENY;
	}
};

/** User mode +R - unidentified users cannot message
 */
class AUser_R : public ModeHandler
{
 public:
	AUser_R(InspIRCd* Instance) : ModeHandler(Instance, 'R', 0, 0, false, MODETYPE_USER, false) { }

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			if (!dest->IsModeSet('R'))
			{
				dest->SetMode('R',true);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (dest->IsModeSet('R'))
			{
				dest->SetMode('R',false);
				return MODEACTION_ALLOW;
			}
		}

		return MODEACTION_DENY;
	}
};

/** Channel mode +M - unidentified users cannot message channel
 */
class AChannel_M : public ModeHandler
{
 public:
	AChannel_M(InspIRCd* Instance) : ModeHandler(Instance, 'M', 0, 0, false, MODETYPE_CHANNEL, false) { }

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			if (!channel->IsModeSet('M'))
			{
				channel->SetMode('M',true);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (channel->IsModeSet('M'))
			{
				channel->SetMode('M',false);
				return MODEACTION_ALLOW;
			}
		}

		return MODEACTION_DENY;
	}
};

class ModuleServicesAccount : public Module
{
	 
	AChannel_R* m1;
	AChannel_M* m2;
	AUser_R* m3;
 public:
	ModuleServicesAccount(InspIRCd* Me) : Module(Me)
	{
		
		m1 = new AChannel_R(ServerInstance);
		m2 = new AChannel_M(ServerInstance);
		m3 = new AUser_R(ServerInstance);
		if (!ServerInstance->AddMode(m1) || !ServerInstance->AddMode(m2) || !ServerInstance->AddMode(m3))
			throw ModuleException("Could not add new modes!");

		Implementation eventlist[] = { I_OnWhois, I_OnUserPreMessage, I_OnUserPreNotice, I_OnUserPreJoin,
			I_OnSyncUserMetaData, I_OnUserQuit, I_OnCleanup, I_OnDecodeMetaData };

		ServerInstance->Modules->Attach(eventlist, this, 8);
	}

	/* <- :twisted.oscnet.org 330 w00t2 w00t2 w00t :is logged in as */
	virtual void OnWhois(User* source, User* dest)
	{
		std::string *account;
		dest->GetExt("accountname", account);

		if (account)
		{
			ServerInstance->SendWhoisLine(source, dest, 330, "%s %s %s :is logged in as", source->nick, dest->nick, account->c_str());
		}
	}


	virtual int OnUserPreMessage(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		std::string *account;

		if (!IS_LOCAL(user))
			return 0;

		user->GetExt("accountname", account);
		
		if (target_type == TYPE_CHANNEL)
		{
			Channel* c = (Channel*)dest;
			
			if ((c->IsModeSet('M')) && (!account))
			{
				if ((ServerInstance->ULine(user->nick)) || (ServerInstance->ULine(user->server)))
				{
					// user is ulined, can speak regardless
					return 0;
				}

				// user messaging a +M channel and is not registered
				user->WriteServ("477 "+std::string(user->nick)+" "+std::string(c->name)+" :You need to be identified to a registered account to message this channel");
				return 1;
			}
		}
		if (target_type == TYPE_USER)
		{
			User* u = (User*)dest;
			
			if ((u->modes['R'-65]) && (!account))
			{
				if ((ServerInstance->ULine(user->nick)) || (ServerInstance->ULine(user->server)))
				{
					// user is ulined, can speak regardless
					return 0;
				}

				// user messaging a +R user and is not registered
				user->WriteServ("477 "+std::string(user->nick)+" "+std::string(u->nick)+" :You need to be identified to a registered account to message this user");
				return 1;
			}
		}
		return 0;
	}
	 
	virtual int OnUserPreNotice(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		return OnUserPreMessage(user, dest, target_type, text, status, exempt_list);
	}
	 
	virtual int OnUserPreJoin(User* user, Channel* chan, const char* cname, std::string &privs)
	{
		std::string *account;
		user->GetExt("accountname", account);
		
		if (chan)
		{
			if (chan->IsModeSet('R'))
			{
				if (!account)
				{
					if ((ServerInstance->ULine(user->nick)) || (ServerInstance->ULine(user->server)))
					{
						// user is ulined, won't be stopped from joining
						return 0;
					}
					// joining a +R channel and not identified
					user->WriteServ("477 "+std::string(user->nick)+" "+std::string(chan->name)+" :You need to be identified to a registered account to join this channel");
					return 1;
				}
			}
		}
		return 0;
	}
	
	// Whenever the linking module wants to send out data, but doesnt know what the data
	// represents (e.g. it is metadata, added to a User or Channel by a module) then
	// this method is called. We should use the ProtoSendMetaData function after we've
	// corrected decided how the data should look, to send the metadata on its way if
	// it is ours.
	virtual void OnSyncUserMetaData(User* user, Module* proto, void* opaque, const std::string &extname, bool displayable)
	{
		// check if the linking module wants to know about OUR metadata
		if (extname == "accountname")
		{
			// check if this user has an swhois field to send
			std::string* account;
			user->GetExt("accountname", account);
			if (account)
			{
				// remove any accidental leading/trailing spaces
				trim(*account);

				// call this function in the linking module, let it format the data how it
				// sees fit, and send it on its way. We dont need or want to know how.
				proto->ProtoSendMetaData(opaque,TYPE_USER,user,extname,*account);
			}
		}
	}

	// when a user quits, tidy up their metadata
	virtual void OnUserQuit(User* user, const std::string &message, const std::string &oper_message)
	{
		std::string* account;
		user->GetExt("accountname", account);
		if (account)
		{
			user->Shrink("accountname");
			delete account;
		}
	}

	// if the module is unloaded, tidy up all our dangling metadata
	virtual void OnCleanup(int target_type, void* item)
	{
		if (target_type == TYPE_USER)
		{
			User* user = (User*)item;
			std::string* account;
			user->GetExt("accountname", account);
			if (account)
			{
				user->Shrink("accountname");
				delete account;
			}
		}
	}

	// Whenever the linking module receives metadata from another server and doesnt know what
	// to do with it (of course, hence the 'meta') it calls this method, and it is up to each
	// module in turn to figure out if this metadata key belongs to them, and what they want
	// to do with it.
	// In our case we're only sending a single string around, so we just construct a std::string.
	// Some modules will probably get much more complex and format more detailed structs and classes
	// in a textual way for sending over the link.
	virtual void OnDecodeMetaData(int target_type, void* target, const std::string &extname, const std::string &extdata)
	{
		// check if its our metadata key, and its associated with a user
		if ((target_type == TYPE_USER) && (extname == "accountname"))
		{	
			User* dest = (User*)target;
			
			/* logging them out? */
			if (extdata.empty())
			{
				std::string* account;
				dest->GetExt("accountname", account);
				if (account)
				{
					dest->Shrink("accountname");
					delete account;
				}
			}
			else
			{
				// if they dont already have an accountname field, accept the remote server's
				std::string* text;
				if (!dest->GetExt("accountname", text))
				{
					text = new std::string(extdata);
					// remove any accidental leading/trailing spaces
					trim(*text);
					dest->Extend("accountname", text);
				}
			}
		}
	}

	virtual ~ModuleServicesAccount()
	{
		ServerInstance->Modes->DelMode(m1);
		ServerInstance->Modes->DelMode(m2);
		ServerInstance->Modes->DelMode(m3);
		delete m1;
		delete m2;
		delete m3;
	}
	
	virtual Version GetVersion()
	{
		return Version(1,1,0,0,VF_COMMON|VF_VENDOR,API_VERSION);
	}
};

MODULE_INIT(ModuleServicesAccount)
