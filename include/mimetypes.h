
#ifndef __MIMETYPES_H__
#define __MIMETYPES_H__

class CoreExport MimeManager : public classbase
{
	std::map<std::string, std::string>MimeTypes;
 public:
	InspIRCd *ServerInstance;

	MimeManager(InspIRCd *Instance) : ServerInstance(Instance)
	{
	}

	~MimeManager()
	{
	}

	void AddType(const std::string &ext, const std::string &type);

	const std::string GetType(const std::string &ext);
};

#endif
