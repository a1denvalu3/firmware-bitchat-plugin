# Meshtastic BitChat Peer Announcement Feature

## Overview
This feature makes the Meshtastic device act as a **full BitChat peer**, not just a relay bridge. The device now:
- Generates its own unique BitChat peer identity
- Periodically broadcasts ANNOUNCE messages to connected BitChat apps
- Appears in the "people nearby" counter on BitChat iOS/Android apps

## Implementation Details

### 1. Peer Identity Generation
- **Peer ID**: Derived from the Meshtastic node ID (`nodeDB->getNodeNum()`)
- **Device Name**: Uses the owner's long name from Meshtastic settings
- **Announcement Format**: `"Meshtastic: <device_name>"`

### 2. Periodic Announcements
- **Interval**: Every 30 seconds (`ANNOUNCE_INTERVAL_MS = 30000`)
- **Message Type**: `BITCHAT_MSG_ANNOUNCE` (0x01)
- **TTL**: 60 seconds (standard BitChat announcement TTL)
- **Delivery**: Via BLE notifications to connected phones

### 3. Code Changes

#### BitChatBridgeModule.h
- Added `myBitChatPeerId` - stores our BitChat peer ID
- Added `lastAnnounceTime` - tracks when we last announced
- Added `ANNOUNCE_INTERVAL_MS` - 30 second announcement interval
- Added `sendPeerAnnouncement()` - sends announcement via BLE
- Added `createPeerAnnouncement()` - creates BitChat ANNOUNCE message

#### BitChatBridgeModule.cpp
**setup():**
```cpp
myBitChatPeerId = nodeDB->getNodeNum();
LOG_INFO("BitChat Bridge: Acting as peer ID 0x%08x", myBitChatPeerId);
```

**runOnce():**
```cpp
if (bleEnabled && bleServiceSetup) {
    uint32_t currentTime = millis();
    if (currentTime - lastAnnounceTime >= ANNOUNCE_INTERVAL_MS) {
        sendPeerAnnouncement();
        lastAnnounceTime = currentTime;
    }
}
```

**sendPeerAnnouncement():**
- Creates announcement message with `createPeerAnnouncement()`
- Broadcasts via `broadcastToBLE()` to connected phones
- Logs the peer ID being announced

**createPeerAnnouncement():**
- Sets message type to `BITCHAT_MSG_ANNOUNCE`
- Uses Meshtastic node ID as sender ID
- Sets current timestamp
- Creates payload: `"Meshtastic: <device_name>"`
- Sets TTL to 60 seconds

## Expected Behavior

### When a BitChat app connects:
1. Meshtastic immediately sends an announcement via BLE notification
2. The BitChat app receives the announcement
3. The "people nearby" counter increments by 1
4. The Meshtastic device appears as "Meshtastic: <device_name>"

### Every 30 seconds:
1. Meshtastic sends a fresh announcement
2. BitChat apps refresh their peer list
3. The Meshtastic peer stays visible in the "nearby" list

### When acting as dual-role (central + peripheral):
1. As **peripheral**: Sends announcements to the phone's central
2. As **central**: Receives announcements from the phone's peripheral
3. **Full peer behavior**: The device acts exactly like another BitChat app

## Testing

### What to check:
1. Connect BitChat iOS/Android app to Meshtastic via BLE
2. Watch the "people nearby" counter - should increment to 1
3. Look for "Meshtastic: <device_name>" in the peer list
4. Verify announcements every 30 seconds in logs:
   ```
   DEBUG | BitChat Bridge: Sending peer announcement (ID: 0x12345678)
   ```

### Expected logs on startup:
```
INFO | BitChat Bridge: Setting up module
INFO | BitChat Bridge: Acting as peer ID 0x12345678
INFO | BitChat Bridge: Module setup complete - bridge enabled
```

### Expected logs during operation:
```
DEBUG | BitChat Bridge: Sending peer announcement (ID: 0x12345678)
DEBUG | BitChat BLE: Broadcasting message type 0x01 to BLE
```

## Benefits
- **Zero app changes required** - works with existing BitChat apps
- **Full peer compatibility** - Meshtastic is indistinguishable from other BitChat peers
- **Mesh + BLE integration** - device relays messages AND acts as a peer
- **People counter works** - users see the Meshtastic device in their nearby list

## Notes
- Announcements are only sent when BLE is enabled and setup is complete
- The device name comes from Meshtastic's owner settings
- If no device name is set, defaults to "Meshtastic"
- Peer ID is stable (derived from node ID), so the device has a consistent identity


