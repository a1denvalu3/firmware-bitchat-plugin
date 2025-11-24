#include <unity.h>
#include "modules/BitChatBridgeModule.h"
#include <cstring>

// ============================================================================
// FragmentReassemblyBuffer Tests
// ============================================================================

void test_fragment_buffer_add_single_fragment(void) {
    FragmentReassemblyBuffer buffer;
    
    // Create a simple fragment (complete message in 1 fragment)
    BitChatMessage fragment;
    fragment.type = BITCHAT_MSG_FRAGMENT;
    fragment.payloadLength = 12;  // 2+1+1+2 header + 6 data
    
    uint16_t fragmentId = 0x1234;
    fragment.payload[0] = (fragmentId >> 8) & 0xFF;
    fragment.payload[1] = fragmentId & 0xFF;
    fragment.payload[2] = 0;  // index 0
    fragment.payload[3] = 1;  // total 1
    fragment.payload[4] = 0;  // original size MSB
    fragment.payload[5] = 6;  // original size LSB
    memcpy(&fragment.payload[6], "data", 4);
    
    bool complete = buffer.addFragment(fragment, fragmentId, 0, 1, 6);
    
    TEST_ASSERT_TRUE(complete);
}

void test_fragment_buffer_multiple_fragments(void) {
    FragmentReassemblyBuffer buffer;
    uint16_t fragmentId = 0x5678;
    uint16_t originalSize = 20;
    uint8_t totalFragments = 3;
    
    // Fragment 0
    BitChatMessage frag0;
    frag0.type = BITCHAT_MSG_FRAGMENT;
    frag0.payloadLength = 12;  // header + 6 bytes data
    frag0.payload[0] = (fragmentId >> 8) & 0xFF;
    frag0.payload[1] = fragmentId & 0xFF;
    frag0.payload[2] = 0;  // index
    frag0.payload[3] = totalFragments;
    frag0.payload[4] = (originalSize >> 8) & 0xFF;
    frag0.payload[5] = originalSize & 0xFF;
    memcpy(&frag0.payload[6], "012345", 6);
    
    bool complete0 = buffer.addFragment(frag0, fragmentId, 0, totalFragments, originalSize);
    TEST_ASSERT_FALSE(complete0);  // Not complete yet
    
    // Fragment 1
    BitChatMessage frag1;
    frag1.type = BITCHAT_MSG_FRAGMENT;
    frag1.payloadLength = 12;
    frag1.payload[0] = (fragmentId >> 8) & 0xFF;
    frag1.payload[1] = fragmentId & 0xFF;
    frag1.payload[2] = 1;  // index
    frag1.payload[3] = totalFragments;
    frag1.payload[4] = (originalSize >> 8) & 0xFF;
    frag1.payload[5] = originalSize & 0xFF;
    memcpy(&frag1.payload[6], "6789AB", 6);
    
    bool complete1 = buffer.addFragment(frag1, fragmentId, 1, totalFragments, originalSize);
    TEST_ASSERT_FALSE(complete1);  // Still not complete
    
    // Fragment 2 (last)
    BitChatMessage frag2;
    frag2.type = BITCHAT_MSG_FRAGMENT;
    frag2.payloadLength = 12;
    frag2.payload[0] = (fragmentId >> 8) & 0xFF;
    frag2.payload[1] = fragmentId & 0xFF;
    frag2.payload[2] = 2;  // index
    frag2.payload[3] = totalFragments;
    frag2.payload[4] = (originalSize >> 8) & 0xFF;
    frag2.payload[5] = originalSize & 0xFF;
    memcpy(&frag2.payload[6], "CDEFGH", 6);
    
    bool complete2 = buffer.addFragment(frag2, fragmentId, 2, totalFragments, originalSize);
    TEST_ASSERT_TRUE(complete2);  // Now complete!
}

void test_fragment_buffer_get_reassembled(void) {
    FragmentReassemblyBuffer buffer;
    uint16_t fragmentId = 0xABCD;
    
    // Add a complete single-fragment message
    BitChatMessage fragment;
    fragment.type = BITCHAT_MSG_FRAGMENT;
    fragment.payloadLength = 16;
    fragment.payload[0] = (fragmentId >> 8) & 0xFF;
    fragment.payload[1] = fragmentId & 0xFF;
    fragment.payload[2] = 0;
    fragment.payload[3] = 1;
    fragment.payload[4] = 0;
    fragment.payload[5] = 10;
    memcpy(&fragment.payload[6], "0123456789", 10);
    
    buffer.addFragment(fragment, fragmentId, 0, 1, 10);
    
    // Get reassembled message
    BitChatMessage reassembled;
    bool success = buffer.getReassembledMessage(fragmentId, reassembled);
    
    TEST_ASSERT_TRUE(success);
    TEST_ASSERT_EQUAL(10, reassembled.payloadLength);
    TEST_ASSERT_EQUAL_MEMORY("0123456789", reassembled.payload, 10);
}

