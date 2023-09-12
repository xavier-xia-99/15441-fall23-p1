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


// Declare functions
void receive_and_update(void *const handle, struct node *node);
void send_packet(void *const handle, struct node *node, enum mixnet_packet_type_enum type);
void receive_STP(struct node *currNode, uint8_t i, mixnet_packet *stp_packet);

  /**
   * @brief This function is called send STPS constatnly
   *
   * @param handle
   * @param node
   */
  void send_packet(void *const handle, struct node *node,
                  enum mixnet_packet_type_enum type) {
    //initialize a packet and send to all neighbors
    //don't have to bother w blocks, stp should send everywhere

    //don't need send last one. last one is user
    if (type == PACKET_TYPE_STP){
        for (int i = 0; i < *node->neighbors_addy - 1; i++) {
                mixnet_packet* discover_packet = initialize_STP_packet(node->root_addr,node->path_len,node -> my_addr);
                bool sent = mixnet_send(handle, i, discover_packet); //TODO error_handling
                if (!sent){
                    printf("error sending STP packet \n");
                }
        }
    }

    //send all instead of 
    else if (type == PACKET_TYPE_FLOOD){
         for (int i = 0; i < *node->neighbors_addy; i++) {
                if (!node->neighbors_blocked[i]){
                    mixnet_packet* flood_packet = initialize_FLOOD_packet(node->root_addr,node->path_len,node -> my_addr);
                    bool sent = mixnet_send(handle, i, flood_packet); //TODO error_handling
                    if (!sent) {
                      printf("Error sending Flood packet \n");
                    }
                }
        }
    }
}


// Print Packet Payload

// WE cant print all types here, payload diff...(size wise), 

// just print in init? can try 
//the point is just print everytime we send and everytime we receive right?
//why not we just print whetehr we send and recv first, instead of what packet contains? e
// also can, but eventually need print pakcet i feel , print inside is okay tbh

//
//ok then we j make now 
void print_packet(mixnet_packet *packet) {
    //  Free-d
    printf("------------------------------------------------------------------------");
    printf("PACKET TYPE: %d \n", packet->type);


    // print_STP()
    // mixnet_packet_stp *update =
    //     (mixnet_packet_stp *)malloc(sizeof(mixnet_packet_stp)); //
    // memcpy((void *)update, (void *)packet->payload,
    //        sizeof(mixnet_packet_stp));
    // printf("Printing Packet! \n");
    // printf("Root address: %d \n", update->root_address);
    // printf("Path length: %d \n", update->path_length);
    // printf("Node address: %d \n", update->node_address);
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
    send_packet(handle, node, PACKET_TYPE_STP); // SEND STP 
    receive_and_update(handle, node);

    clock_t start_time = clock();

    //allocate chunk of memory
    mixnet_packet *packet_buffer =
        (mixnet_packet *)malloc(sizeof(MAX_MIXNET_PACKET_SIZE));
    if (packet_buffer == NULL) {
        exit(1);
    }

    while(*keep_running) {

        // receiving from all my neighbors
        for (uint8_t i = 0; i < node->num_neighbors; i++) {
                // check if i received anything
                bool recv = mixnet_recv(handle, &i, &packet_buffer);
                
                // we didn't receive anything
                if (!recv) {
                    // i'm root, and its time to send a root hello
                    if (node->root_addr == node->my_addr && clock() - start_time >= config.root_hello_interval_ms) {
                        send_packet(handle, node, PACKET_TYPE_STP);
                        start_time = clock();
                    }

                    // hey, i'm not the root, but i haven't got a root hello in a while. i'm root now!
                    else if (node->root_addr != node->my_addr && clock() - start_time >= config.reelection_interval_ms) {
                        node->root_addr = node->my_addr;
                        node->path_len = 0;
                        node->next_hop = node->my_addr;
                        send_packet(handle, node, PACKET_TYPE_STP);
                        start_time = clock();
                    }
                } 
                // we received something. 
                else if (recv) {
                    switch (packet_buffer->type) {
                    case PACKET_TYPE_STP:
                        if (node->root_addr != node->my_addr) {
                            receive_STP(node, i, packet_buffer);
                            start_time = clock();
                        }
                      break;
                    case PACKET_TYPE_FLOOD:
                      send_packet(handle, node, PACKET_TYPE_FLOOD);
                      break;
                    }
                }
        }
    }
}

void receive_STP(struct node * currNode, uint8_t i, mixnet_packet* stp_packet){
    
    mixnet_packet_stp *update = (mixnet_packet_stp *)malloc(sizeof(mixnet_packet_stp)); // Free-d
    
    memcpy((void *)update, (void *)stp_packet->payload,
           sizeof(mixnet_packet_stp));

    // Update if the neighbors list are NULL
    if (currNode->neighbors_addy[i] == -1) {
        currNode->neighbors_addy[i] = update->node_address;
        currNode->neighbors_blocked[i] = false;
    } else {
        bool updated = false;
        // Received lower_root_address
        if (update->root_address < currNode->root_addr) {
                    currNode->root_addr = update->root_address;
                    currNode->path_len = update->path_length + 1;
                    currNode->next_hop = update->node_address;
                    updated = true;
                    printf("updated root address because received lower address \n");
        }
        // Received lower_path_length
        else if (update->root_address == currNode->root_addr &&
                 update->path_length + 1 < currNode->path_len) {
                    currNode->path_len = update->path_length + 1;
                    currNode->next_hop = update->node_address;
                    updated = true;
                    printf("updated root address because better path\n");
        }
        // Received lower_address for next_hop
        else if (update->root_address == currNode->root_addr &&
                 update->path_length + 1 == currNode->path_len &&
                 update->node_address < currNode->next_hop) {
                    currNode->next_hop = update->node_address;
                    updated = true;
                    printf("updated root address cause update's node is better for next hop\n");
        }

        // If this update_packet was not useful, block the port
        if (!updated) {
                    currNode->neighbors_blocked[i] = true;
        }
    }
    free(update);
}

/**
 * @brief // Given a node, set the neighbors list of the neighbors interms of
 * uint8_t
 *
 * @param handle
 * @param currNode
 */
void receive_and_update(void *const handle, struct node *currNode) {
    (void)handle;
    mixnet_packet *packet =
        (mixnet_packet *)malloc(sizeof(MAX_MIXNET_PACKET_SIZE));
    for (uint8_t i = 0; i < currNode->num_neighbors; i++) {
        // Init Header
        if (packet == NULL) {
            exit(1);
        }
        bool recv = mixnet_recv(handle, &i, &packet);
        if (!recv) {

                    return;
        } else if (recv) {
                    switch (packet->type) {
                    case PACKET_TYPE_STP:
                    receive_STP(currNode, i, packet);
                    // Do nothing
                    break;
                    case PACKET_TYPE_FLOOD:
                    send_packet(handle, currNode, PACKET_TYPE_FLOOD);
                    break;
                    }
        }
        free(packet);
    }
}
