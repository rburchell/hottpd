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

/* $Core: libhttpd_buffer */

#include "inspircd.h"
#include <stdarg.h>
#include "socketengine.h"
#include "wildcard.h"

void Connection::ReadData()
{
	int result = EAGAIN;

	if (this->GetFd() == FD_MAGIC_NUMBER)
		return;

	char* ReadBuffer = ServerInstance->GetReadBuffer();

#ifndef WIN32
	result = read(this->fd, ReadBuffer, sizeof(ReadBuffer));
#else
	result = recv(this->fd, (char*)ReadBuffer, sizeof(ReadBuffer), 0);
#endif

	if ((result) && (result != -EAGAIN))
	{
		int currfd;

		if (result > 0)
			ReadBuffer[result] = '\0';

		currfd = this->GetFd();

		// add the data to the connection's buffer (which will process it if necessary)
		if (result > 0)
		{
			if (!this->AddBuffer(ReadBuffer))
			{
				// fuck, something exploded
				ServerInstance->Connections->Delete(this);
				return;
			}

			return;
		}

		if ((result == -1) && (errno != EAGAIN) && (errno != EINTR))
		{
			ServerInstance->Connections->Delete(this);
			return;
		}
	}

	// result EAGAIN means nothing read
	else if ((result == EAGAIN) || (result == -EAGAIN))
	{
		/* do nothing */
	}
	else if (result == 0)
	{
		ServerInstance->Connections->Delete(this);
		return;
	}
}

bool Connection::AddBuffer(const std::string &a)
{
	if (!a.length())
	{
		/* how is this possible .. */
		return true;
	}
	
	if (State == HTTP_RECV_REQBODY)
	{
		/*
		 * Note!
		 *
		 * Don't be clever here, we *must* only take what we can to the request body
		 * (i.e. NO MORE than RequestBodyLength!). It would be naughty to accept any
		 * more than content-length bytes for the request body, primarily
		 * thanks to pipelining. So, we take what we can, and shove the rest in the request buffer.
		 */
		unsigned int remains = RequestBodyLength - RequestBody.length();
		
		if (remains < a.length())
		{
			RequestBody.append(a.begin(), a.begin() + remains);
			// The rest of this goes into the request buffer
			requestbuf.append(a);
		}
		else
			RequestBody.append(a);
		
		if (RequestBody.length() == RequestBodyLength)
		{
			// Done reading the request body
			ServerInstance->Log(DEBUG, "Request body: %s", RequestBody.c_str());
			ServerInstance->Log(DEBUG, "Finished reading request body (%d bytes). Serving request.", RequestBodyLength);
			ServeData();
			return true;
		}
	}
	else
	{
		/* We can get data for a future request at any time, and that
		 * is what we're doing here. We only trigger the check for a
		 * new request if we're waiting for one. */
		
		int nspos = requestbuf.length();
		
		requestbuf.append(a);

		if (State == HTTP_WAIT_REQUEST)
			this->CheckRequest(nspos);		
	}

	if (requestbuf.length() > 5120)
	{
		// XXX arbitrary limit; needs discussion of a proper default
		ServerInstance->Log(DEBUG, "Too much data in buffer; dropping");
		return false;
	}
	

	return true;
}

void Connection::AddWriteBuf(const std::string &data)
{
	sendq.append(data);
}

// send AS MUCH OF THE ConnectionS SENDQ as we are able to (might not be all of it)
void Connection::FlushWriteBuf()
{
	if ((sendq.length()) && (this->fd != FD_MAGIC_NUMBER))
	{
		int old_sendq_length = sendq.length();
		int n_sent = ServerInstance->SE->Send(this, this->sendq.data(), this->sendq.length(), 0);

		if (n_sent == -1)
		{
			if (errno == EAGAIN)
			{
				/* The socket buffer is full. This isnt fatal,
				 * try again later.
				 */
				this->ServerInstance->SE->WantWrite(this);
			}
			else
			{
				/* Fatal error, set write error and bail
				 */
				ServerInstance->Connections->Delete(this); // XXX shouldn't try flush buffer, add a bool?
				return;
			}
		}
		else
		{
			/* advance the queue */
			if (n_sent)
				this->sendq = this->sendq.substr(n_sent);
			if (n_sent != old_sendq_length)
				this->ServerInstance->SE->WantWrite(this);
		}
	}

	if (this->sendq.empty())
	{
		FOREACH_MOD(I_OnBufferFlushed,OnBufferFlushed(this));
		
		if ((RespondType == HTTP_RESPOND_FLUSH) && (State == HTTP_SEND_DATA))
		{
			EndRequest();
		}
		else if ((State == HTTP_SEND_HEADERS) && (RespondType == HTTP_RESPOND_BACKEND))
		{
			State = HTTP_SEND_DATA;
			SendStaticData();
		}
	}
}

/** NOTE: We cannot pass a const reference to this method.
 * The string is changed by the workings of the method,
 * so that if we pass const ref, we end up copying it to
 * something we can change anyway. Makes sense to just let
 * the compiler do that copy for us.
 */
void Connection::Write(const std::string &text)
{
	if (!ServerInstance->SE->BoundsCheckFd(this))
		return;

	this->AddWriteBuf(text);
	this->ServerInstance->SE->WantWrite(this);
}

/** Write()
 */
void Connection::Write(const char *text, ...)
{
	va_list argsPtr;
	char textbuffer[MAXBUF];

	va_start(argsPtr, text);
	vsnprintf(textbuffer, MAXBUF, text, argsPtr);
	va_end(argsPtr);

	this->Write(std::string(textbuffer));
}

