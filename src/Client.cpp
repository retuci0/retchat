#include "Client.hpp"

#include "DiffieHellman.hpp"
#include "Logger.hpp"
#include "Protocol.hpp"
#include "Server.hpp"

#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>

#include <arpa/inet.h>
#include <chrono>
#include <string>
#include <unistd.h>
#include <pthread.h>
#include <poll.h>


constexpr int MAX_NICK_LENGTH = 20;
constexpr size_t MAX_PACKET_SIZE = 2 * 1024 * 1024;  // 2 MB

namespace Retchat {

    Client::Client(int fd, Server* srv, const std::string& ip) 
        : sockfd(fd), server(srv), ip(ip), sendCounter(0), recvCounter(0), connected(true) 
    {
        name = "usuario" + std::to_string(fd);
        room = "lobby";
        lastRecvTime = std::chrono::steady_clock::now();
    }

    Client::~Client() { close(sockfd); }

    void Client::start() {
        pthread_t tid;
        pthread_create(&tid, nullptr, [](void* arg) -> void* {
            ((Client*) arg)->run();
            return nullptr;
        }, this);
        pthread_detach(tid);
    }

    bool Client::handshake() {
        BIGNUM* server_priv = BN_new();
        BIGNUM* server_pub = BN_new();
        BIGNUM* client_pub = BN_new();
        BIGNUM* shared = BN_new();

        DH::generatePrivateKey(server_priv);
        DH::computePublicKey(server_priv, server_pub);

        size_t pub_len = BN_num_bytes(server_pub);
        uint8_t* pub_buf = new uint8_t[pub_len];
        BN_bn2bin(server_pub, pub_buf);
        uint32_t net_len = htonl(pub_len);

        if (send(sockfd, &net_len, 4, 0) != 4 ||
            send(sockfd, pub_buf, pub_len, 0) != (ssize_t)pub_len) {
            delete[] pub_buf;
            goto error;
        }
        delete[] pub_buf;

        if (recv(sockfd, &net_len, 4, 0) != 4) goto error;
        pub_len = ntohl(net_len);
        if (pub_len > 4096) goto error;
        pub_buf = new uint8_t[pub_len];
        if (recv(sockfd, pub_buf, pub_len, 0) != (ssize_t)pub_len) {
            delete[] pub_buf;
            goto error;
        }
        BN_bin2bn(pub_buf, pub_len, client_pub);
        delete[] pub_buf;

        DH::computeSharedSecret(client_pub, server_priv, shared);
        DH::deriveEncKey(shared, encKey);

        BN_free(server_priv); BN_free(server_pub);
        BN_free(client_pub); BN_free(shared);
        return true;

    error:
        BN_free(server_priv); BN_free(server_pub);
        BN_free(client_pub); BN_free(shared);
        return false;
    }

