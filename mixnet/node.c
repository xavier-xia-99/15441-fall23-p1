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
void send_packet(void *const handle, struct node *node, enum mixnet_packet_type_enum type, uint16_t sender_index);
bool receive_STP(struct node *currNode, uint8_t i, mixnet_packet *stp_packet);
// UTILS:
void print_packet(mixnet_packet *packet);
void print_node(struct node *node);

  /**
   * @brief This function is called send STPS constatnly
   *
   * @param handle
   * @param node
   */
  void send_packet(void *const handle, struct node *node,
                  enum mixnet_packet_type_enum type, uint16_t sender_index) {
    //initialize a packet and send to all neighbors
    //don't have to bother w blocks, stp should send everywhere

    //don't need send last one. last one is user
    if (type == PACKET_TYPE_STP){
    //    printf("-----------------------going to be sending out STP from %u--------------------\n", node->my_addr);
        for (int i = 0; i < node->num_neighbors; i++) {
                // printf("Sending out STP to %d \n", node->neighbors_addy[i]);
                mixnet_packet* discover_packet = initialize_STP_packet(node->root_addr,node->path_len,node -> my_addr);
                // print_packet(discover_packet);
                bool sent = mixnet_send(handle, i, discover_packet); //TODO error_handling
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
                }
        }

        // printf("-------------------------------------ending STP sending from %u--------------------\n", node->my_addr);
    }

    //send all instead of 
    else if (type == PACKET_TYPE_FLOOD){
        printf("-----------Sending Flood-Packet from Node #%u---------------\n", node->my_addr);
        bool sent = false;
        for (int i = 0; i < node->num_neighbors; i++) {
                if (!node->neighbors_blocked[i] && i != sender_index){
                    printf("Sending out FLOOD to %u \n", i);
                    mixnet_packet* flood_packet = initialize_FLOOD_packet(node->root_addr,node->path_len,node->my_addr);
                    // print_packet(flood_packet);
                    
                    // print_packet(flood_packet);
                    mixnet_send(handle, i, flood_packet); //TODO error_handling
                    sent = true;
                    // bool sent = 
                    // if (!sent) {
                    //   printf("Error sending FlOOD packet \n");
                    // }
                    // else {
                    // printf("success! FlOOD packet sent to %hn\n", node->neighbors_addy);
                    //  }
                }
        }

        // [MISSING] send to my user!
        if (!sent){
            printf("[FLOOD ENDS] Sending out Flood to my USER! \n");
            mixnet_packet* flood_packet = initialize_FLOOD_packet(node->root_addr,node->path_len,node->my_addr);
            mixnet_send(handle, node->num_neighbors, flood_packet);
        }
            // printf("-------------------------------------ending FlOODS sending from %u--------------------\n", node->my_addr);
    }
}

// TODO : add support for custom message
void print_packet(mixnet_packet *packet) {
    //  Free-d
    printf("\n--------------------[START OF PACKET]------------------\n");
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

    node->root_addr = config.node_addr; // I AM THE ROOT! -> this is teh real root
    node->my_addr = config.node_addr;
    node->next_hop = config.node_addr; // self
    node->path_len = 0;

    //discovering neighbor logic -> sends out messages to all neighbors
    //receiving and updating neighbor states

    //can i just do this
    print_node(node);

    send_packet(handle, node, PACKET_TYPE_STP, 0); // SEND STP 
    // printf("Sending STP for Neighbor update \n");
    // receive_and_update(handle, node);
    // printf("Set up neighbours");

    // print_node(node);
    
    clock_t start_time = clock();
    printf("Starting time: %lu \n", start_time);

    //allocate chunk of memory
    mixnet_packet *packet_buffer =
        (mixnet_packet *)malloc(sizeof(MAX_MIXNET_PACKET_SIZE));
    if (packet_buffer == NULL) {
        exit(1);
    }
    uint16_t num_user_packets = 0;

    while(*keep_running) {
        uint8_t node_id = node->my_addr; 
        uint8_t port;
        bool recv = mixnet_recv(handle, &port, &packet_buffer);
        if (port == node->num_neighbors) {
            num_user_packets++;
        }
        // printf("packet type: %s \n",get_packet_type(packet_buffer));
        
        // if (recv) {;
            // }

        // receiving from all my neighbors
        // for (uint8_t i = 0; i < node->num_neighbors; i++) {
        if (!recv) {
            // i'm root, and its time to send a root hello
            clock_t curr_time = clock();
            if (node->root_addr == node->my_addr && curr_time - start_time >= config.root_hello_interval_ms * 1000) {
                // printf("Starting time: %lu \n", curr_time);
                // printf("Node: %d did not receive, and I AM ROOT so sending hello!' \n", node_id);
                send_packet(handle, node, PACKET_TYPE_STP, 0);
                start_time = clock();
            }

            // hey, i'm not the root, but i haven't got a root hello in a while. i'm root now!
            else if (node->root_addr != node->my_addr && curr_time - start_time >= config.reelection_interval_ms * 1000) {
                // printf("Node: %d not receive, i am NOT root but exceed REELECTION interval, so I AM ROOT!' \n", node_id);
                node->root_addr = node->my_addr;
                node->path_len = 0;
                node->next_hop = node->my_addr;
                send_packet(handle, node, PACKET_TYPE_STP, 0);
                start_time = clock();
            }
        } 
        // we received something. 
        else if (recv) {
            printf("Node: %d received packet! @ Port : %d \n", node_id, port);
            // printf("Packet Type : %s \n", get_packet_type(packet_buffer));
            // printf("RECEIVED Packet! \n");
            mixnet_packet_stp *update = (mixnet_packet_stp *)malloc(sizeof(mixnet_packet_stp));
            memcpy((void *)update, (void *)packet_buffer->payload,
    sizeof(mixnet_packet_stp));
        uint16_t sender = update->node_address;
        free(update);
        printf("Packet's root address: %u \n", sender);
            switch (packet_buffer->type) {
            case PACKET_TYPE_STP:

                receive_STP(node, port, packet_buffer);
                
                // }
                break;
            case PACKET_TYPE_FLOOD:

            //only send to other neighbors
                send_packet(handle, node, PACKET_TYPE_FLOOD, sender);
                break;
            }
        }
        }

    printf("\n Node[#%d] Stats: \n | Received %d user packets | Node[#%d] stopped running \n", node->my_addr, num_user_packets, node->my_addr);
    print_node(node);
}


