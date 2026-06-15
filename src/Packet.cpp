#include "Packet.hpp"

#include <arpa/inet.h>
#include <cstring>


namespace Retchat {

    std::vector<uint8_t> serializeString(const std::string& str) {
        std::vector<uint8_t> out(str.begin(), str.end());
        out.push_back(0);
        return out;
    }

    std::string deserializeString(const uint8_t* data, size_t& offset) {
        const uint8_t* start = data + offset;
        while (data[offset] != 0) offset++;
        std::string result(reinterpret_cast<const char*>(start), offset - (start - data));
        offset++;  // skip null
        return result;
    }

    void Packet::serialize(std::vector<uint8_t>& out) const {
        out.insert(out.end(), payload.begin(), payload.end());
    }

    bool Packet::deserialize(const uint8_t* data, size_t len) {
        payload.assign(data, data + len);
        return true;
    }

    Packet* Packet::create(PacketType type) {
        switch (type) {
            case PKT_NICK_REQUEST: return new NickRequestPacket();
            case PKT_NICK_ACK: return new NickAckPacket();
            case PKT_NICK_NOTIFY: return new NickNotifyPacket();
            case PKT_JOIN_REQUEST: return new JoinRequestPacket();
            case PKT_JOIN_ACK: return new JoinAckPacket();
            case PKT_JOIN_NOTIFY: return new JoinNotifyPacket();
            case PKT_LEAVE_NOTIFY: return new LeaveNotifyPacket();
            case PKT_ROOM_LIST: return new RoomListPacket();
            case PKT_USER_LIST: return new UserListPacket();
            case PKT_CHAT_MSG: return new ChatPacket();
            case PKT_SYSTEM_MSG: return new SystemPacket();
            case PKT_DISCONNECT: return new DisconnectPacket();
            default: return nullptr;
        }
    }

    // --- NickRequestPacket ---
    void NickRequestPacket::serialize(std::vector<uint8_t>& out) const {
        auto s = serializeString(newNick);
        out.insert(out.end(), s.begin(), s.end());
    }
    bool NickRequestPacket::deserialize(const uint8_t* data, size_t len) {
        size_t off = 0;
        newNick = deserializeString(data, off);
        return off == len;
    }

    // --- NickAckPacket ---
    void NickAckPacket::serialize(std::vector<uint8_t>& out) const {
        auto s = serializeString(newNick);
        out.insert(out.end(), s.begin(), s.end());
    }
    bool NickAckPacket::deserialize(const uint8_t* data, size_t len) {
        size_t off = 0;
        newNick = deserializeString(data, off);
        return off == len;
    }

    // --- NickNotifyPacket ---
    void NickNotifyPacket::serialize(std::vector<uint8_t>& out) const {
        auto s1 = serializeString(oldNick);
        auto s2 = serializeString(newNick);
        out.insert(out.end(), s1.begin(), s1.end());
        out.insert(out.end(), s2.begin(), s2.end());
    }
    bool NickNotifyPacket::deserialize(const uint8_t* data, size_t len) {
        size_t off = 0;
        oldNick = deserializeString(data, off);
        newNick = deserializeString(data, off);
        return off == len;
    }

    // --- JoinRequestPacket ---
    void JoinRequestPacket::serialize(std::vector<uint8_t>& out) const {
        auto s = serializeString(roomName);
        out.insert(out.end(), s.begin(), s.end());
    }
    bool JoinRequestPacket::deserialize(const uint8_t* data, size_t len) {
        size_t off = 0;
        roomName = deserializeString(data, off);
        return off == len;
    }

    // --- JoinAckPacket ---
    void JoinAckPacket::serialize(std::vector<uint8_t>& out) const {
        auto s = serializeString(roomName);
        out.insert(out.end(), s.begin(), s.end());
    }
    bool JoinAckPacket::deserialize(const uint8_t* data, size_t len) {
        size_t off = 0;
        roomName = deserializeString(data, off);
        return off == len;
    }

    // --- JoinNotifyPacket ---
    void JoinNotifyPacket::serialize(std::vector<uint8_t>& out) const {
        auto s = serializeString(nick);
        out.insert(out.end(), s.begin(), s.end());
    }
    bool JoinNotifyPacket::deserialize(const uint8_t* data, size_t len) {
        size_t off = 0;
        nick = deserializeString(data, off);
        return off == len;
    }

    // --- LeaveNotifyPacket ---
    void LeaveNotifyPacket::serialize(std::vector<uint8_t>& out) const {
        auto s = serializeString(nick);
        out.insert(out.end(), s.begin(), s.end());
    }
    bool LeaveNotifyPacket::deserialize(const uint8_t* data, size_t len) {
        size_t off = 0;
        nick = deserializeString(data, off);
        return off == len;
    }

    // --- RoomListPacket ---
    void RoomListPacket::serialize(std::vector<uint8_t>& out) const {
        for (const auto& r : rooms) {
            auto s = serializeString(r);
            out.insert(out.end(), s.begin(), s.end());
        }
    }
    bool RoomListPacket::deserialize(const uint8_t* data, size_t len) {
        size_t off = 0;
        while (off < len) {
            rooms.push_back(deserializeString(data, off));
        }
        return off == len;
    }

    // --- UserListPacket ---
    void UserListPacket::serialize(std::vector<uint8_t>& out) const {
        for (const auto& u : users) {
            auto s = serializeString(u);
            out.insert(out.end(), s.begin(), s.end());
        }
    }
    bool UserListPacket::deserialize(const uint8_t* data, size_t len) {
        size_t off = 0;
        while (off < len) {
            users.push_back(deserializeString(data, off));
        }
        return off == len;
    }

    // --- ChatPacket ---
    void ChatPacket::serialize(std::vector<uint8_t>& out) const {
        auto s1 = serializeString(sender);
        auto s2 = serializeString(text);
        out.insert(out.end(), s1.begin(), s1.end());
        out.insert(out.end(), s2.begin(), s2.end());
    }
    bool ChatPacket::deserialize(const uint8_t* data, size_t len) {
        size_t off = 0;
        sender = deserializeString(data, off);
        text = deserializeString(data, off);
        return off == len;
    }

    // --- SystemPacket ---
    void SystemPacket::serialize(std::vector<uint8_t>& out) const {
        out.push_back(isError ? 1 : 0);
        auto s = serializeString(text);
        out.insert(out.end(), s.begin(), s.end());
    }
    bool SystemPacket::deserialize(const uint8_t* data, size_t len) {
        size_t off = 0;
        isError = data[off++] != 0;
        text = deserializeString(data, off);
        return off == len;
    }

    // --- DisconnectPacket ---
    void DisconnectPacket::serialize(std::vector<uint8_t>& out) const {
        // no payload
    }
    bool DisconnectPacket::deserialize(const uint8_t* data, size_t len) {
        return len == 0;
    }

}