    bool Client::readFrame(std::vector<uint8_t>& outPlain) {
        uint8_t recvHmac[32];
        size_t hmacRead = 0;
        while (hmacRead < 32) {
            ssize_t r = recv(sockfd, recvHmac + hmacRead, 32 - hmacRead, 0);
            if (r <= 0) return false;
            hmacRead += r;
        }

        uint32_t netLen;   // now 32‑bit
        size_t lenRead = 0;
        while (lenRead < 4) {
            ssize_t r = recv(sockfd, (char*)&netLen + lenRead, 4 - lenRead, 0);
            if (r <= 0) return false;
            lenRead += r;
        }
        uint32_t msgLen = ntohl(netLen);
        if (msgLen == 0 || msgLen > MAX_PACKET_SIZE) return false;

        std::vector<uint8_t> ciphertext(msgLen);
        size_t total = 0;
        while (total < msgLen) {
            ssize_t r = recv(sockfd, ciphertext.data() + total, msgLen - total, 0);
            if (r <= 0) return false;
            total += r;
        }

        // verify HMAC
        unsigned int hmacLen;
        uint8_t expectedHmac[32];
        HMAC(EVP_sha256(), encKey, 32, ciphertext.data(), msgLen, expectedHmac, &hmacLen);
        if (CRYPTO_memcmp(recvHmac, expectedHmac, 32) != 0) return false;  // do not discard, instead kill connection

        // decrypt
        DH::xorCrypt(ciphertext.data(), msgLen, encKey, recvCounter);
        recvCounter++;
        outPlain.swap(ciphertext);
        return true;
    }

void Client::sendPacket(const Packet& pkt) {
    std::vector<uint8_t> payload;
    payload.push_back(pkt.type);
    pkt.serialize(payload);

    // encrypt
    std::vector<uint8_t> ciphertext = payload;
    DH::xorCrypt(ciphertext.data(), ciphertext.size(), encKey, sendCounter);
    sendCounter++;

    // HMAC
    uint8_t hmac[32];
    unsigned int hmacLen;
    HMAC(EVP_sha256(), encKey, 32, ciphertext.data(), ciphertext.size(), hmac, &hmacLen);

    uint32_t netLen = htonl(ciphertext.size());
    std::lock_guard<std::mutex> lock(sendMutex);
    send(sockfd, hmac, 32, 0);
    send(sockfd, &netLen, 4, 0);
    send(sockfd, ciphertext.data(), ciphertext.size(), 0);
}
    void Client::run() {
        if (!handshake()) {
            server->removeClient(this);
            return;
        }

        // welcome message
        SystemPacket welcome;
        welcome.isError = false;
        welcome.code = MSG_WELCOME;
        welcome.params = { name, room };
        sendPacket(welcome);

        server->getRoom(room).addClient(this);

        JoinNotifyPacket joinNotify;
        joinNotify.nick = name;
        server->broadcastToRoom(room, nullptr, joinNotify);

        struct pollfd pfd;
        pfd.fd = sockfd;
        pfd.events = POLLIN;

        constexpr int POLL_TIMEOUT_MS = 1000;  // check every second
        constexpr int KEEPALIVE_INTERVAL_SEC = 30;
        constexpr int KEEPALIVE_WAIT_SEC = 10;

        while (connected) {
            auto now = std::chrono::steady_clock::now();
            double idleSec = std::chrono::duration<double>(now - lastRecvTime).count();

            // if waiting for ack and timeout exceeded, disconnect
            if (waitingForAck) {
                double sinceSent = std::chrono::duration<double>(now - lastKeepAliveSent).count();
                if (sinceSent > KEEPALIVE_WAIT_SEC) {
                    Logger::warn("keepalive timeout, disconnecting " + name);
                    break;
                }
            }

            if (!waitingForAck && idleSec > KEEPALIVE_INTERVAL_SEC) {
                KeepAlivePacket keep;
                sendPacket(keep);
                waitingForAck = true;
                lastKeepAliveSent = now;
            }

            int ret = poll(&pfd, 1, POLL_TIMEOUT_MS);
            if (ret < 0) {
                if (errno == EINTR) continue;
                break;
            }

            if (ret > 0 && (pfd.revents & POLLIN)) {
                std::vector<uint8_t> plain;
                if (readFrame(plain)) {
                    if (!plain.empty()) {
                        size_t off = 0;
                        uint8_t type = plain[off++];
                        Packet* pkt = Packet::create((PacketType)type);
                        if (pkt && pkt->deserialize(plain.data() + off, plain.size() - off)) {
                            processPacket(pkt);
                            delete pkt;
                        }
                    }
                    lastRecvTime = std::chrono::steady_clock::now();
                } else {
                    break;
                }
            }
        }

        connected = false;

        std::string n = name;
        std::string r = room;
        LeaveNotifyPacket leaveNotify;
        leaveNotify.nick = n;
        server->broadcastToRoom(r, nullptr, leaveNotify);

        server->removeClient(this);
    }

