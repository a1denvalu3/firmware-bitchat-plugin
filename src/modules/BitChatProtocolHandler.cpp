#include "BitChatBridgeModule.h"
#include "configuration.h"
#include "mesh/MeshService.h"
#include "mesh/Router.h"
#include "NodeDB.h"
#include "RTC.h"
#include <cstring>
#include <pb_encode.h>
#include <pb_decode.h>

// BitChat Protocol Handler Implementation

bool BitChatProtocolHandler::parseMessage(const uint8_t* data, size_t length, BitChatMessage& msg)
{
    if (!data || length < BITCHAT_HEADER_SIZE) {
        LOG_WARN("BitChat: Invalid message - too short (%d bytes)", length);
        return false;
    }

    // Parse 13-byte BitChat header (iOS-compatible format)
    size_t offset = 0;
    
    // Byte 0: Version
    msg.version = data[offset++];
    if (msg.version != BITCHAT_VERSION) {
        LOG_WARN("BitChat: Unsupported version %d (expected %d)", msg.version, BITCHAT_VERSION);
        return false;
    }
    
    // Byte 1: Message type
    msg.type = data[offset++];
    
    // Byte 2: TTL
    msg.ttl = data[offset++];
    
    // Bytes 3-10: Timestamp (8 bytes, big-endian)
    msg.timestamp = 0;
    for (int i = 0; i < 8; i++) {
        msg.timestamp = (msg.timestamp << 8) | static_cast<uint64_t>(data[offset++]);
    }
    
    // Byte 11: Flags
    msg.flags = data[offset++];
    bool hasRecipient = (msg.flags & BITCHAT_FLAG_HAS_RECIPIENT) != 0;
    bool hasSignature = (msg.flags & BITCHAT_FLAG_HAS_SIGNATURE) != 0;
    bool isCompressed = (msg.flags & BITCHAT_FLAG_IS_COMPRESSED) != 0;
    
    // Bytes 12-13: Payload length (2 bytes, big-endian)
    msg.payloadLength = (static_cast<uint16_t>(data[offset]) << 8) | static_cast<uint16_t>(data[offset + 1]);
    offset += 2;
    
    // Bytes 14-21: Sender ID (8 bytes)
    memcpy(msg.senderId, data + offset, 8);
    offset += 8;
    
    // Bytes 22-29: Recipient ID (8 bytes, optional)
    if (hasRecipient) {
        memcpy(msg.recipientId, data + offset, 8);
        offset += 8;
    } else {
        memset(msg.recipientId, 0, 8);
    }
    
    // Validate payload length
    if (msg.payloadLength > BITCHAT_MAX_PAYLOAD_SIZE) {
        LOG_WARN("BitChat: Payload too large (%d bytes)", msg.payloadLength);
        return false;
    }
    
    // Calculate minimum required length
    size_t minLength = BITCHAT_HEADER_SIZE + 8 + (hasRecipient ? 8 : 0) + msg.payloadLength + (hasSignature ? BITCHAT_SIGNATURE_SIZE : 0);
    if (length < minLength) {
        LOG_WARN("BitChat: Message truncated (expected %d, got %d)", minLength, length);
        return false;
    }
    
    // Payload (with optional compression handling)
    if (isCompressed) {
        // For now, we don't support compression - log warning and fail
        LOG_WARN("BitChat: Compressed payload not supported yet");
        return false;
    }
    
    if (msg.payloadLength > 0) {
        memcpy(msg.payload, data + offset, msg.payloadLength);
        offset += msg.payloadLength;
    }
    
    // Zero out remaining payload buffer
    if (msg.payloadLength < BITCHAT_MAX_PAYLOAD_SIZE) {
        memset(msg.payload + msg.payloadLength, 0, 
               BITCHAT_MAX_PAYLOAD_SIZE - msg.payloadLength);
    }
    
    // Signature (64 bytes, optional)
    if (hasSignature) {
        memcpy(msg.signature, data + offset, BITCHAT_SIGNATURE_SIZE);
        offset += BITCHAT_SIGNATURE_SIZE;
    } else {
        memset(msg.signature, 0, BITCHAT_SIGNATURE_SIZE);
    }
    
    LOG_DEBUG("BitChat: Parsed message type=0x%02x, sender=0x%08x, ttl=%d, payload=%d bytes, flags=0x%02x",
              msg.type, msg.getSenderId32(), msg.ttl, msg.payloadLength, msg.flags);
    
    return true;
}

