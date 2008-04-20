/*
 *   hottpd - a fast, extensible, featureful http server
 *          (C) 2007-2008 hottpd development team
 *
 * Based on InspIRCd - (C) 2002-2007 InspIRCd Development Team
 *
 *    This program is free but copyrighted software; see
 *              the file COPYING for details.
 *
 */

/* $Core: libhttpd_request */

#include "inspircd.h"
#include <stdarg.h>
#include "socketengine.h"
#include "wildcard.h"

void Connection::CheckRequest(int newpos)
{
	/* A HTTP request ends with a blank header, i.e. \r\n\r\n. We're trying first
	 * to search for this sequence, but we only search in new data (and the last
	 * three bytes of old data) to save time.
	 *
	 * rfind can't be used, as it would break pipelining.
	 */
	
	std::string::size_type reqend = requestbuf.find("\r\n\r\n", (newpos >= 3) ? (newpos - 3) : 0);
	if (reqend == std::string::npos)
		return;

	ServerInstance->Log(DEBUG, "Got headers.");
	
	/* BIG HUGE WARNING THAT YOU SHOULD READ BEFORE YOU MAKE AN INFINITE LOOP!
	 *
	 * Erase from the requestbuf before you call anything like SendError, etc.
	 * Not doing so has very unfortunate consequences, k?
	 */
	
	std::string::size_type hbegin = 0, hend;
	for (; (hend = requestbuf.find("\r\n", hbegin)) != std::string::npos; hbegin = hend + 2)
	{
		if (hbegin == hend)
			break;
		
		if (method.empty())
		{
			std::istringstream cheader(std::string(requestbuf, hbegin, hend - hbegin));
			std::string tmpversion;
			cheader >> method;
			cheader >> uri;
			cheader >> tmpversion;

			if (method.empty() || uri.empty() || tmpversion.empty())
			{
				requestbuf.erase(0, reqend + 4);
				SendError(400, "Bad Request");
				return;
			}

			std::transform(tmpversion.begin(), tmpversion.end(), tmpversion.begin(), ::toupper);
			
			if (tmpversion == "HTTP/1.1")
			{
				http_version = HTTP_1_1;
			}
			else if (tmpversion == "HTTP/1.0")
			{
				http_version = HTTP_1_0;

				// HTTP 1.0 defaults to Connection: Close (but only set this on the first request of the connection)
				if (!RequestsCompleted)
					keepalive = false;
			}
			else
			{
				requestbuf.erase(0, reqend + 4);
				SendError(505, "Version Not Supported");
				return;
			}
		}
		else
		{
			std::string cheader = requestbuf.substr(hbegin, hend - hbegin);
	
			std::string::size_type fieldsep = cheader.find(':');
			if ((fieldsep == std::string::npos) || (fieldsep == 0) || (fieldsep == cheader.length() - 1))
			{
				requestbuf.erase(0, reqend + 4);
				SendError(400, "Bad Request");
				return;
			}
	
			headers.SetHeader(cheader.substr(0, fieldsep), cheader.substr(fieldsep + 2));
		}
	}

	requestbuf.erase(0, reqend + 4);

	// In the interest of convention, make the method uppercase
	std::transform(method.begin(), method.end(), method.begin(), ::toupper);
	
	// Important header checks for internal state and RFC compatibility
	if (strcasecmp(headers.GetHeader("Connection").c_str(), "close") == 0)
		keepalive = false;
	else if (strcasecmp(headers.GetHeader("Connection").c_str(), "keep-alive") == 0)
		keepalive = true;
	
	if ((RequestsCompleted + 1) >= ServerInstance->Config->KeepAliveMax)
	{
		ServerInstance->Log(DEBUG, "Closing connection after request due to keepalive limit");
		keepalive = false;
	}
	
	// Check for a request body
	if (headers.IsSet("Content-Length"))
	{
		RequestBodyLength = atoi(headers.GetHeader("Content-Length").c_str());

		if (RequestBodyLength > ServerInstance->Config->MaxPostBody)
		{
			// Sorry, lardy. Don't try send so much crap.
			keepalive = false;
			SendError(413, "Request Entity Too Large");
			return;
		}
	}
	
	if (headers.IsSet("Transfer-Encoding"))
	{
		ServerInstance->Log(DEBUG, "Transfer encoded request bodies are not yet supported");
		keepalive = false;
		SendError(500, "Internal Server Error");
		return;
	}
	
	if (RequestBodyLength > 0)
	{
		if (method != "POST")
		{
			// We currently only accept bodies for POST requests.
			ServerInstance->Log(DEBUG, "Rejecting non-POST request with a request body");
			// This will kill the connection, which we must do because the client will send the request body anyway.
			keepalive = false;
			SendError(500, "Internal Server Error");
			return;
		}
		
		ServerInstance->Log(DEBUG, "Reading %d bytes for the request body", RequestBodyLength);
		State = HTTP_RECV_REQBODY;
		return;
	}
	
	ServeData();
 }