    void Client::processPacket(Packet* pkt) {
        switch (pkt->type) {
            case PKT_KEEPALIVE: {
                KeepAliveAckPacket ack;
                sendPacket(ack);
                break;
            }
            case PKT_KEEPALIVE_ACK: {
                waitingForAck = false;
                break;
            }
            case PKT_NICK_REQUEST: {
                auto* req = (NickRequestPacket*) pkt;
                std::string newNick = req->newNick;
                
                bool invalid = false;
                SystemMessageCode code = MSG_NICK_EMPTY;
                std::vector<std::string> params;
                
                size_t start = newNick.find_first_not_of(" \t\n\r");
                size_t end = newNick.find_last_not_of(" \t\n\r");
                
                if (start == std::string::npos) {
                    invalid = true;
                    code = MSG_NICK_EMPTY;
                } else {
                    newNick = newNick.substr(start, end - start + 1);
                    if (newNick.length() < 1) {
                        invalid = true;
                        code = MSG_NICK_EMPTY;
                    } else if (newNick.length() > MAX_NICK_LENGTH) {
                        invalid = true;
                        code = MSG_NICK_TOO_LONG;
                        params.push_back(std::to_string(MAX_NICK_LENGTH));
                    } else if (newNick.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-") != std::string::npos) {
                        invalid = true;
                        code = MSG_NICK_INVALID_CHARS;
                    } else if (newNick == name) {
                        invalid = true;
                        code = MSG_NICK_SAME;
                    } else if (server->isNicknameBanned(newNick)) {
                        invalid = true;
                        code = MSG_NICK_BANNED;
                    }
                }
                
                if (invalid) {
                    SystemPacket err;
                    err.isError = true;
                    err.code = code;
                    err.params = params;
                    sendPacket(err);
                    break;
                }
                
                if (server->isNicknameTaken(newNick, room, this)) {
                    SystemPacket err;
                    err.isError = true;
                    err.code = MSG_NICK_TAKEN;
                    err.params = { newNick };
                    sendPacket(err);
                } else {
                    std::string old = name;
                    name = newNick;
                    NickAckPacket ack;
                    ack.newNick = name;
                    sendPacket(ack);
                    NickNotifyPacket notify;
                    notify.oldNick = old;
                    notify.newNick = name;
                    server->broadcastToRoom(room, this, notify);
                }
                break;
            }
            case PKT_JOIN_REQUEST: {
                auto* req = (JoinRequestPacket*) pkt;
                if (req->roomName == room) {
                    SystemPacket err;
                    err.isError = true;
                    err.code = MSG_JOIN_ALREADY;
                    sendPacket(err);
                } else if (server->isNicknameTaken(name, req->roomName, nullptr)) {
                    SystemPacket err;
                    err.isError = true;
                    err.code = MSG_JOIN_NAME_TAKEN;
                    err.params = { req->roomName };
                    sendPacket(err);
                } else {
                    std::string oldRoom = room;
                    // leave old room
                    LeaveNotifyPacket leaveNotify;
                    leaveNotify.nick = name;
                    server->broadcastToRoom(oldRoom, this, leaveNotify);
                    server->getRoom(oldRoom).removeClient(this);
                    // join new room
                    room = req->roomName;
                    server->getRoom(room).addClient(this);
                    JoinAckPacket ack;
                    ack.roomName = room;
                    sendPacket(ack);
                    JoinNotifyPacket joinNotify;
                    joinNotify.nick = name;
                    server->broadcastToRoom(room, this, joinNotify);
                }
                break;
            }
            case PKT_CHAT_MSG: {
                auto* chat = (ChatPacket*) pkt;
                ChatPacket broadcast;
                broadcast.sender = name;
                broadcast.text = chat->text;
                server->broadcastToRoom(room, this, broadcast);
                break;
            }
            case PKT_DM_REQUEST: {
                auto* dm = (DmRequestPacket*) pkt;
                server->sendDm(this, dm->targetNick, dm->text);
                break;
            }
            case PKT_IMAGE_MSG: {
                auto* img = (ImagePacket*) pkt;

                static const std::unordered_set<std::string> allowed = {
                    "image/png", "image/jpeg", "image/webp", "image/avif"
                };
                if (allowed.find(img->mimeType) == allowed.end()) {
                    SystemPacket err;
                    err.isError = true;
                    err.code = MSG_IMAGE_UNSUPPORTED;
                    err.params = { img->mimeType };
                    sendPacket(err);
                    break;
                }
                img->sender = name;

                if (img->target.empty()) {
                    // doom message
                    server->broadcastToRoom(room, this, *img);
                } else {
                    // direct message
                    server->sendImageDm(this, img->target, *img);
                }
                break;
            }
            default:
                break;
        }
    }

    void Client::disconnect() {
        connected = false;
        close(sockfd);
    }

}