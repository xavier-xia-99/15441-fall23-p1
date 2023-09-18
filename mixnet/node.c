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
#include "node.h"

#include "connection.h"
#include "packet.h"
#include "utils.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


struct Node {
  uint8_t num_neighbors;  // number of neighbors
  int16_t *neighbors_addy; // Array of neighbors
  bool *neighbors_blocked; // Block of neighbors (false is unblocked)

  mixnet_address root_addr; // root addr
  mixnet_address my_addr; // self addr
  mixnet_address next_hop; // Next hop
  int path_len;
};


// Found in DATA and PING Packet
struct RoutingHeader {
    mixnet_address src_addr; // given by user of mixnet_node
    mixnet_address dest_addr; // given by user of mixnet_node
    uint16_t route_len; // len(route)
    uint16_t route_index; // pointer at route (using pointer arithmetic)
};





// Declare functions
void receive_and_update(void *const handle, struct Node *node);
// void send_packet(void *const handle, struct node *node, enum mixnet_packet_type_enum type, uint16_t sender_port);
bool receive_STP(struct Node *currNode, uint8_t i, mixnet_packet *stp_packet, void *const handle);

  /**
   * @brief This function is called send STP constatnly
   *
   * @param handle
   * @param node
   */
void send_STP(void *const handle, struct Node *node) {
    //initialize a packet and send to all neighbors
    //don't have to bother w blocks, stp should send everywhere
    //don't need send last one. last one is user
    for (int i = 0; i < node->num_neighbors; i++) {
        // printf("Sending out STP to %d \n", node->neighbors_addy[i]);
        mixnet_packet* discover_packet = initialize_STP_packet(node->root_addr,node->path_len,node->my_addr);
        // print_packet(discover_packet);
        mixnet_send(handle, i, discover_packet); //TODO
    }
}


  /**
   * @brief This function sends Flood Packet, and takes note of the sender port
   *
   * @param handle
   * @param node
   * @param sender_port (do not send back)
   */
void send_FLOOD(void *const handle, struct Node *node, uint16_t sender_port) {
        // printf("------Node #%u 's NB_INDX:%d sent FLOOD------------\n", node->my_addr, sender_port);
        for (int i = 0; i < node->num_neighbors; i++) {
                if (!node->neighbors_blocked[i] && i != sender_port){
                    // printf("Sending out FLOOD to NB_INDX:%u | addr:%u \n", i, (unsigned int)node->neighbors_addy[i]);
                    mixnet_packet* flood_packet = initialize_FLOOD_packet(node->root_addr,node->path_len,node->my_addr);
                    mixnet_send(handle, i, flood_packet); //TODO error_handling
                }
        }

        // Always send to my user, unless I received it from the user!
        if (sender_port != node->num_neighbors){
            mixnet_packet* flood_packet = initialize_FLOOD_packet(node->root_addr,node->path_len,node->my_addr);
            mixnet_send(handle, node->num_neighbors, flood_packet);
        }
    }

/**
 * @brief This function is entrance point into each node
 * 
 * @param handle 
 * @param keep_running 
 * @param c 
 */

uint32_t get_time_in_ms(void) {
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return (time.tv_sec * 1000) + (time.tv_nsec / 1000000);
}
void run_node(void *const handle,
              volatile bool *const keep_running,
              const struct mixnet_node_config config) {

    (void) config;
    (void) handle;

    // print_node_config(config);
    // Initialize Node
    struct Node* node = malloc(sizeof(struct Node));
    node->num_neighbors = config.num_neighbors;
    node->neighbors_addy = malloc(sizeof(int) * config.num_neighbors);
    node->neighbors_blocked = malloc(sizeof(bool) * config.num_neighbors);
    if (node->neighbors_addy == NULL || node->neighbors_blocked == NULL) {
        exit(1);
    }
    for (int i = 0; i < config.num_neighbors; i++) {
        node->neighbors_addy[i] = -1;
    }
    
    for (int i = 0; i < config.num_neighbors; i++) {
        node->neighbors_blocked[i] = true; // initialize all to un-blocked
    }

    node->root_addr = config.node_addr; // I AM THE ROOT! 
    node->my_addr = config.node_addr;
    node->next_hop = config.node_addr; // self
    node->path_len = 0;

    send_STP(handle, node); // SEND STP 
    // printf("Set up neighbours");
    // print_node(node);
    
    uint32_t re_election_time = get_time_in_ms();
    uint32_t heartbeat_time = get_time_in_ms();
    // printf("Starting time: %d \n", start_time);

    //allocate chunk of memory
    mixnet_packet *packet_buffer =
        (mixnet_packet *)malloc(sizeof(MAX_MIXNET_PACKET_SIZE));
    if (packet_buffer == NULL) {
        exit(1);
    }
    uint16_t num_user_packets = 0;
    // uint8_t node_id = node->my_addr; 

    while(*keep_running) {
        uint8_t port;
        bool recv = mixnet_recv(handle, &port, &packet_buffer);

        // receiving from all my neighbors
        // uint32_t curr_time = get_time_in_ms();

        // i'm root, and its time to send a root hello
        if (node->root_addr == node->my_addr && get_time_in_ms() - heartbeat_time >= config.root_hello_interval_ms) {
            send_STP(handle, node);
            heartbeat_time = get_time_in_ms();
            re_election_time = get_time_in_ms();
        }

        // hey, i'm not the root, but i haven't got a root hello in a while. i'm root now!
        if (node->root_addr != node->my_addr && get_time_in_ms() - re_election_time >= config.reelection_interval_ms) {
            node->root_addr = node->my_addr;
            node->path_len = 0;
            node->next_hop = node->my_addr;
            send_STP(handle, node);
            re_election_time = get_time_in_ms();
        }
    
        // we received something. 
        if (recv) {
            // printf("Node: %d received packet! @ Port : %d \n", node_id, port);
            // printf("Packet Type : %s \n", get_packet_type(packet_buffer));
            // printf("RECEIVED Packet! \n");

            mixnet_packet_stp *update = (mixnet_packet_stp *)malloc(sizeof(mixnet_packet_stp));
            memcpy((void *)update, (void *)packet_buffer->payload,
    sizeof(mixnet_packet_stp));
        // uint16_t sender_id = update->node_address;
        // free(update);

        // else {
        //     printf("Received from other neighbours @ NB_INDX:%u \n", port);
        // }
        //     //
            if (packet_buffer->type == PACKET_TYPE_STP) {
                if (update->root_address == node->root_addr && update->path_length + 1 == node->path_len && update->node_address == node->next_hop) {
                    // mixnet_packet* new_packet = initialize_STP_packet(node->root_addr,node->path_len,node->my_addr);
                    // mixnet_send(handle, port, new_packet);
                    send_STP(handle, node);
                    re_election_time = get_time_in_ms(); 
                    } 
                else {
                    receive_STP(node, port, packet_buffer, handle);
                }
            }

            else if (packet_buffer->type == PACKET_TYPE_FLOOD){
                if (port == node->num_neighbors) {
                    num_user_packets++;
                    // printf("Received FLOOD from user! \n");
                    send_FLOOD(handle, node, port);
                    } 
                else if (!node->neighbors_blocked[port]){
                    send_FLOOD(handle, node, port);
                    }
                //only send to other neighbors
                }
        }
    }
    printf("\n Node[#%d] Stats: \n | Received %d user packets | Node[#%d] stopped running \n", node->my_addr, num_user_packets, node->my_addr);
    // print_node(node);
}