// XXX does this belong here?
void Connection::HandleURI()
{
	// TODO absolute URLs (i.e. http://[HOST HEADER VALUE]/blah)
	if (uri[0] != '/')
		return;
	
	std::string sanepath;
	std::string pathpart;
	for (std::string::const_iterator i = uri.begin() + 1; ; ++i)
	{
		char c = (i == uri.end()) ? 0 : *i;
		
		if (c == '%')
		{
			// URL Encoded value
			if (((i + 1) == uri.end()) || ((i + 2) == uri.end()))
			{
				// Without an actual value. Idiots.
				i = uri.end() - 1;
				continue;
			}
			
			unsigned char v = utils::unhexchar(*(i + 1), *(i + 2));
			if ((v < 32) || (v == 127))
			{
				ServerInstance->Log(DEBUG, "URLEncoded unprintable character %d removed from URI.", v);
				i += 2;
				continue;
			}
			
			if (v != '/')
			{
				// Slashes are *NOT* allowed. They will be parsed like a normal /. Other characters are fine.
				pathpart.push_back(v);
				i += 2;
				continue;
			}
			
			// Parse this as if it were the normal character
			i += 2;
			c = v;
		}
		
		if (!c || (c == '/'))
		{
			// End of the URL *or* a path seperator
			
			if (!pathpart.length() || (pathpart == "."))
			{
				// Do nothing; these are meaningless and will be cleaned from the path
			}
			else if (pathpart == "..")
			{
				std::string::size_type sloc = sanepath.rfind('/', sanepath.length() - 2);
				if (sloc != std::string::npos)
					sanepath.erase(sloc + 1);
				else
					sanepath.clear();
			}
			else
			{
				sanepath.append(pathpart);
				if (c)
					sanepath.push_back('/');
			}
			
			pathpart.clear();
			
			if (!c)
				break;
		}
		else if (c == '?')
		{
			// Begin query
			// TODO URLEncode support
			uriquery.assign<std::string::const_iterator>(i + 1, uri.end());
			i = uri.end() - 1;
		}
		else if (c == '#')
		{
			// The client should not be sending this. Idiot.
			i = uri.end() - 1;
		}
		else
		{
			pathpart.push_back(c);
		}
	}
	
	uri.clear();
	if (sanepath[0] != '/')
		uri.push_back('/');
	
	uri.append(sanepath);
	
	ServerInstance->Log(DEBUG, "Cleaned URI to '%s'", uri.c_str());
}

void Connection::ServeData()
{
	ServerInstance->Log(DEBUG, "ServeData: %s: %s", method.c_str(), uri.c_str());
	
	// In the future, these might be handled differently. For now they're identical (Except that POST can have a body)
	if ((method == "GET") || (method == "POST"))
	{
		HandleURI();
		
		struct stat *fst = NULL;
		
		upath = ServerInstance->FileSys->CheckFilePath(ServerInstance->Config->DocRoot, uri, fst);

		if (upath.empty())
		{
			switch (errno)
			{
				case EACCES:
					this->SendError(403, "Forbidden");
					break;
				case ENOENT:
				case ELOOP:
				case ENAMETOOLONG:
				case ENOTDIR:
					this->SendError(404, "File Not Found");
					break;
				default:
					this->SendError(500, "Internal Server Error");
					break;
			}
			
			return;
		}
		
		int oflags = O_RDONLY;
#ifdef O_NOATIME
		// O_NOATIME can only be used on files owned by this user
		if (ServerInstance->Config->NoAtime && (fst->st_uid == geteuid()))
			oflags |= O_NOATIME;
#endif
		
		if (filefd > -1)
			close(filefd);
		
		filefd = open(upath.c_str(), oflags);
		if (filefd < 0)
		{
			switch (errno)
			{
				case EACCES:
					this->SendError(403, "Forbidden");
					break;
				case ENOENT:
				case ENOTDIR:
					this->SendError(404, "File Not Found");
					break;
				default:
					ServerInstance->Log(DEBUG, "open() to serve file '%s' failed with error: %s", upath.c_str(), strerror(errno));
					this->SendError(500, "Internal Server Error");
					break;
			}
			
			return;
		}
		
		// Don't set request state here; SendHeaders() will do that.
		
		rfilesize = fst->st_size;
		rfilesent = 0;
		RespondType = HTTP_RESPOND_BACKEND;
		ResponseBackend = WriteBackend::GetInstance(ServerInstance);

		HTTPHeaders empty;
		this->SendHeaders(fst->st_size, 200, "OK", empty);
		
		// When the headers have finished being sent, sending of data will be automatically triggered.
	}
	else
	{
		this->SendError(501, "Not Implemented");
	}
}

