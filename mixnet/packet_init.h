// packet_INIT.h

#ifndef PACKET_INIT_H
#define PACKET_INIT_H

#include "packet.h" // assuming this is where mixnet_packet and mixnet_packet_stp are defined


//---------------packet declarations--------------------------


// general type of all packets
// A 16-bit total size (total_size)
// A 16-bit type (type)
// A 64-bit reserved field (_reserved)
// A variable-size payload (payload)


// packet types you can use

// mixnet_packet_stp: Spanning Tree Protocol packets
// mixnet_packet_lsa: Link State Advertisement packets
// mixnet_packet_routing_header: For routing info
// mixnet_packet_ping: Ping packets

mixnet_packet* initialize_STP_packet(mixnet_address root_address, u_int16_t path_length, mixnet_address node_address);

#endif 