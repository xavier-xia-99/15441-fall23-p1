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
// void send_packet(void *const handle, struct node *node, enum mixnet_packet_type_enum type, uint16_t sender_port);
bool receive_STP(struct node *currNode, uint8_t i, mixnet_packet *stp_packet, void *const handle);
// UTILS:
void print_packet(mixnet_packet *packet);
void print_node(struct node *node);

  /**
   * @brief This function is called send STP constatnly
   *
   * @param handle
   * @param node
   */
void send_STP_from_node(void *const handle, struct node *node) {
    //initialize a packet and send to all neighbors
    //don't have to bother w blocks, stp should send everywhere
    //don't need send last one. last one is user
    for (int i = 0; i < node->num_neighbors; i++) {
        mixnet_packet* discover_packet = initialize_STP_packet(node->root_addr,node->path_len,node -> my_addr);
        // print_packet(discover_packet);
        mixnet_send(handle, i, discover_packet); //TODO error_handling
        // if (!sent){
        //     printf("error sending STP packet \n");
        // }
        // else {
        //     const char* val = (node->neighbors_addy[i] == -1) ? "NULL" : "NOT NULL";
        //     if (strcmp(val, "NULL") == 0) {
        //         printf("Success! STP packet sent to NULL\n");
        //     } else {
        //         printf("Success! STP packet sent to %d\n", node->neighbors_addy[i]);
        //     }
        // }
    }
        // printf("-------------------------------------ending STP sending from %u--------------------\n", node->my_addr);
}


  /**
   * @brief This function sends Flood Packet, and takes note of the sender port
   *
   * @param handle
   * @param node
   * @param sender_port (do not send back)
   */