void Connection::SendHeaders(unsigned long size, int response, const std::string &rtext, HTTPHeaders &rheaders)
{
	State = HTTP_SEND_HEADERS;
	
	if (http_version == HTTP_1_0)
		this->Write("HTTP/1.0 ");
	else
		this->Write("HTTP/1.1 ");
	
	this->Write(ConvToStr(response)+" "+rtext+"\r\n");

	time_t local = this->ServerInstance->Time();
	struct tm *timeinfo = gmtime(&local);
	char *date = asctime(timeinfo);
	date[strlen(date) - 1] = '\0';
	rheaders.CreateHeader("Date", date);
	
	rheaders.CreateHeader("Server", "hottpd");
	rheaders.SetHeader("Content-Length", ConvToStr(size));
	
	if (size && !rheaders.IsSet("Content-Type"))
	{
		size_t p = uri.find_last_of("."); // XXX hack of sorts
		std::string mime;

		if (p != std::string::npos)
			mime = ServerInstance->MimeTypes->GetType(uri.substr(p + 1));

		if (mime.empty())
			mime = "application/x-octet-stream";

		ServerInstance->Log(DEBUG, "Sending mimetype %s for %s", mime.c_str(), uri.c_str());

		rheaders.SetHeader("Content-Type", mime);
	}
	else if (!size)
		rheaders.RemoveHeader("Content-Type");
	
	if (strcasecmp(rheaders.GetHeader("Connection").c_str(), "Close") == 0)
		keepalive = false;
	else if (keepalive)
		rheaders.SetHeader("Connection", "Keep-Alive");
	else
		rheaders.SetHeader("Connection", "Close");
	
	this->Write(rheaders.GetFormattedHeaders());
	this->Write("\r\n");
		
	if (!size)
	{
		// No request body, so we're done now
		EndRequest();
	}
}

void Connection::EndRequest()
{
	ServerInstance->Log(DEBUG, "End request");
	
	RequestsCompleted++;
	
	if (!keepalive)
	{
		State = HTTP_FINISHED;
		ServerInstance->Connections->Delete(this);
		return;
	}
	
	headers.Clear();
	method.clear();
	uri.clear();
	uriquery.clear();
	upath.clear();
	RequestBody.clear();
	http_version = HTTP_UNSPECIFIED;
	RequestBodyLength = 0;
	State = HTTP_WAIT_REQUEST;
	ResponseBackend = NULL;
	RespondType = HTTP_RESPOND_FLUSH;
	rfilesize = rfilesent = 0;
	
	if (filefd > -1)
	{
		close(filefd);
		filefd = -1;
	}
	
	if (requestbuf.length())
	{
		/* XXX should this be delayed to the next iteration to avoid letting a
		 * connection hog the process? If so, it would need to trigger regardless
		 * of a read event... */
		
		this->CheckRequest(0);
	}
}

void Connection::SendStaticData()
{
	ServerInstance->Log(DEBUG, "Sending response with backend");
	
	if (filefd < 0)
	{
		ServerInstance->Log(DEBUG, "Backend triggered but connection has no open file - closing connection");
		ServerInstance->Connections->Delete(this);
	}
	
	int re = ResponseBackend->ServeFile(GetFd(), filefd, rfilesent, rfilesize);
	if (re < 0)
	{
		ServerInstance->Log(DEBUG, "Response backend returned error; closing connection");
		ServerInstance->Connections->Delete(this);
	}	
	else if (rfilesent == rfilesize)
	{
		ServerInstance->Log(DEBUG, "Response backend finished serving request");
		EndRequest();
	}
	else
	{
		// More data to send; poll for write
		ServerInstance->SE->WantWrite(this);
	}
}

