
#ifndef __BACKEND_H__
#define __BACKEND_H__

class CoreExport Backend : public classbase
{
 public:
	InspIRCd *ServerInstance;

	Backend(InspIRCd *Instance) : ServerInstance(Instance)
	{
	}

	~Backend()
	{
	}

	virtual bool Request(const std::string &uri)
	{
		return false;
	}

	virtual bool IsEOF()
	{
		return true;
	}

	virtual std::string Fetch()
	{
		return std::string("");
	}
};

// XXX these should have a manager class, and be in "modules" of a sort.
class FOpenBackend : public Backend
{
	FILE *fd;

 public:
	FOpenBackend(InspIRCd *Instance) : Backend(Instance)
	{
		fd = NULL;
	}

	~FOpenBackend()
	{
	}

	bool Request(std::string &path)
	{
		if (path.empty())
			return false; // XXX find index file, use instead (XXX should be done prior to the backend!)

		fd = fopen(path.c_str(), "r");
		if (fd == NULL)
			return false;

		return true;
	}

	bool IsEOF()
	{
		return (feof(fd) == 1 ? true : false);
	}

	// XXX this should return pointer or reference, copying the data is bad...
	std::string Fetch()
	{
		std::string sbuf = "";
		char buf[MAXBUF];
		int len = 0;

		while (!this->IsEOF())
		{
			len = fread(buf, 1, MAXBUF - 1, fd);
			//buf[len] = '\0'; // null term
			sbuf.append(buf, len);
		}

		fclose(fd);
		ServerInstance->Log(DEBUG, "FOpen: %d bytes ", sbuf.length());

		return sbuf;
	}
};

#endif