size_t BitChatProtocolHandler::serializeMessage(const BitChatMessage& msg, uint8_t* buffer, size_t maxLength)
{
    // Calculate required length
    bool hasRecipient = (msg.flags & BITCHAT_FLAG_HAS_RECIPIENT) != 0;
    bool hasSignature = (msg.flags & BITCHAT_FLAG_HAS_SIGNATURE) != 0;
    size_t requiredLength = BITCHAT_HEADER_SIZE + 8 + (hasRecipient ? 8 : 0) + msg.payloadLength + (hasSignature ? BITCHAT_SIGNATURE_SIZE : 0);
    
    if (!buffer || maxLength < requiredLength) {
        return 0;
    }
    
    size_t offset = 0;
    
    // Byte 0: Version
    buffer[offset++] = msg.version;
    
    // Byte 1: Message type
    buffer[offset++] = msg.type;
    
    // Byte 2: TTL
    buffer[offset++] = msg.ttl;
    
    // Bytes 3-10: Timestamp (8 bytes, big-endian)
    for (int i = 7; i >= 0; i--) {
        buffer[offset++] = static_cast<uint8_t>((msg.timestamp >> (i * 8)) & 0xFF);
    }
    
    // Byte 11: Flags
    buffer[offset++] = msg.flags;
    
    // Bytes 12-13: Payload length (2 bytes, big-endian)
    buffer[offset++] = static_cast<uint8_t>((msg.payloadLength >> 8) & 0xFF);
    buffer[offset++] = static_cast<uint8_t>(msg.payloadLength & 0xFF);
    
    // Bytes 14-21: Sender ID (8 bytes)
    memcpy(buffer + offset, msg.senderId, 8);
    offset += 8;
    
    // Bytes 22-29: Recipient ID (8 bytes, optional)
    if (hasRecipient) {
        memcpy(buffer + offset, msg.recipientId, 8);
        offset += 8;
    }
    
    // Payload
    if (msg.payloadLength > 0) {
        memcpy(buffer + offset, msg.payload, msg.payloadLength);
        offset += msg.payloadLength;
    }
    
    // Signature (64 bytes, optional)
    if (hasSignature) {
        memcpy(buffer + offset, msg.signature, BITCHAT_SIGNATURE_SIZE);
        offset += BITCHAT_SIGNATURE_SIZE;
    }
    
    LOG_DEBUG("BitChat: Serialized message %d bytes (flags=0x%02x)", offset, msg.flags);
    return offset;
}

bool BitChatProtocolHandler::validateMessage(const BitChatMessage& msg)
{
    // Check version
    if (msg.version != BITCHAT_VERSION) {
        LOG_WARN("BitChat: Invalid version %d (expected %d)", msg.version, BITCHAT_VERSION);
        return false;
    }
    
    // Check message type - validate known types
    bool validType = false;
    switch (msg.type) {
        case BITCHAT_MSG_ANNOUNCE:
        case BITCHAT_MSG_MESSAGE:
        case BITCHAT_MSG_LEAVE:
        case BITCHAT_MSG_IDENTITY:
        case BITCHAT_MSG_CHANNEL:
        case BITCHAT_MSG_PING:
        case BITCHAT_MSG_PONG:
        case BITCHAT_MSG_NOISE_HANDSHAKE:
        case BITCHAT_MSG_NOISE_ENCRYPTED:
        case BITCHAT_MSG_FRAGMENT_NEW:
        case BITCHAT_MSG_REQUEST_SYNC:
        case BITCHAT_MSG_FILE_TRANSFER:
        case BITCHAT_MSG_FRAGMENT:
            validType = true;
            break;
        default:
            LOG_WARN("BitChat: Invalid message type 0x%02x", msg.type);
            return false;
    }
    if (!validType) {
        LOG_WARN("BitChat: Invalid message type 0x%02x", msg.type);
        return false;
    }
    
    // Check TTL
    if (msg.ttl == 0) {
        LOG_DEBUG("BitChat: Message TTL expired");
        return false;
    }
    
    // Check payload length
    if (msg.payloadLength > BITCHAT_MAX_PAYLOAD_SIZE) {
        LOG_WARN("BitChat: Payload too large (%d bytes)", msg.payloadLength);
        return false;
    }
    
    // Check timestamp (not too far in future, not too old)
    // Convert timestamp from milliseconds to seconds for comparison
    uint64_t currentTimeMs = static_cast<uint64_t>(getTime()) * 1000ULL;
    const uint64_t MAX_FUTURE_SKEW_MS = 86400ULL * 1000ULL; // 24 hours in milliseconds
    const uint64_t MAX_PAST_AGE_MS   = 3600ULL * 1000ULL;   // 1 hour in milliseconds

    if (currentTimeMs > 0) {
        if (msg.timestamp > currentTimeMs + MAX_FUTURE_SKEW_MS) {
            LOG_WARN("BitChat: Message timestamp too far in future (accepting due to clock skew)");
        }
        
        // Allow messages up to 1 hour old
        if (currentTimeMs > msg.timestamp && (currentTimeMs - msg.timestamp) > MAX_PAST_AGE_MS) {
            LOG_DEBUG("BitChat: Message too old, dropping");
            return false;
        }
    }
    
    return true;
}

