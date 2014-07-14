/** Interface related functions
 *
 * These functions are used to manage a network interface.
 * Most of them make use of Linux-specific APIs.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 * @file
 */

#ifndef _IF_H_
#define _IF_H_

#include <sys/types.h>
#include <net/if.h>

#define IF_NAME_MAX	IFNAMSIZ /**< Maximum length of an interface name */
#define IF_IRQ_MAX	3	 /**< Maxmimal number of IRQs of an interface */

/** Interface data structure */
struct interface {
	/** The index used by the kernel to reference this interface */
	int index;
	/** How many nodes use this interface for outgoing packets */
	int refcnt;
	/** Human readable name of this interface */
	char name[IF_NAME_MAX];
	/** List of IRQs of the NIC */
	char irqs[IF_IRQ_MAX];

	/** Linked list pointer */
	struct interface *next;
};

/** Get outgoing interface.
 *
 * Does a lookup in the kernel routing table to determine
 * the interface which sends the data to a certain socket
 * address.
 *
 * @param sa A destination address for outgoing packets
 * @return The interface index
 */
int if_getegress(struct sockaddr_in *sa);

/** Get all IRQs for this interface.
 *
 * Only MSI IRQs are determined by looking at:
 *  /sys/class/net/{ifname}/device/msi_irqs/
 *
 * @param i A pointer to the interface structure
 * @return
 *  - 0 on success
 *  - otherwise an error occured
 */
int if_getirqs(struct interface *i);

/** Change the SMP affinity of NIC interrupts.
 *
 * @param i A pointer to the interface structure
 * @param affinity A mask specifying which cores should handle this interrupt.
 * @return
 *  - 0 on success
 *  - otherwise an error occured
 */
int if_setaffinity(struct interface *i, int affinity);

/** Search list of interface for a index.
 *
 * @param index The interface index to search for
 * @param interfaces A linked list of all interfaces
 * @return A pointer to the node or NULL if not found
 */
struct interface* if_lookup_index(int index, struct interface *interfaces);

#endif /* _IF_H_ */
