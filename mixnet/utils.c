#include "node.h"

#include "connection.h"
#include "packet.h"

#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/*
STP Packet is made up of 3 fields

1. Root Address (Global)
2. Path Length 
3. My Node Address (Local)


typedef struct mixnet_packet_stp {
    mixnet_address root_address;        // Root of the spanning tree
    uint16_t path_length;               // Length of path to the root
    mixnet_address node_address;        // Current node's mixnet address

}


*/ 

mixnet_packet* initialize_STP_packet(mixnet_address root_address,
                                   u_int16_t path_length,
                                   mixnet_address node_address) {
                                    
  mixnet_packet *stp_packet = (mixnet_packet *)malloc(
      sizeof(mixnet_packet) + sizeof(mixnet_packet_stp));

  if (stp_packet == NULL) {
    exit(1);
  }
  // Initialize mixnet_packet fields
  stp_packet->total_size = 18;
  stp_packet->type = PACKET_TYPE_STP;

  // Allocate memory for the mixnet_packet_stp payload
  mixnet_packet_stp *stp_payload =
      (mixnet_packet_stp *)malloc(sizeof(mixnet_packet_stp));
  if (!stp_payload) {
    // Handle allocation failure
    free(stp_packet);
    exit(1);
  }
  // Initialize mixnet_packet_stp fields
  stp_payload->root_address = root_address;
  stp_payload->path_length = path_length;
  stp_payload->node_address = node_address;

  // Point packet's payload to stp_payload
  memcpy((void *)stp_packet->payload, (void *)stp_payload,
         sizeof(mixnet_packet_stp));

  return stp_packet;
}
//identify? wym oh like u mean fi the mem thing we r doing is correct is it
//shld be correct la u maxmined it ma yep, thats why ya Ok
// case on packet what,like same as init. 
// yeah cause we receive then allocate size , i think is fine, then memcpy the payload over. oh ya

// we also allocated packet to MAXMINXETSIZE right yeaj, ask him if thats fine also, should be la
mixnet_packet* initialize_FLOOD_packet(mixnet_address root_address,
                                   u_int16_t path_length,
                                   mixnet_address node_address) {
                                    
  mixnet_packet *stp_packet = (mixnet_packet *)malloc(
      sizeof(mixnet_packet) + sizeof(mixnet_packet_stp));

  if (stp_packet == NULL) {
    exit(1);
  }
  // Initialize mixnet_packet fields
  stp_packet->total_size = 12;
  stp_packet->type = PACKET_TYPE_FLOOD;

  // Allocate memory for the mixnet_packet_stp payload
  mixnet_packet_stp *stp_payload =
      (mixnet_packet_stp *)malloc(sizeof(mixnet_packet_stp));
  if (!stp_payload) {
    // Handle allocation failure
    free(stp_packet);
    exit(1);
  }

  // Initialize mixnet_packet_stp fields
  stp_payload->root_address = root_address;
  stp_payload->path_length = path_length;
  stp_payload->node_address = node_address;

  // Point packet's payload to stp_payload
  memcpy((void *)stp_packet->payload, (void *)stp_payload,
         sizeof(mixnet_packet_stp));
  
  return stp_packet;
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