void test_fragment_buffer_out_of_order(void) {
    FragmentReassemblyBuffer buffer;
    uint16_t fragmentId = 0x1111;
    uint8_t totalFragments = 3;
    
    // Add fragment 2 first (out of order)
    BitChatMessage frag2;
    frag2.type = BITCHAT_MSG_FRAGMENT;
    frag2.payloadLength = 12;
    frag2.payload[0] = (fragmentId >> 8) & 0xFF;
    frag2.payload[1] = fragmentId & 0xFF;
    frag2.payload[2] = 2;
    frag2.payload[3] = totalFragments;
    frag2.payload[4] = 0;
    frag2.payload[5] = 18;
    memcpy(&frag2.payload[6], "LAST", 4);
    
    bool complete2 = buffer.addFragment(frag2, fragmentId, 2, totalFragments, 18);
    TEST_ASSERT_FALSE(complete2);
    
    // Add fragment 0
    BitChatMessage frag0;
    frag0.type = BITCHAT_MSG_FRAGMENT;
    frag0.payloadLength = 12;
    frag0.payload[0] = (fragmentId >> 8) & 0xFF;
    frag0.payload[1] = fragmentId & 0xFF;
    frag0.payload[2] = 0;
    frag0.payload[3] = totalFragments;
    frag0.payload[4] = 0;
    frag0.payload[5] = 18;
    memcpy(&frag0.payload[6], "FIRST", 5);
    
    bool complete0 = buffer.addFragment(frag0, fragmentId, 0, totalFragments, 18);
    TEST_ASSERT_FALSE(complete0);
    
    // Add fragment 1 (middle, completes the message)
    BitChatMessage frag1;
    frag1.type = BITCHAT_MSG_FRAGMENT;
    frag1.payloadLength = 12;
    frag1.payload[0] = (fragmentId >> 8) & 0xFF;
    frag1.payload[1] = fragmentId & 0xFF;
    frag1.payload[2] = 1;
    frag1.payload[3] = totalFragments;
    frag1.payload[4] = 0;
    frag1.payload[5] = 18;
    memcpy(&frag1.payload[6], "MIDDLE", 6);
    
    bool complete1 = buffer.addFragment(frag1, fragmentId, 1, totalFragments, 18);
    TEST_ASSERT_TRUE(complete1);  // Should be complete now
}

void test_fragment_buffer_duplicate_fragment(void) {
    FragmentReassemblyBuffer buffer;
    uint16_t fragmentId = 0x2222;
    
    BitChatMessage frag;
    frag.type = BITCHAT_MSG_FRAGMENT;
    frag.payloadLength = 12;
    frag.payload[0] = (fragmentId >> 8) & 0xFF;
    frag.payload[1] = fragmentId & 0xFF;
    frag.payload[2] = 0;
    frag.payload[3] = 2;
    frag.payload[4] = 0;
    frag.payload[5] = 10;
    memcpy(&frag.payload[6], "data", 4);
    
    // Add same fragment twice
    buffer.addFragment(frag, fragmentId, 0, 2, 10);
    bool complete = buffer.addFragment(frag, fragmentId, 0, 2, 10);
    
    // Should not cause issues, still incomplete
    TEST_ASSERT_FALSE(complete);
}

void test_fragment_buffer_cleanup_expired(void) {
    FragmentReassemblyBuffer buffer;
    uint16_t fragmentId = 0x3333;
    
    // Add incomplete fragment
    BitChatMessage frag;
    frag.type = BITCHAT_MSG_FRAGMENT;
    frag.payloadLength = 12;
    frag.payload[0] = (fragmentId >> 8) & 0xFF;
    frag.payload[1] = fragmentId & 0xFF;
    frag.payload[2] = 0;
    frag.payload[3] = 2;
    frag.payload[4] = 0;
    frag.payload[5] = 10;
    
    buffer.addFragment(frag, fragmentId, 0, 2, 10);
    
    // Simulate time passing (beyond timeout)
    uint32_t currentTime = BITCHAT_FRAGMENT_TIMEOUT_MS + 1000;
    buffer.cleanup(currentTime);
    
    // Buffer should have been cleaned up, so we can't retrieve it
    BitChatMessage reassembled;
    bool success = buffer.getReassembledMessage(fragmentId, reassembled);
    TEST_ASSERT_FALSE(success);
}

