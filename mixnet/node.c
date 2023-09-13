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
#include "packet_init.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct node {
  uint8_t num_neighbors;  // number of neighbors
  int16_t *neighbors_addy; // Array of neighbors
  bool *neighbors_blocked; // Block of neighbors (false is unblocked)

  mixnet_address root_addr; // root addr
  mixnet_address my_addr; // self addr
  mixnet_address next_hop; // Next hop
  int path_len;
};

// Considering switching to struct
struct neighbor {
  int16_t addr; // allowing -1 as default
  bool blocked;
};

// Declare functions
void receive_and_update(void *const handle, struct node *node);
void send_packet(void *const handle, struct node *node, enum mixnet_packet_type_enum type, int16_t sender_port);
bool receive_STP(struct node *currNode, int16_t i, mixnet_packet *stp_packet, void* const handle);
// UTILS:
void print_packet(mixnet_packet *packet);
void print_node(struct node *node);

  /**
   * @brief send_packet 
   *
   * @param handle
   * @param node
    * @param type (mixnet_packet_type_enum)
    * @param sender_port (sender port)
   */
  void send_packet(void *const handle, struct node *node,
                  enum mixnet_packet_type_enum type, int16_t sender_port) {
    /*
    Send out STP packets to all neighbors 
    */
    if (type == PACKET_TYPE_STP){
    //    printf("-----------------------going to be sending out STP from %u--------------------\n", node->my_addr);
        for (int i = 0; i < node->num_neighbors; i++) {
            // should we send back STP?
            // if(sender_port != i ){
            // printf("Sending out STP to %d \n", node->neighbors_addy[i]);
            mixnet_packet* discover_packet = initialize_STP_packet(node->root_addr,node->path_len,node -> my_addr);
            
            // print_packet(discover_packet);
            bool sent = mixnet_send(handle, i, discover_packet); 
            if (!sent){
                printf("error sending STP packet \n");
            }
            else {
                const char* val = (node->neighbors_addy[i] == -1) ? "NULL" : "NOT NULL";
                if (strcmp(val, "NULL") == 0) {
                    printf("Success! STP packet sent to NULL\n");
                } else {
                    printf("Success! STP packet sent to %d\n", node->neighbors_addy[i]);
                }
                // }
            }
        }
        // printf("-------------------------------------ending STP sending from %u--------------------\n", node->my_addr);
    }

    //send all instead of 
    else if (type == PACKET_TYPE_FLOOD){
        // printf("------Node #%u 's NB_INDX:%d sent FLOOD------------\n", node->my_addr, sender_port);
        // bool sent = false;
        for (int i = 0; i < node->num_neighbors; i++) {
                if (!node->neighbors_blocked[i] && i != sender_port){
                    // printf("Sending out FLOOD to NB_INDX:%u | addr:%u \n", i, (unsigned int)node->neighbors_addy[i]);
                    mixnet_packet* flood_packet = initialize_FLOOD_packet(node->root_addr,node->path_len,node->my_addr);

                    mixnet_send(handle, i, flood_packet); //TODO error_handling
                }
        }

        // Always send to my user, unless I received it from the user!
        if (sender_port != node->num_neighbors && sender_port != -1){
            // printf("[FLOOD ENDING @ Node$#%d] Sending to USER! \n", node->my_addr);
            mixnet_packet* flood_packet = initialize_FLOOD_packet(node->root_addr,node->path_len,node->my_addr);
            mixnet_send(handle, node->num_neighbors, flood_packet);
        }
    }
}


void print_packet(mixnet_packet *packet) {
    //  Free-d
    printf("\n--------------[START OF PACKET]-------------\n");
    printf("PACKET TYPE: %d \n", packet->type);
    printf("Payload: \n");
    switch (packet->type) {
        case PACKET_TYPE_STP:
        case PACKET_TYPE_FLOOD:
        {
            mixnet_packet_stp *update =(mixnet_packet_stp *)malloc(sizeof(mixnet_packet_stp)); //
            memcpy((void *)update, (void *)packet->payload,
                sizeof(mixnet_packet_stp));

            printf("Printing Packet! \n");
            printf("Root(Node) Address: %d \n", update->root_address);
            printf("Path Length: %d \n", update->path_length);
            printf("Sender(Node) address: %d \n", update->node_address);

            free(update);
        }
        break;  
    // case PACKET_TYPE_DATA:
    //     mixnet_packet_data *update =
    //         (mixnet_packet_data *)malloc(sizeof(mixnet_packet_data)); //
    //     memcpy((void *)data, (void *)packet->payload,
    //            sizeof(mixnet_packet_data));
    //     printf("Printing Packet! \n");
    //     printf("Root address: %d \n", data->root_address);
    //     printf("Path length: %d \n", data->path_length);
    //     printf("Node address: %d \n", data->node_address);
    //     printf("Data: %s \n", data->data);
    //     break;
    // }
    printf("\n-------------------[END OF PACKET]----------------------\n");
    }
}