meshtastic_MeshPacket* BitChatProtocolHandler::createMeshtasticPacket(const BitChatMessage& bitchatMsg)
{
    // Allocate Meshtastic packet using router
    meshtastic_MeshPacket* packet = router->allocForSending();
    if (!packet) {
        LOG_ERROR("BitChat: Failed to allocate Meshtastic packet");
        return nullptr;
    }
    
    // Set port number for BitChat bridge
    packet->decoded.portnum = meshtastic_PortNum_PRIVATE_APP;
    
    // Directly serialize the BitChat message into the payload
    // Format: [4 bytes: magic] [BitChat message data]
    const uint32_t BITCHAT_MAGIC = 0x42434854; // "BCHT"
    
    // Calculate actual message size (same calculation as serializeMessage)
    bool hasRecipient = (bitchatMsg.flags & BITCHAT_FLAG_HAS_RECIPIENT) != 0;
    bool hasSignature = (bitchatMsg.flags & BITCHAT_FLAG_HAS_SIGNATURE) != 0;
    size_t messageSize = BITCHAT_HEADER_SIZE + 8 + (hasRecipient ? 8 : 0) + bitchatMsg.payloadLength + (hasSignature ? BITCHAT_SIGNATURE_SIZE : 0);
    size_t totalSize = sizeof(BITCHAT_MAGIC) + messageSize;
    
    if (totalSize > sizeof(packet->decoded.payload.bytes)) {
        LOG_ERROR("BitChat: Message too large for Meshtastic packet (%d bytes, max %d)", 
                  totalSize, sizeof(packet->decoded.payload.bytes));
        packetPool.release(packet);
        return nullptr;
    }
    
    uint8_t* buffer = packet->decoded.payload.bytes;
    size_t offset = 0;
    
    // Write magic number
    memcpy(buffer + offset, &BITCHAT_MAGIC, sizeof(BITCHAT_MAGIC));
    offset += sizeof(BITCHAT_MAGIC);
    
    // Serialize BitChat message (messageSize was already calculated above)
    size_t serializedSize = serializeMessage(bitchatMsg, buffer + offset, 
                                        sizeof(packet->decoded.payload.bytes) - offset);
    if (serializedSize == 0) {
        LOG_ERROR("BitChat: Failed to serialize message");
        packetPool.release(packet);
        return nullptr;
    }
    
    // Verify serialized size matches expected size
    if (serializedSize != messageSize) {
        LOG_WARN("BitChat: Serialized size (%d) doesn't match expected size (%d)", serializedSize, messageSize);
    }
    
    packet->decoded.payload.size = offset + serializedSize;
    
    // Set hop limit based on TTL
    packet->hop_limit = bitchatMsg.ttl;
    packet->want_ack = false; // BitChat handles its own reliability
    
    LOG_DEBUG("BitChat: Created Meshtastic packet with %d byte payload", packet->decoded.payload.size);
    return packet;
}

bool BitChatProtocolHandler::extractBitChatMessage(const meshtastic_MeshPacket& meshPacket, BitChatMessage& bitchatMsg)
{
    // Check if this is a BitChat packet
    if (meshPacket.decoded.portnum != meshtastic_PortNum_PRIVATE_APP) {
        return false;
    }
    
    // Check for minimum size (magic + header)
    const uint32_t BITCHAT_MAGIC = 0x42434854; // "BCHT"
    if (meshPacket.decoded.payload.size < sizeof(BITCHAT_MAGIC) + BITCHAT_HEADER_SIZE) {
        return false;
    }
    
    const uint8_t* buffer = meshPacket.decoded.payload.bytes;
    size_t offset = 0;
    
    // Check magic number
    uint32_t magic;
    memcpy(&magic, buffer + offset, sizeof(magic));
    offset += sizeof(magic);
    
    if (magic != BITCHAT_MAGIC) {
        // Not a BitChat packet
        return false;
    }
    
    // Parse BitChat message from remaining data
    size_t remainingSize = meshPacket.decoded.payload.size - offset;
    if (!parseMessage(buffer + offset, remainingSize, bitchatMsg)) {
        LOG_WARN("BitChat: Failed to parse BitChat message from Meshtastic packet");
        return false;
    }
    
    LOG_DEBUG("BitChat: Extracted message type=0x%02x, sender=0x%08x, ttl=%d, payload=%d bytes",
              bitchatMsg.type, bitchatMsg.getSenderId32(), bitchatMsg.ttl, bitchatMsg.payloadLength);
    
    return true;
}