// Does not reach this function at all
bool receive_STP(struct node * currNode, uint8_t port, mixnet_packet* stp_packet){
    printf("\n[Received STP packet!]\n");

    mixnet_packet_stp *update = (mixnet_packet_stp *)malloc(sizeof(mixnet_packet_stp)); // Free-d
    
    memcpy((void *)update, (void *)stp_packet->payload,
           sizeof(mixnet_packet_stp));

    // Update if the neighbors list are NULL
    // for (int i = 0; i < currNode->num_neighbors; i++) {
    //     printf("NB #%u , &: %d \n", i, currNode->neighbors_addy[i]);
    // }

    // if (update->node_address == currNode->my_addr) {
    //     printf("Received STP packet from myself, ignoring \n");
    //     return false;
    // }
    
    if (currNode->neighbors_addy[port] == -1) {
        printf("Updating neighbors list because it was NULL \n");
        currNode->neighbors_addy[port] = update->node_address;
        currNode->neighbors_blocked[port] = false; // unblock
    } else {
        bool updated = false;
        // Received lower_root_address
        if (update->root_address < currNode->root_addr) {
                    printf("[1] Updating.. root[%d] address of because received lower ROOT addr \n", currNode->my_addr);
                    //print before and after for me

                    printf("before: root address: %u, path length: %u, next hop: %u \n", currNode->root_addr, currNode->path_len, currNode->next_hop);
                    printf("after: root address: %u, path length: %u, next hop: %u \n", update->root_address, update->path_length + 1, update->node_address);

                    currNode->root_addr = update->root_address;
                    currNode->path_len = update->path_length + 1;
                    currNode->next_hop = update->node_address;
                    updated = true;


        }
        // Received lower_path_length
        else if (update->root_address == currNode->root_addr &&
                 update->path_length + 1 < currNode->path_len) {
                    printf("[2] Updated root[%d] because better path\n", currNode->my_addr);
                    printf("before: root address: %u, path length: %u, next hop: %u \n", currNode->root_addr, currNode->path_len, currNode->next_hop);
                    printf("after: root address: %u, path length: %u, next hop: %u \n", update->root_address, update->path_length + 1, update->node_address);
                    
                    currNode->path_len = update->path_length + 1;
                    currNode->next_hop = update->node_address;
                    updated = true;
        }
        // Received lower_address for next_hop
        else if (update->root_address == currNode->root_addr &&
                 update->path_length + 1 == currNode->path_len &&
                 update->node_address < currNode->next_hop) {
                    printf("[3] pdated root[%d] cause update's node is better for next hop\n", currNode->my_addr);
                    printf("before: root address: %u, path length: %u, next hop: %u \n", currNode->root_addr, currNode->path_len, currNode->next_hop);
                    printf("after: root address: %u, path length: %u, next hop: %u \n", update->root_address, update->path_length + 1, update->node_address);

                    currNode->next_hop = update->node_address;
                    updated = true;
        }

        // If the update_packet from this port was not useful, BLOCK it
        if (!updated) {
                printf("Not updating, Blocking relative port \n");
                currNode->neighbors_blocked[port] = true;
        } 
        else { // updated, unblock
            currNode->neighbors_blocked[port] = false;
        }
    }
    free(update);
    printf("[End of STP packet!]\n\n");
    return true;
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
                        receive_STP(currNode, i, packet);
                    break;
                    case PACKET_TYPE_FLOOD:
                        // send_packet(handle, currNode, PACKET_TYPE_FLOOD);
                    break;
                    }
        }
        free(packet);
    }
}