void print_node(struct node *node) {
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


char* get_packet_type(mixnet_packet *packet) {
    switch (packet->type) {
        case PACKET_TYPE_STP:
        return "STP";
        case PACKET_TYPE_FLOOD:
        return "FLOOD";
        case PACKET_TYPE_DATA:
        return "DATA";
        default :
        return "ERROR WITH PACKET TYPE!";
    }
}

void print_node_config(const struct mixnet_node_config config){
    printf("--------- Printing Node Config! ---------\n");
    printf("Node Address: %d \n", config.node_addr);
    printf("Root Hello Interval: %d \n", config.root_hello_interval_ms);
    printf("Reelection Interval: %d \n", config.reelection_interval_ms);
    printf("Number of Neighbors: %d \n", config.num_neighbors);
    // printf("Neighbors: \n");
    // for (int i = 0; i < config.num_neighbors; i++) {
    //     printf("Neighbor Node %d: %d \n", i, config.neighbors_addrs[i]);
    // }
    printf("---------Printing Node Config Complete! ---------\n");
}

uint32_t get_time_in_ms(void) {
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return (time.tv_sec * 1000) + (time.tv_nsec / 1000000);
}
/**
 * @brief This function is entrance point into each node
 * 
 * @param handle 
 * @param keep_running 
 * @param c 
 */
void run_node(void *const handle,
              volatile bool *const keep_running,
              const struct mixnet_node_config config) {

    (void) config;
    (void) handle;

    print_node_config(config);
    // Initialize Node
    struct node* node = malloc(sizeof(struct node));
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
        node->neighbors_blocked[i] = false;
    }

    node->root_addr = config.node_addr; // Real Root
    node->my_addr = config.node_addr; // my addr
    node->next_hop = config.node_addr; // Node_Addr
    node->path_len = 0;
    
    print_node(node);

    send_packet(handle, node, PACKET_TYPE_STP, -1); // SEND STP 
    // printf("Sending STP for Neighbor update \n");
    // receive_and_update(handle, node);
    // printf("Set up neighbours");
    // print_node(node);

    uint32_t start_time = get_time_in_ms();

    //allocate chunk of memory
    mixnet_packet *packet_buffer =
        (mixnet_packet *)malloc(sizeof(MAX_MIXNET_PACKET_SIZE));

    if (packet_buffer == NULL) {
        exit(1);
    }
    uint16_t num_user_packets = 0;
    uint8_t node_id = node->my_addr; 

    while(*keep_running) {
        uint8_t port;
        bool recv = mixnet_recv(handle, &port, &packet_buffer);
        if (port == node->num_neighbors) {
            num_user_packets++;
        }
        uint32_t end_time = get_time_in_ms();
        uint32_t time_passed = end_time - start_time;
        if (!recv) {
            // i'm root, and its time to send a root hello
            // Send out ROOT_HELLO
            if (node->root_addr == node->my_addr && time_passed >= config.root_hello_interval_ms) {
                send_packet(handle, node, PACKET_TYPE_STP, -1);
                start_time = get_time_in_ms();

            }

            // hey, i'm not the root, but i haven't got a root hello in a while. i'm root now!
            else if (node->root_addr != node->my_addr && time_passed >= config.reelection_interval_ms) {
                // printf("Node: %d not receive, i am NOT root but exceed REELECTION interval, so I AM ROOT!' \n", node_id);
                node->root_addr = node->my_addr;
                node->path_len = 0;
                node->next_hop = node->my_addr;
                send_packet(handle, node, PACKET_TYPE_STP, -1);

                start_time = get_time_in_ms();
            }
        } 

        // Received Packet
        else if (recv) {
            printf("Node: %d received packet! @ Port : %d \n", node_id, port);
            mixnet_packet_stp *update = (mixnet_packet_stp *)malloc(sizeof(mixnet_packet_stp));
            memcpy((void *)update, (void *)packet_buffer->payload,
    sizeof(mixnet_packet_stp)); // Need to free

            // uint16_t sender_id = update->node_address;
            free(update);
            if (port == node->num_neighbors) {
                printf(" \n USER Sent Packet! \n");
            } else {
                printf("Received from other neighbours @ NB_INDX:%u \n", port);
            }
                //
            switch (packet_buffer->type) {
                case PACKET_TYPE_STP:
                    receive_STP(node, port, packet_buffer, handle);
                    // send_packet(handle, node, PACKET_TYPE_FLOOD, -1); // STP == -1
                    break;
                case PACKET_TYPE_FLOOD:
                    // Ignore Flood packets from blocked ports
                    if (port < node->num_neighbors && !node->neighbors_blocked[port]){
                        send_packet(handle, node, PACKET_TYPE_FLOOD, port);
                    }
                //only send to other neighbors
                    break;
                }
            }
        }


    printf("\n Node[#%d] Stats: \n | Received %d user packets | Node[#%d] stopped running \n", node->my_addr, num_user_packets, node->my_addr);
    print_node(node);
}


