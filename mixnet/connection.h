/**
 * Copyright (C) 2023 Carnegie Mellon University
 *
 * This file is part of the Mixnet course project developed for
 * the Computer Networks course (15-441/641) taught at Carnegie
 * Mellon University.
 *
 * No part of the Mixnet project may be copied and/or distributed
 * without the express permission of the 15-441/641 course staff.
 */
#ifndef MIXNET_CONNECTION_H_
#define MIXNET_CONNECTION_H_

#include "packet.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Receive a packet sent to this node over the Mixnet network.
 *
 * @param handle Opaque handle. DO NOT TOUCH!
 * @param port Callee-populated port on which the packet is received. For
 *             a node with n neighbors (ports {0,..., (n - 1)}), the n'th
 *             port is special: packets received on this port correspond
 *             to INPUTS (user data) that your implementation must handle.
 *             These are either FLOOD, DATA, or PING packets that need to
 *             be properly routed to their destinations:
 *
 *             1. FLOOD packets received on the n'th port can be sent out
 *                as-is. Note that, since mixnet_send() owns packets once
 *                once they are sent (see the mixnet_send() docstring for
 *                details), you should clone the packet before sending it
 *                out more than once.
 *
 *             2. For DATA and PING packets received on the n'th port, the
 *                "dst_address" field will be populated with the requisite
 *                destination node for this packet. You must check the FIB
 *                to find the appropriate path to route the packet on. All
 *                packets received on the n'th port will be maximum-sized
 *                (MAX_MIXNET_PACKET_SIZE). Finally, for DATA packets, the
 *                data will appear after the Routing Header (RH) assuming
 *                zero hops (i.e., at the `route` field of mixnet_packet_
 *                routing_header). You must fix-up the payload structure
 *                after you compute the source route and hop count.
 *
 * @param packet Pointer to a packet that will be populated by the callee.
 *               Packet themselves are heap-allocated. You may modify the
 *               contents as you see fit, but packets must either be:
 *               (a) free()'d once you are done processing them, OR
 *               (b) sent back over the network using mixnet_send()
 *
 * @return Number of packets received
 */
int mixnet_recv(void *handle, uint8_t *port, mixnet_packet **packet);

/**
 * Send a packet over the Mixnet network.
 *
 * @param handle Opaque handle. DO NOT TOUCH!
 * @param port Port on which the packet should be sent
 * @param packets Pointer to a packet to send. Packets themselves must be heap-
 *                allocated, either by you or a previous call to mixnet_recv().
 *                After this point, sent packets are "owned" by the callee, so
 *                you must not try to free them or modify their contents. Note:
 *                In the event that a packet is not successfully sent, you are
 *                responsible for re-attemptting sending until successful.
 *
 *                Similar to recv, for a node with n neighbors, the n'th port
 *                is special: packets sent on this port correspond to OUTPUTS
 *                (user data) that will be relayed to users up the stack. You
 *                must only ever send FLOOD, DATA, and PING packets on this
 *                port, and ONLY if they are destined for this node (e.g., if
 *                you recv a FLOOD packet that would create loops and result
 *                in broadcast storms, you must NOT send it to the user).
 *
 * @return Number of packets sent, or -1 on error (bad packet or arguments)
 */
int mixnet_send(void *handle, const uint8_t port, mixnet_packet *packet);

#ifdef __cplusplus
}
#endif

#endif // MIXNET_CONNECTION_H_
