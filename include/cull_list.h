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

#ifndef __CULLLIST_H__
#define __CULLLIST_H__

/** The CullList class is used by the core, to compile a list
 * of connections in preperation to quitting them all at once.
 * This is faster than quitting them within the loop, as the
 * loops become tighter with little or no comparisons within them.
 * The CullList class operates by allowing the programmer to push
 * connections onto the list, and then,
 * once the list is complete, call a method to flush the list,
 * quitting all the connections upon it. A CullList may only hold
 * each connection once.
 * If you attempt to add the same connection twice, then the second
 * attempt will be ignored.
 */
class CoreExport CullList : public classbase
{
 private:
	/** Creator of this CullList
	 */
	InspIRCd* ServerInstance;

	/** Holds a list of connections being quit.
	 * See the information for CullItem for
	 * more information.
	 */
	std::vector<Connection *> list;

 public:
	/** Constructor.
	 * Clears the CullList::list and CullList::exempt
	 * items.
	 * @param Instance Creator of this CullList object
	 */
	CullList(InspIRCd* Instance);

	/** Adds a connection to the cull list for later removal via QUIT.
	 * @param c The connection to add
	 */
	void AddItem(Connection *c);

	/** Applies the cull list, closing all connections on the list
	 * at once. This is a very fast operation compared to
	 * iterating the connection list and comparing each one,
	 * especially if there are multiple comparisons
	 * to be done, or recursion.
	 * @returns The number of connections closed.
	 */
	int Apply();
};

#endif

