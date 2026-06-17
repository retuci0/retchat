#include "Packet.hpp"
#include "Protocol.hpp"

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
            case PKT_KEEPALIVE:     return new KeepAlivePacket();
            case PKT_KEEPALIVE_ACK: return new KeepAliveAckPacket();
            case PKT_NICK_REQUEST:  return new NickRequestPacket();
            case PKT_NICK_ACK:      return new NickAckPacket();
            case PKT_NICK_NOTIFY:   return new NickNotifyPacket();
            case PKT_JOIN_REQUEST:  return new JoinRequestPacket();
            case PKT_JOIN_ACK:      return new JoinAckPacket();
            case PKT_JOIN_NOTIFY:   return new JoinNotifyPacket();
            case PKT_LEAVE_NOTIFY:  return new LeaveNotifyPacket();
            case PKT_ROOM_LIST:     return new RoomListPacket();
            case PKT_USER_LIST:     return new UserListPacket();
            case PKT_CHAT_MSG:      return new ChatPacket();
            case PKT_SYSTEM_MSG:    return new SystemPacket();
            case PKT_DM_REQUEST:    return new DmRequestPacket();
            case PKT_DM_MSG:        return new DmMsgPacket();
            case PKT_IMAGE_MSG:     return new ImagePacket();
            case PKT_DISCONNECT:    return new DisconnectPacket();
            case PKT_KICK:          return new KickPacket();
            case PKT_BAN:           return new BanPacket();
            default: return nullptr;
        }
    }


    // --- KeepAlivePacket ---
    void KeepAlivePacket::serialize(std::vector<uint8_t>& out) const {
        // no payload
    }
    bool KeepAlivePacket::deserialize(const uint8_t* data, size_t len) {
        return len == 0;
    }

    // --- KeepAliveAckPacket ---
    void KeepAliveAckPacket::serialize(std::vector<uint8_t>& out) const {
        // no payload
    }
    bool KeepAliveAckPacket::deserialize(const uint8_t* data, size_t len) {
        return len == 0;
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
        text = deserializeString(data, off);
        return off == len;
    }

    // --- SystemPacket ---
    void SystemPacket::serialize(std::vector<uint8_t>& out) const {
        out.push_back(isError ? 1 : 0);
        // code (2 bytes, big-endian)
        out.push_back((code >> 8) & 0xFF);
        out.push_back(code & 0xFF);
        // param count
        out.push_back(static_cast<uint8_t>(params.size()));
        for (const auto& p : params) {
            auto s = serializeString(p);
            out.insert(out.end(), s.begin(), s.end());
        }
    }
    bool SystemPacket::deserialize(const uint8_t* data, size_t len) {
        size_t off = 0;
        if (off >= len) return false;
        isError = (data[off++] != 0);
        if (off + 2 > len) return false;
        code = (data[off] << 8) | data[off + 1];
        off += 2;
        if (off >= len) return false;
        uint8_t paramCount = data[off++];
        params.clear();
        for (uint8_t i = 0; i < paramCount; ++i) {
            params.push_back(deserializeString(data, off));
        }
        return off == len;
    }
    // --- DisconnectPacket ---
    void DisconnectPacket::serialize(std::vector<uint8_t>& out) const {}
    bool DisconnectPacket::deserialize(const uint8_t* data, size_t len) { return len == 0; }

    // --- KickPacket ---
    void KickPacket::serialize(std::vector<uint8_t>& out) const {
        auto s = serializeString(reason);
        out.insert(out.end(), s.begin(), s.end());
    }
    bool KickPacket::deserialize(const uint8_t* data, size_t len) {
        size_t off = 0;
        reason = deserializeString(data, off);
        return true;
    }

    // --- BanPacket ---
    void BanPacket::serialize(std::vector<uint8_t>& out) const {
        auto s = serializeString(reason);
        out.insert(out.end(), s.begin(), s.end());
    }
    bool BanPacket::deserialize(const uint8_t* data, size_t len) {
        size_t off = 0;
        reason = deserializeString(data, off);
        return true;
    }

    // --- DmRequestPacket ---
    void DmRequestPacket::serialize(std::vector<uint8_t>& out) const {
        auto s1 = serializeString(targetNick);
        auto s2 = serializeString(text);
        out.insert(out.end(), s1.begin(), s1.end());
        out.insert(out.end(), s2.begin(), s2.end());
    }
    bool DmRequestPacket::deserialize(const uint8_t* data, size_t len) {
        size_t off = 0;
        targetNick = deserializeString(data, off);
        text = deserializeString(data, off);
        return true;
    }

    // --- DmMsgPacket ---
    void DmMsgPacket::serialize(std::vector<uint8_t>& out) const {
        auto s1 = serializeString(senderNick);
        auto s2 = serializeString(text);
        out.insert(out.end(), s1.begin(), s1.end());
        out.insert(out.end(), s2.begin(), s2.end());
    }
    bool DmMsgPacket::deserialize(const uint8_t* data, size_t len) {
        size_t off = 0;
        senderNick = deserializeString(data, off);
        text = deserializeString(data, off);
        return true;
    }

    // --- ImagePacket ---
    void ImagePacket::serialize(std::vector<uint8_t>& out) const {
        auto s1 = serializeString(sender);
        auto s2 = serializeString(target);
        auto s3 = serializeString(mimeType);
        auto s4 = serializeString(fileName);
        out.insert(out.end(), s1.begin(), s1.end());
        out.insert(out.end(), s2.begin(), s2.end());
        out.insert(out.end(), s3.begin(), s3.end());
        out.insert(out.end(), s4.begin(), s4.end());
        out.insert(out.end(), imageData.begin(), imageData.end());
    }
    bool ImagePacket::deserialize(const uint8_t* data, size_t len) {
        size_t off = 0;
        sender   = deserializeString(data, off);
        target   = deserializeString(data, off);
        mimeType = deserializeString(data, off);
        fileName = deserializeString(data, off);
        // remaining bytes are the image
        if (off < len) {
            imageData.assign(data + off, data + len);
        }
        return true;
    }

}