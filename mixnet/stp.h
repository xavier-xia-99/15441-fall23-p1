#ifndef STP_H
#define STP_H

#include "address.h"
#include "packet.h"
#include "config.h"
#include "node.h"
#include "utils.h"
#include "connection.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

struct Node;

bool receive_STP(struct Node *currNode, uint8_t i, mixnet_packet *stp_packet, void* handle);
void send_FLOOD(void *const handle, struct Node *node, uint16_t sender_port);
void send_STP(void *const handle, struct Node *node);

#endif
