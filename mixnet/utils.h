// packet_INIT.h

#ifndef PACKET_INIT_H
#define PACKET_INIT_H

#include "packet.h" // assuming this is where mixnet_packet and mixnet_packet_stp are defined

#ifndef UTILS_H
#define UTILS_H

#include "packet.h" // assuming this is where mixnet_packet and mixnet_packet_stp are defined
#include "node.h" // assuming this is where struct Node is defined

#include "node.h"

#endif
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

mixnet_packet* initialize_STP_packet(mixnet_address root_address, uint16_t path_length, mixnet_address node_address);

mixnet_packet* initialize_FLOOD_packet(mixnet_address root_address, uint16_t path_length, mixnet_address node_address);

// TO IMPL (for Dijkstra's)
// 2^8 (nb, cost) pairs possible
mixnet_packet* initialize_LSA_packet(mixnet_address node_addr, uint16_t nb_count, mixnet_address* neighbor_mixaddr, uint16_t* cost);
mixnet_packet* initialize_DATA_packet(mixnet_address** best_paths, mixnet_address dst_address, mixnet_address src_address, char* data, unsigned long data_size);  
mixnet_packet* initialize_PING_packet(mixnet_address** best_paths, mixnet_address dst_address, mixnet_address src_address);
// alloc to MAX_MIXNET_DATA_SIZE , copy over data properly usin gpointer
uint32_t get_time_in_ms(void);



// UTILS:
// void print_packet(mixnet_packet *packet);

// void print_node(struct node *node);


#endif
