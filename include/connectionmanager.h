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

class ConnectionManager : public classbase
{
 private:
	InspIRCd *ServerInstance;
 public:
	ConnectionManager(InspIRCd* Instance) : ServerInstance(Instance) { };

	/** Add a connection.
	 * This will create a new connection and initialise it.
	 * @param socket The socket id (file descriptor) this connection is on
	 * @param port The port number this connection connected on
	 * @param ip The IP address of the connection
	 * @return This function has no return value, but may not succeed always. XXX fix this
	 */
	void Add(int socket, int port, int socketfamily, sockaddr *ip);

	/** Disconnect a connection gracefully
	 * @param connection The connection to remove
	 * @return Although this function has no return type, on exit the connection provided will no longer exist. XXX fix this
	 */
	void Delete(Connection *c);
};