// Does not reach this function at all
bool receive_STP(struct Node * currNode, uint8_t port, mixnet_packet* stp_packet, void *const handle){
    // printf("\n[Received STP packet!]\n");

    mixnet_packet_stp *update = (mixnet_packet_stp *)malloc(sizeof(mixnet_packet_stp)); // Free-d
    
    memcpy((void *)update, (void *)stp_packet->payload,
           sizeof(mixnet_packet_stp));

    
    if (currNode->neighbors_addy[port] == -1) {
        currNode->neighbors_addy[port] = update->node_address;
        currNode->neighbors_blocked[port] = false; // unblock
        return true;
    } else {
        bool state_changed = false;
        // Received lower_root_address
        if (update->root_address < currNode->root_addr) {
                currNode->root_addr = update->root_address;
                currNode->path_len = update->path_length + 1;
                currNode->next_hop = update->node_address;
                state_changed = true;
                // old_root = currNode->root_addr;
        }
        // Received lower_path_length
        else if (update->root_address == currNode->root_addr &&
                 update->path_length + 1 < currNode->path_len) {
                    currNode->path_len = update->path_length + 1;
                    currNode->next_hop = update->node_address;
                    state_changed = true;
        }
        // Received lower_address for next_hop
        else if (update->root_address == currNode->root_addr &&
                 update->path_length + 1 == currNode->path_len &&
                 update->node_address < currNode->next_hop) {
                    currNode->next_hop = update->node_address;
                    state_changed = true;
        }

        if (state_changed){
            // Reset All Nodes
            send_STP(handle, currNode);

            // Need this to block all neighbors
            for (int i = 0; i < currNode->num_neighbors; i++) {
                currNode->neighbors_blocked[i] = true;
            }

        }
        // ======================= BLOCK LOGIC =======================
        // Parent is unblocked
        if (update->node_address == currNode->next_hop){
            currNode->neighbors_blocked[port] = false; 
        }

        // Unblock potential children 
        else if (update->root_address == currNode->root_addr &&
                update->path_length == currNode->path_len + 1){
                // printf("Node #%d, blocking my potential Parent, who is strictly worse \n", currNode->my_addr);
                currNode->neighbors_blocked[port] = false;
        }
        else {
            currNode->neighbors_blocked[port] = true;
        }
        // ======================= BLOCK LOGIC =======================
        
    }
    return true;
}



void print_node(struct Node *node) {
    printf("-----------------Printing Node [#%d]!------------------- \n", node->my_addr);
    printf("Root Addr: %d \n", node->root_addr);
    printf("Path Len: %d \n", node->path_len);
    printf("Node Addr: %d \n", node->my_addr);
    printf("Next Hop: %d \n", node->next_hop);
    printf("Neighbors List : \n");
    for (int i = 0; i < node->num_neighbors; i++) {
        const char* value = (node->neighbors_blocked[i]) ? "Yes" : "No";
        printf("Neighbor Index #%d | Address: %d | Blocked: %s | \n", i, node->neighbors_addy[i], value);
    }
    printf("--------------Printing Node [#%d] Complete!--------------\n",node->my_addr);
}
