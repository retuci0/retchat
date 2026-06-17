#pragma once

#include "Protocol.hpp"

#include <string>
#include <cstring>
#include <vector>


namespace Retchat {

    class Packet {
    public:
        PacketType type;
        std::vector<uint8_t> payload;

        virtual ~Packet() = default;
        virtual void serialize(std::vector<uint8_t>& out) const;
        virtual bool deserialize(const uint8_t* data, size_t len);
        static Packet* create(PacketType type);
    };

    std::vector<uint8_t> serializeString(const std::string& str);
    std::string deserializeString(const uint8_t* data, size_t& offset);

    
    class KeepAlivePacket : public Packet {
    public:
        KeepAlivePacket() { type = PKT_KEEPALIVE; }
        void serialize(std::vector<uint8_t>& out) const override;
        bool deserialize(const uint8_t* data, size_t len) override;
    };

    class KeepAliveAckPacket : public Packet {
    public:
        KeepAliveAckPacket() { type = PKT_KEEPALIVE_ACK; }
        void serialize(std::vector<uint8_t>& out) const override;
        bool deserialize(const uint8_t* data, size_t len) override;
    };

    class NickRequestPacket : public Packet {
    public:
        std::string newNick;
        NickRequestPacket() { type = PKT_NICK_REQUEST; }
        void serialize(std::vector<uint8_t>& out) const override;
        bool deserialize(const uint8_t* data, size_t len) override;
    };

    class NickAckPacket : public Packet {
    public:
        std::string newNick;
        NickAckPacket() { type = PKT_NICK_ACK; }
        void serialize(std::vector<uint8_t>& out) const override;
        bool deserialize(const uint8_t* data, size_t len) override;
    };

    class NickNotifyPacket : public Packet {
    public:
        std::string oldNick, newNick;
        NickNotifyPacket() { type = PKT_NICK_NOTIFY; }
        void serialize(std::vector<uint8_t>& out) const override;
        bool deserialize(const uint8_t* data, size_t len) override;
    };

    class JoinRequestPacket : public Packet {
    public:
        std::string roomName;
        JoinRequestPacket() { type = PKT_JOIN_REQUEST; }
        void serialize(std::vector<uint8_t>& out) const override;
        bool deserialize(const uint8_t* data, size_t len) override;
    };

    class JoinAckPacket : public Packet {
    public:
        std::string roomName;
        JoinAckPacket() { type = PKT_JOIN_ACK; }
        void serialize(std::vector<uint8_t>& out) const override;
        bool deserialize(const uint8_t* data, size_t len) override;
    };

    class JoinNotifyPacket : public Packet {
    public:
        std::string nick;
        JoinNotifyPacket() { type = PKT_JOIN_NOTIFY; }
        void serialize(std::vector<uint8_t>& out) const override;
        bool deserialize(const uint8_t* data, size_t len) override;
    };

    class LeaveNotifyPacket : public Packet {
    public:
        std::string nick;
        LeaveNotifyPacket() { type = PKT_LEAVE_NOTIFY; }
        void serialize(std::vector<uint8_t>& out) const override;
        bool deserialize(const uint8_t* data, size_t len) override;
    };

    class RoomListPacket : public Packet {
    public:
        std::vector<std::string> rooms;
        RoomListPacket() { type = PKT_ROOM_LIST; }
        void serialize(std::vector<uint8_t>& out) const override;
        bool deserialize(const uint8_t* data, size_t len) override;
    };

    class UserListPacket : public Packet {
    public:
        std::vector<std::string> users;
        UserListPacket() { type = PKT_USER_LIST; }
        void serialize(std::vector<uint8_t>& out) const override;
        bool deserialize(const uint8_t* data, size_t len) override;
    };

    class ChatPacket : public Packet {
    public:
        std::string sender, text;
        ChatPacket() { type = PKT_CHAT_MSG; }
        void serialize(std::vector<uint8_t>& out) const override;
        bool deserialize(const uint8_t* data, size_t len) override;
    };

    class SystemPacket : public Packet {
    public:
        bool isError = false;
        uint16_t code = 0;
        std::vector<std::string> params;

        SystemPacket() { type = PKT_SYSTEM_MSG; }
        void serialize(std::vector<uint8_t>& out) const override;
        bool deserialize(const uint8_t* data, size_t len) override;
    };

    class DisconnectPacket : public Packet {
    public:
        DisconnectPacket() { type = PKT_DISCONNECT; }
        void serialize(std::vector<uint8_t>& out) const override;
        bool deserialize(const uint8_t* data, size_t len) override;
    };

    class KickPacket : public Packet {
    public:
        std::string reason;
        KickPacket() { type = PKT_KICK; }
        void serialize(std::vector<uint8_t>& out) const override;
        bool deserialize(const uint8_t* data, size_t len) override;
    };

    class BanPacket : public Packet {
    public:
        std::string reason;
        BanPacket() { type = PKT_BAN; }
        void serialize(std::vector<uint8_t>& out) const override;
        bool deserialize(const uint8_t* data, size_t len) override;
    };

    class DmRequestPacket : public Packet {
    public:
        std::string targetNick;
        std::string text;
        DmRequestPacket() { type = PKT_DM_REQUEST; }
        void serialize(std::vector<uint8_t>& out) const override;
        bool deserialize(const uint8_t* data, size_t len) override;
    };

    class DmMsgPacket : public Packet {
    public:
        std::string senderNick;
        std::string text;
        DmMsgPacket() { type = PKT_DM_MSG; }
        void serialize(std::vector<uint8_t>& out) const override;
        bool deserialize(const uint8_t* data, size_t len) override;
    };

    class ImagePacket : public Packet {
    public:
        std::string sender;
        std::string target;  // empty for room, otherwise recipient name (for dms)
        std::string mimeType;  // e.g. "image/png"
        std::string fileName;  // may be empty
        std::vector<uint8_t> imageData;

        ImagePacket() { type = PKT_IMAGE_MSG; }
        void serialize(std::vector<uint8_t>& out) const override;
        bool deserialize(const uint8_t* data, size_t len) override;
    };

}