// Does not reach this function at all
bool receive_STP(struct node * currNode, int16_t port, mixnet_packet* stp_packet, void *const handle){
    printf("\n[Received STP packet!]\n");

    mixnet_packet_stp *update = (mixnet_packet_stp *)malloc(sizeof(mixnet_packet_stp)); // Free-d
    
    memcpy((void *)update, (void *)stp_packet->payload,
           sizeof(mixnet_packet_stp));

    
    bool state_changed = false;
    if (currNode->neighbors_addy[port] == -1) {
        printf("Updating neighbors list because it was NULL \n");
        currNode->neighbors_addy[port] = update->node_address;
        currNode->neighbors_blocked[port] = false; // unblock
    } else {
        // Received lower_root_address
        if (update->root_address < currNode->root_addr) {
                    printf("[1] Updating.. root[%d] address of because received lower ROOT addr \n", currNode->my_addr);
                    // printf("before: root address: %u, path length: %u, next hop: %u \n", currNode->root_addr, currNode->path_len, currNode->next_hop);
                    // printf("after: root address: %u, path length: %u, next hop: %u \n", update->root_address, update->path_length + 1, update->node_address);

                    currNode->root_addr = update->root_address;
                    currNode->path_len = update->path_length + 1;
                    currNode->next_hop = update->node_address;
                    state_changed = true;
        }
        // Received lower_path_length
        else if (update->root_address == currNode->root_addr &&
                 update->path_length + 1 < currNode->path_len) {
                    printf("[2] Updated root[%d] because better path\n", currNode->my_addr);
                    // printf("before: root address: %u, path length: %u, next hop: %u \n", currNode->root_addr, currNode->path_len, currNode->next_hop);
                    // printf("after: root address: %u, path length: %u, next hop: %u \n", update->root_address, update->path_length + 1, update->node_address);
                    
                    currNode->path_len = update->path_length + 1;
                    currNode->next_hop = update->node_address;
                    state_changed = true;
        }
        // Received lower_address for next_hop
        else if (update->root_address == currNode->root_addr &&
                 update->path_length + 1 == currNode->path_len &&
                 update->node_address < currNode->next_hop) {
                    printf("[3] Updated root[%d] cause update's node is better for next hop\n", currNode->my_addr);
                    // printf("before: root address: %u, path length: %u, next hop: %u \n", currNode->root_addr, currNode->path_len, currNode->next_hop);
                    // printf("after: root address: %u, path length: %u, next hop: %u \n", update->root_address, update->path_length + 1, update->node_address);
                    currNode->next_hop = update->node_address;
                    state_changed = true;
        }

        if (state_changed){
            // Reset All Nodes
                // Reset all neighbours state to unblocked:
            // Manually send to all neighbors for in a for loop except sender
            for (int i = 0; i < currNode->num_neighbors; i++) {
                mixnet_packet* stp_packet = initialize_STP_packet(currNode->root_addr, currNode->path_len, currNode->my_addr);
                mixnet_send(handle, i, stp_packet);
            }
                // TODO : Send to all neighbors with new state except SENDER
            for (int i = 0; i < currNode->num_neighbors; i++) {
                currNode->neighbors_blocked[i] = false;
            }
            // SEND THEM again
        }

        // Received from parent [Send update to everyone]
        else if (port != currNode->num_neighbors){
            // Find and Block Siblings
            printf("[NO STATE CHANGE]! \n");
            if (update->root_address == currNode->root_addr &&
                 update->path_length == currNode->path_len){
                    printf("Blocking siblings of root[%d] cause they are siblings\n", currNode->my_addr);
                    currNode->neighbors_blocked[port] = true;
            }
        // For potential parent, i as a children, block my update[parent]
        // same root as you and whose path length + 1== your path length
            else if (update->root_address == currNode->root_addr &&
                 update->path_length + 1 == currNode->path_len &&
                 update->node_address != currNode->next_hop){
                    printf("Node #%d, blocking my potential Parent, who is strictly worse \n", currNode->my_addr);
                    currNode->neighbors_blocked[port] = true;
            }
        }
        /*
        
        BLOCKING LOGIC
        1. siblings
        2. not real parents
        */
        
    }
    free(update);
    printf("[End of STP packet!]\n\n");
    return state_changed;
}

/**
 * @brief This function is called to receive and update the node
 *
 * @param handle
 * @param currNode
 */
void receive_and_update(void *const handle, struct node *currNode) {

    (void)handle;

    mixnet_packet *packet =
        (mixnet_packet *)malloc(sizeof(MAX_MIXNET_PACKET_SIZE));
    if (packet == NULL) {
        exit(1);
    }
    
    for (uint8_t i = 0; i < currNode->num_neighbors; i++) {
        // Init Header
        if (packet == NULL) {
            exit(1);
        }
        bool recv = mixnet_recv(handle, &i, &packet); // DID I USE THIS PROPERLY?
        if (!recv) {
            printf("Did not receive anything! \n");
            return;
        } else if (recv) {
                switch (packet->type) {
                    case PACKET_TYPE_STP:
                        receive_STP(currNode, i, packet, handle);

                    break;
                    case PACKET_TYPE_FLOOD:
                        // send_packet(handle, currNode, PACKET_TYPE_FLOOD);
                    break;
                    }
        }
        free(packet);
    }
}
