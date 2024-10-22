#include "node.h"

#include "connection.h"
#include "packet.h"

#include "packet_init.h"
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
