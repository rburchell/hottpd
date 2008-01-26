#ifndef __BACKEND_H__
#define __BACKEND_H__

class CoreExport Backend : public classbase
{
 protected:
	InspIRCd *ServerInstance;
	
 public:
	Backend(InspIRCd *Server) : ServerInstance(Server)
	{
	}

	virtual ~Backend()
	{
	}
	
	virtual int ServeFile(int sockfd, int filefd, off_t &sent, off_t filesize) = 0;
};

class WriteBackend : public Backend
{
 protected:
	static WriteBackend *Instance;
	
	WriteBackend(InspIRCd *Server) : Backend(Server) { }
	
 public:
	static WriteBackend *GetInstance(InspIRCd *Server)
	{
		return (!Instance) ? (Instance = new WriteBackend(Server)) : Instance;
	}
	
	virtual ~WriteBackend()
	{
	}
	
	virtual int ServeFile(int sockfd, int filefd, off_t &sent, off_t filesize);
};

#endif
