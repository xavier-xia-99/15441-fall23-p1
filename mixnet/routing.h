// routing.h



#ifndef ROUTING_H
#define ROUTING_H

#include "packet.h" // assuming this is where mixnet_packet and mixnet_packet_stp are defined
#include "node.h" // assuming this is where struct Node is defined
#include "utils.h"

struct Node;

void receive_and_send_LSA(mixnet_packet* LSA_packet, void* handle , struct Node * node, uint16_t sender_port);
void dijkstra(struct Node * node, bool verbose);
void construct_shortest_path(mixnet_address toNodeAddress, struct Node* node, mixnet_address *prev_neighbor);
uint16_t find_next_port(mixnet_packet_routing_header* routing_header, struct Node* node);  

#endif

                            // if (node->queue_size < node->mixingfactor){
                            //     node->queue[node->queue_size] = final_data_packet;
                            //     node->queue_size ++;
                            //     printf("[%d] Queue Size: %d| Max Size %d \n", node->my_addr, node->queue_size, node->mixingfactor);
                            //     printf("Data Save: %d %d", header->src_address, header->dst_address);
                            //     continue;
                            // }
                            // printf("[%d] Queue is full! \n", node->my_addr);
                            // uint16_t start = 0;
                            // node->queue_size = 0;
                            // while (start < node->mixingfactor){
                            //     assert(node->queue[start] != NULL);
                            //     mixnet_packet* curr_packet = node->queue[start];