void test_fragment_buffer_max_payload(void) {
    FragmentReassemblyBuffer buffer;
    uint16_t fragmentId = 0x4444;
    
    // Test with maximum payload size
    BitChatMessage frag;
    frag.type = BITCHAT_MSG_FRAGMENT;
    frag.payloadLength = BITCHAT_FRAGMENT_HEADER_SIZE + BITCHAT_MAX_FRAGMENT_PAYLOAD;
    frag.payload[0] = (fragmentId >> 8) & 0xFF;
    frag.payload[1] = fragmentId & 0xFF;
    frag.payload[2] = 0;
    frag.payload[3] = 1;
    uint16_t size = BITCHAT_MAX_FRAGMENT_PAYLOAD;
    frag.payload[4] = (size >> 8) & 0xFF;
    frag.payload[5] = size & 0xFF;
    
    // Fill with test data
    for (int i = 0; i < BITCHAT_MAX_FRAGMENT_PAYLOAD; i++) {
        frag.payload[6 + i] = (i % 26) + 'A';
    }
    
    bool complete = buffer.addFragment(frag, fragmentId, 0, 1, size);
    TEST_ASSERT_TRUE(complete);
    
    BitChatMessage reassembled;
    bool success = buffer.getReassembledMessage(fragmentId, reassembled);
    TEST_ASSERT_TRUE(success);
    TEST_ASSERT_EQUAL(BITCHAT_MAX_FRAGMENT_PAYLOAD, reassembled.payloadLength);
}

void test_fragment_buffer_invalid_fragment_index(void) {
    FragmentReassemblyBuffer buffer;
    uint16_t fragmentId = 0x5555;
    
    // Try to add fragment with index >= total
    BitChatMessage frag;
    frag.type = BITCHAT_MSG_FRAGMENT;
    frag.payloadLength = 12;
    frag.payload[0] = (fragmentId >> 8) & 0xFF;
    frag.payload[1] = fragmentId & 0xFF;
    frag.payload[2] = 3;  // index 3
    frag.payload[3] = 3;  // total 3 (indices should be 0, 1, 2)
    frag.payload[4] = 0;
    frag.payload[5] = 10;
    
    // This should fail or be rejected
    bool complete = buffer.addFragment(frag, fragmentId, 3, 3, 10);
    TEST_ASSERT_FALSE(complete);
}

void test_fragment_buffer_multiple_concurrent_assemblies(void) {
    FragmentReassemblyBuffer buffer;
    
    // Add fragments for two different messages concurrently
    uint16_t fragmentId1 = 0x1001;
    uint16_t fragmentId2 = 0x2002;
    
    // Message 1, fragment 0
    BitChatMessage frag1_0;
    frag1_0.type = BITCHAT_MSG_FRAGMENT;
    frag1_0.payloadLength = 12;
    frag1_0.payload[0] = (fragmentId1 >> 8) & 0xFF;
    frag1_0.payload[1] = fragmentId1 & 0xFF;
    frag1_0.payload[2] = 0;
    frag1_0.payload[3] = 2;
    frag1_0.payload[4] = 0;
    frag1_0.payload[5] = 10;
    memcpy(&frag1_0.payload[6], "msg1", 4);
    
    buffer.addFragment(frag1_0, fragmentId1, 0, 2, 10);
    
    // Message 2, fragment 0
    BitChatMessage frag2_0;
    frag2_0.type = BITCHAT_MSG_FRAGMENT;
    frag2_0.payloadLength = 12;
    frag2_0.payload[0] = (fragmentId2 >> 8) & 0xFF;
    frag2_0.payload[1] = fragmentId2 & 0xFF;
    frag2_0.payload[2] = 0;
    frag2_0.payload[3] = 2;
    frag2_0.payload[4] = 0;
    frag2_0.payload[5] = 10;
    memcpy(&frag2_0.payload[6], "msg2", 4);
    
    buffer.addFragment(frag2_0, fragmentId2, 0, 2, 10);
    
    // Complete message 1
    BitChatMessage frag1_1;
    frag1_1.type = BITCHAT_MSG_FRAGMENT;
    frag1_1.payloadLength = 12;
    frag1_1.payload[0] = (fragmentId1 >> 8) & 0xFF;
    frag1_1.payload[1] = fragmentId1 & 0xFF;
    frag1_1.payload[2] = 1;
    frag1_1.payload[3] = 2;
    frag1_1.payload[4] = 0;
    frag1_1.payload[5] = 10;
    memcpy(&frag1_1.payload[6], "part2", 5);
    
    bool complete1 = buffer.addFragment(frag1_1, fragmentId1, 1, 2, 10);
    TEST_ASSERT_TRUE(complete1);
    
    // Message 2 should still be incomplete
    BitChatMessage reassembled2;
    bool success2 = buffer.getReassembledMessage(fragmentId2, reassembled2);
    TEST_ASSERT_FALSE(success2);  // Not complete yet
}


