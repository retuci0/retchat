#pragma once

#include <cstdint>


namespace Retchat {

    // packet types
    enum PacketType : uint8_t {
        PKT_HANDSHAKE      = 0x01,  // raw DH public key
        PKT_KEEPALIVE      = 0x02,  // c2s: keep connection alive
        PKT_KEEPALIVE_ACK  = 0x03,  // s2c: keep alive ack
        PKT_NICK_REQUEST   = 0x10,  // c2s: new nickname
        PKT_NICK_ACK       = 0x11,  // s2c: nickname changed
        PKT_NICK_NOTIFY    = 0x12,  // s2c: someone changed nickname
        PKT_JOIN_REQUEST   = 0x13,  // c2s: join room
        PKT_JOIN_ACK       = 0x14,  // s2c: joined room
        PKT_JOIN_NOTIFY    = 0x15,  // s2c: someone joined
        PKT_LEAVE_NOTIFY   = 0x16,  // s2c: someone left
        PKT_ROOM_LIST      = 0x17,  // s2c: list of rooms
        PKT_USER_LIST      = 0x18,  // s2c: list of users in current room
        PKT_CHAT_MSG       = 0x20,  // s2c: chat message
        PKT_SYSTEM_MSG     = 0x21,  // s2c: system message
        PKT_DM_REQUEST     = 0x22,  // c2s: direct message
        PKT_DM_MSG         = 0x23,  // s2c: direct message received
        PKT_IMAGE_MSG      = 0x24,  // image payload
        PKT_DISCONNECT     = 0x30,  // s2c: disconnected
        PKT_KICK           = 0x31,  // s2c: kicked
        PKT_BAN            = 0x32   // s2c: banned
    };

    // header: type(1), flags(1), length(2 big-endian)
    struct PacketHeader {
        uint8_t type;
        uint8_t flags;
        uint16_t length;
    } __attribute__((packed));


    enum SystemMessageCode : uint16_t {
        MSG_WELCOME              = 1,
        MSG_NICK_EMPTY           = 2,
        MSG_NICK_TOO_LONG        = 3,
        MSG_NICK_INVALID_CHARS   = 4,
        MSG_NICK_SAME            = 5,
        MSG_NICK_BANNED          = 6,
        MSG_NICK_TAKEN           = 7,
        MSG_JOIN_ALREADY         = 8,
        MSG_JOIN_NAME_TAKEN      = 9,
        MSG_DM_TARGET_NOT_FOUND  = 10,
        MSG_IMAGE_UNSUPPORTED    = 11,
    };

}