void send_FLOOD(void *const handle, struct node *node, uint16_t sender_port) {
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
        if (sender_port != node->num_neighbors){
            // printf("[FLOOD ENDS] Sending out Flood to my USER! \n");
            mixnet_packet* flood_packet = initialize_FLOOD_packet(node->root_addr,node->path_len,node->my_addr);
            mixnet_send(handle, node->num_neighbors, flood_packet);
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
    // print_node(node);


    //allocate chunk of memory
    mixnet_packet *packet_buffer =
        (mixnet_packet *)malloc(sizeof(MAX_MIXNET_PACKET_SIZE));
    if (packet_buffer == NULL) {
        exit(1);
    }

    send_STP_from_node(handle, node); // SEND STP 

    // printf("Set up neighbours");
    // for (int times = 0; times < node->num_neighbors; times ++){
    //     uint8_t tmp;
    //     bool recv = mixnet_recv(handle, &tmp, &packet_buffer);
    //     if (recv){
    //         if (!receive_STP(node, tmp, packet_buffer, handle)){
    //             printf("Error with receiving STP for Neighbor Discovery");
    //         };
    //     }
    // }
    
    // print_node(node);
    
    uint32_t start_time = get_time_in_ms();
    // printf("Starting time: %d \n", start_time);

    uint16_t num_user_packets = 0;
    // uint8_t node_id = node->my_addr; 

    while(*keep_running) {
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
        uint32_t curr_time = get_time_in_ms();
        if (!recv) {
            // ROOT HELLO
            if (node->root_addr == node->my_addr && curr_time - start_time >= config.root_hello_interval_ms) {
                // printf("Starting time: %lu \n", curr_time);
                // printf("Node: %d did not receive, and I AM ROOT so sending hello!' \n", node_id);
                send_STP_from_node(handle, node);
                start_time = get_time_in_ms();
            }

            // RE-ELECT SELF AS ROOT
            else if (node->root_addr != node->my_addr && curr_time - start_time >= config.reelection_interval_ms) {
                // printf("Node: %d not receive, i am NOT root but exceed REELECTION interval, so I AM ROOT!' \n", node_id);
                node->root_addr = node->my_addr;
                node->path_len = 0;
                node->next_hop = node->my_addr;
                for (int i = 0; i < config.num_neighbors; i++) {
                    node->neighbors_blocked[i] = false;
                }
                send_STP_from_node(handle, node);
                start_time = get_time_in_ms();
            }
        } 
        // RECEIVED PACKET 
        else if (recv) {
            // printf("Node: %d received packet! @ Port : %d \n", node_id, port);
            // printf("Packet Type : %s \n", get_packet_type(packet_buffer));
            // printf("RECEIVED Packet! \n");
            mixnet_packet_stp *update = (mixnet_packet_stp *)malloc(sizeof(mixnet_packet_stp));
            memcpy((void *)update, (void *)packet_buffer->payload,
    sizeof(mixnet_packet_stp));
        // uint16_t sender_id = update->node_address;
        free(update);

            switch (packet_buffer->type) {
            case PACKET_TYPE_STP:
                // RESET if from parent
                if (node->neighbors_addy[port] == node->next_hop){
                    start_time = get_time_in_ms();
                }
                receive_STP(node, port, packet_buffer, handle);
                break;
            case PACKET_TYPE_FLOOD:

            //only send to other neighbors
                send_FLOOD(handle, node, port);
                break;
            }
        }
        }

    // printf("\n Node[#%d] Stats: \n | Received %d user packets | Node[#%d] stopped running \n", node->my_addr, num_user_packets, node->my_addr);
    print_node(node);
}

// Using update_packet we check for the [3] cases and destructively update CurrNode if there is a need and return if [state is changed] : bool
bool check_update(mixnet_packet_stp* update, struct node * currNode){

    if (update->root_address < currNode->root_addr) {
            // printf("[1] Updating.. root[%d] address of because received lower ROOT addr \n", currNode->my_addr);
            currNode->root_addr = update->root_address;
            currNode->path_len = update->path_length + 1;
            currNode->next_hop = update->node_address;
            return true;
        }
        // Received lower_path_length
        else if (update->root_address == currNode->root_addr &&
                 update->path_length + 1 < currNode->path_len) {
            // printf("[2] Updated root[%d] because better path\n", currNode->my_addr);
            currNode->path_len = update->path_length + 1;
            currNode->next_hop = update->node_address;
            return true;
        }
        // Received lower_address for next_hop
        else if (update->root_address == currNode->root_addr &&
                 update->path_length + 1 == currNode->path_len &&
                 update->node_address < currNode->next_hop) {
            // printf("[3] pdated root[%d] cause update's node is better for next hop\n", currNode->my_addr);
            currNode->next_hop = update->node_address;
            return true;
        }
    return false;

}

bool receive_STP(struct node * currNode, uint8_t port, mixnet_packet* stp_packet, void *const handle){
    // printf("\n[Received STP packet!]\n");

    mixnet_packet_stp *update = (mixnet_packet_stp *)malloc(sizeof(mixnet_packet_stp)); // Free-d
    
    memcpy((void *)update, (void *)stp_packet->payload,
           sizeof(mixnet_packet_stp));


    // if (update->node_address == currNode->my_addr) {
    //     printf("Received STP packet from myself, ignoring \n");
    //     return false;
    // }

    // mixnet_address old_root;

    // Set my Neighbors cause they are null
    if (currNode->neighbors_addy[port] == -1) {
        // printf("Updating neighbors list because it was NULL \n");
        currNode->neighbors_addy[port] = update->node_address;
        currNode->neighbors_blocked[port] = false; // unblock
        return true;
    } 
    // Neighbors are set
    assert(currNode->neighbors_addy[port] != -1);
    bool state_changed = check_update(update, currNode);

    // if STATE_CHANGED!
    if (state_changed){
        // send out the current to all neighbors except sender first
        for (int i = 0; i < currNode->num_neighbors; i++) {
            if (i != port){
                mixnet_packet* discover_packet = initialize_STP_packet(currNode->root_addr,currNode->path_len,currNode->my_addr);
                // print_packet(discover_packet);
                mixnet_send(handle, i, discover_packet); //TODO error_handl
            }
        }
        
        // Reset All Nodes as Blocked [TODO : combine with top]
        for (int i = 0; i < currNode->num_neighbors; i++) {
            currNode->neighbors_blocked[i] = true;
        }

        
    }


// ================== UNBLOCKING LOGIC =====================

    // UNBLOCK parents and broadcast STP
    if (update->root_address == currNode->root_addr &&
            update->path_length + 1 == currNode->path_len && !state_changed){

        printf("Reaffirming Packet From Parent\n");

        for (int i = 0; i < currNode->num_neighbors; i++) {
            if (i != port){
                mixnet_packet* discover_packet = initialize_STP_packet(currNode->root_addr,currNode->path_len,currNode->my_addr);
                // print_packet(discover_packet);
                mixnet_send(handle, i, discover_packet); //TODO error_handl
            }
        }
        currNode->neighbors_blocked[port] = false;
        // Reset election timer
        return true;
    }

    // Unblock for a potential / direct children
    if (update->root_address == currNode->root_addr &&
        update->path_length == currNode->path_len + 1){
        currNode->neighbors_blocked[port] = false;
        return true;
    }
// ================== UNBLOCKING LOGIC =====================
    

    // if (update->root_address > currNode->root_addr){
    //     printf("Unblocking Node#%d, 's NB, Node#%d: \n", currNode->my_addr, update->node_address);
    //     currNode->neighbors_blocked[port] = false;
    //     return true;
    // }

    // // Case 2 : Same Root, 
    // // - 2.1 : Siblings ( same len)
    // if (update->root_address == currNode->root_addr &&
    //         update->path_length == currNode->path_len){
    //         printf("Blocking Node#%d's Sibling [Node#%d] cause they are siblings\n", currNode->my_addr, update->node_address);
    //         currNode->neighbors_blocked[port] = true;
    //         return true;
    // }



    // For potential parent, i as a children, block my update[parent]
    // same root as you and whose path length + 1== your path length
    // if (update->root_address == currNode->root_addr &&
    //         update->path_length + 1 == currNode->path_len && 
    //         update->node_address > currNode->next_hop){
    //         printf("Node #%d, blocking my a strictly worst Parent(Node#%d) \n", currNode->my_addr, update->node_address);
    //         currNode->neighbors_blocked[port] = true;
    //         return true;
    // }

// }

    // free(update);
    // printf("[End of STP packet!]\n\n");
    return true;
}
