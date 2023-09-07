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
#include "message.h"

#include <assert.h>
#include <string.h>
#include <type_traits>

namespace framework {

message::message() {
    // Allocate the internal message buffer
    buffer_ = new char[MAX_MESSAGE_LENGTH];
    assert(buffer_ != nullptr); // Sanity check
}

message::~message() { delete [] buffer_; }

/**
 * Accessors.
 */
bool message::is_request() const {
    return code_is_request(get_header()->message_code);
}

message::type message::get_type() const {
    return code_to_type(get_header()->message_code);
}

bool message::validate() const {
    const length_t length = compute_total_length();
    if ((length < MIN_MESSAGE_LENGTH) ||
        (length > MAX_MESSAGE_LENGTH)) {
        return false;
    }
    auto canary_ptr = (
        reinterpret_cast<decltype(canary)*>(
            buffer_ + length - sizeof(canary)));

    return ((*canary_ptr) == canary);
}

error_code message::get_error_code() const {
    auto val = get_header()->error_code;
    assert(val < static_cast<uint16_t>(
        error_code::ENUMERATION_LIMIT));

    return static_cast<error_code>(val);
}

uint16_t message::get_fragment_id() const {
    return get_header()->fragment_id;
}

message::length_t message::get_total_length() const {
    return get_header()->total_length;
}

message::length_t message::compute_total_length() const {
    bool request = is_request(); type type = get_type();
    length_t length = 0;

    switch (type) {
    case type::SETUP_CTRL: {
        length = (request ?
            request::setup_ctrl::length() :
            response::setup_ctrl::length()
        );
    } break;

    case type::SETUP_PCAP: {
        length = (request ?
            request::setup_pcap::length() :
            0
        );
    } break;

    case type::TOPOLOGY: {
        length = (request ?
            payload<request::topology>()->length() :
            0
        );
    } break;

    case type::START_MIXNET_SERVER: {
        length = (request ?
            0 :
            response::start_mixnet_server::length()
        );
    } break;

    case type::START_MIXNET_CLIENTS: {
        length = (request ?
            payload<request::start_mixnet_clients>()->length() :
            payload<response::start_mixnet_clients>()->length()
        );
    } break;

    case type::RESOLVE_MIXNET_CONNS: {
        length = (request ?
            payload<request::resolve_mixnet_connections>()->length() :
            0
        );
    } break;

    case type::CHANGE_LINK_STATE: {
        length = (request ?
            request::change_link_state::length() :
            0
        );
    } break;

    case type::PCAP_DATA: {
        length = (request ?
            0 :
            payload<mixnet_packet>()->total_size
        );
    } break;

    case type::PCAP_SUBSCRIPTION: {
        length = (request ?
            request::pcap_subscription::length() :
            0
        );
    } break;

    case type::SEND_PACKET: {
        length = (request ?
            payload<request::send_packet>()->length() :
            0
        );
    } break;

    case type::START_TESTCASE:
    case type::END_TESTCASE:
    case type::SHUTDOWN: {
        length = 0;
    }
    break;

    // Unknown message type
    default: { assert(false); } break;
    } // switch

    // {Header, payload, canary}
    return (sizeof(header) + length + sizeof(canary));
}

/**
 * Mutators.
 */
void message::clear() {
    memset(buffer_, 0, sizeof(header));
}

void message::finalize() {
    const length_t length = (
        compute_total_length());

    // Sanity checks
    assert(length >= MIN_MESSAGE_LENGTH);
    assert(length <= MAX_MESSAGE_LENGTH);

    auto canary_ptr = reinterpret_cast<
        std::decay<decltype(canary)>::type*>(
            buffer_ + length - sizeof(canary));

    // Update the message length and canary
    get_header()->total_length = length;
    *canary_ptr = canary;
}

void message::set_fragment_id(const uint16_t fragment_id) {
    get_header()->fragment_id = fragment_id;
}

void message::set_error_code(const error_code error_code) {
    get_header()->error_code = static_cast<uint16_t>(error_code);
}

void message::set_code(const bool is_request, const type type) {
    get_header()->message_code = code_create(is_request, type);
}

/**
 * Internal helper methods.
 */
message::type message::code_to_type(const uint16_t code) {
    const uint16_t code_type = (code & 0x7FFF);
    assert(code_type < static_cast<uint16_t>(
        type::ENUMERATION_LIMIT));

    return static_cast<type>(code_type);
}

bool message::code_is_request(const uint16_t code) {
    return ((code & 0x8000) == 0);
}

uint16_t message::code_create(const bool is_request, const type type) {
    return static_cast<uint16_t>(
        ((!is_request) << 15) | ((static_cast<uint16_t>(type)) & 0x7FFF));
}

} // namespace framework
