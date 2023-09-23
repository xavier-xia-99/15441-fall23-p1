#include "node.h"

#include "connection.h"
#include "packet.h"

#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

 
mixnet_packet* initialize_STP_packet(mixnet_address root_address,
                                   uint16_t path_length,
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

// [DONE..]
mixnet_packet* initialize_LSA_packet(mixnet_address node_addr, uint8_t nb_count, mixnet_address* nb_addrs, uint16_t* nb_costs ){

  // Init to Max
  mixnet_packet *LSA_packet = (mixnet_packet *)malloc(
      sizeof(mixnet_packet) + sizeof(4 + 4 * MAX_MIXNET_ROUTE_LENGTH));

  //TODO : Check if this is correct
  LSA_packet->total_size = 12 + 4 + 4 * nb_count;
  LSA_packet->type = PACKET_TYPE_LSA;
 
  // Allocating Payload 
  mixnet_packet_lsa *LSA_payload =
      (mixnet_packet_lsa *)malloc(sizeof(mixnet_packet_lsa) + sizeof(mixnet_lsa_link_params) * nb_count);

  mixnet_lsa_link_params* nb_links_params = (mixnet_lsa_link_params *)malloc(sizeof(mixnet_lsa_link_params) * nb_count);
  
  // Combine them into the struct
  for (int i = 0; i < nb_count; i++) {
    nb_links_params[i].neighbor_mixaddr = nb_addrs[i];
    nb_links_params[i].cost = nb_costs[i];
  }

  // Fill up the paylod
  LSA_payload->node_address = node_addr;
  LSA_payload->neighbor_count = nb_count;
  memcpy((void*)LSA_payload->links, (void*)nb_links_params, sizeof(mixnet_lsa_link_params) * nb_count);

  // Attach to the LSA_Packet
  memcpy((void *)LSA_packet->payload, (void *)LSA_payload,
         sizeof(mixnet_packet_lsa) + sizeof(mixnet_lsa_link_params) * nb_count);

  return LSA_packet;
}

// TODO : IMPL
// mixnet_packet* initialize_DATA_packet(mixnet_packet_routing_header* routing_header, void* data){
//     // Init to Max
//     mixnet_packet *DATA_packet = (mixnet_packet *)malloc(
//       sizeof(MAX_MIXNET_PACKET_SIZE));


//     DATA_packet->total_size = 12 + sizeof(mixnet_packet_routing_header) + sizeof(data);
//     DATA_packet->type = PACKET_TYPE_DATA;
//     // memcpy(DATA_packet->payload, routing_header, sizeof(mixnet_packet_routing_header));
//     // // Allocating according to nb_count
//     // mixnet_packet_data *DATA_payload =
//     //     (mixnet_packet_data *)malloc(sizeof(mixnet_packet_data) + sizeof(mixnet_packet_routing_header) + sizeof(data));

//     // DATA_payload->routing_header = *routing_header;
//     // DATA_payload->data = data;

//     return DATA_packet;    

// }


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
        case PACKET_TYPE_LSA:
        return "LSA";
        case PACKET_TYPE_DATA:
        return "DATA";
        case PACKET_TYPE_PING:
        return "PING";
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
