#include "stp.h"


// Does not reach this function at all
bool receive_STP(struct Node * currNode, uint8_t port, mixnet_packet* stp_packet, void* handle){
    // printf("\n[Received STP packet!]\n");
    mixnet_packet_stp *update = (mixnet_packet_stp *)malloc(sizeof(mixnet_packet_stp)); 
    
    memcpy((void *)update, (void *)stp_packet->payload,
           sizeof(mixnet_packet_stp));

    if (currNode->neighbors_addy[port] == INVALID_MIXADDR) {
        currNode->neighbors_addy[port] = update->node_address;
        currNode->neighbors_blocked[port] = false; // unblock
    }

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

        // currNode->neighbors_blocked[old_root] = 
    }
    // BLOCK LOGIC
    // Parent is unblocked
    if (update->node_address == currNode->next_hop){
        currNode->neighbors_blocked[port] = false; 
    }

    // Unblock potential children 
    else if (update->root_address == currNode->root_addr &&
            update->path_length == currNode->path_len + 1){
            // printf("Node #%d, blocking my potential Parent, who is strictly worse \n", currNode->my_addr);
        currNode->neighbors_blocked[port] = false;
    } else {
        currNode->neighbors_blocked[port] = true;
    }
        
    // free(update);
    return true;
}


  /**
   * @brief This function is called send STP constantly
   *     initialize a packet and send to all neighbors
   *     don't have to bother w blocks, stp should send everywhere
   *     node->num_neighbors is the port for the user itself
   *
   * @param handle
   * @param node
   */
void send_STP(void *const handle, struct Node *node) {
    for (int i = 0; i < node->num_neighbors; i++) {
        // printf("Sending out STP to %d \n", node->neighbors_addy[i]);
        mixnet_packet* discover_packet = initialize_STP_packet(node->root_addr,node->path_len,node->my_addr);
        // print_packet(discover_packet);
        mixnet_send(handle, i, discover_packet); 
    }
}


  /**
   * @brief This function sends Flood Packet, and takes note of the sender port. 
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
