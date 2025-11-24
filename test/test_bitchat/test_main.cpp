#include <Arduino.h>
#include <unity.h>
#include "TestUtil.h"

// Forward declarations for test functions from protocol_handler
void test_parse_announce_message(void);
void test_parse_message_too_short(void);
void test_parse_message_invalid_type(void);
void test_serialize_message(void);
void test_serialize_buffer_too_small(void);
void test_validate_message_valid(void);
void test_validate_message_invalid_type(void);
void test_validate_message_payload_too_large(void);
void test_roundtrip_serialize_parse(void);
void test_parse_all_message_types(void);
void test_parse_empty_payload(void);
void test_parse_max_payload(void);

// Forward declarations for test functions from duplicate_cache
void test_duplicate_cache_first_message(void);
void test_duplicate_cache_add_and_detect(void);
void test_duplicate_cache_different_messages(void);
void test_duplicate_cache_same_sender_different_timestamp(void);
void test_duplicate_cache_same_content_different_sender(void);
void test_duplicate_cache_different_message_types(void);
void test_duplicate_cache_overflow(void);
void test_duplicate_cache_hash_calculation(void);
void test_duplicate_cache_hash_different_payloads(void);
void test_duplicate_cache_empty_payload(void);

// Forward declarations for test functions from fragment_buffer
void test_fragment_buffer_add_single_fragment(void);
void test_fragment_buffer_multiple_fragments(void);
void test_fragment_buffer_get_reassembled(void);
void test_fragment_buffer_out_of_order(void);
void test_fragment_buffer_duplicate_fragment(void);
void test_fragment_buffer_cleanup_expired(void);
void test_fragment_buffer_max_payload(void);
void test_fragment_buffer_invalid_fragment_index(void);
void test_fragment_buffer_multiple_concurrent_assemblies(void);

void setup()
{
    // Wait for hardware (especially important for embedded)
    delay(10);
    delay(2000);

    initializeTestEnvironment();
    
    UNITY_BEGIN();  // Start Unity test framework
    
    // ========================================================================
    // Protocol Handler Tests
    // ========================================================================
    RUN_TEST(test_parse_announce_message);
    RUN_TEST(test_parse_message_too_short);
    RUN_TEST(test_parse_message_invalid_type);
    RUN_TEST(test_serialize_message);
    RUN_TEST(test_serialize_buffer_too_small);
    RUN_TEST(test_validate_message_valid);
    RUN_TEST(test_validate_message_invalid_type);
    RUN_TEST(test_validate_message_payload_too_large);
    RUN_TEST(test_roundtrip_serialize_parse);
    RUN_TEST(test_parse_all_message_types);
    RUN_TEST(test_parse_empty_payload);
    RUN_TEST(test_parse_max_payload);
    
    // ========================================================================
    // Duplicate Cache Tests
    // ========================================================================
    RUN_TEST(test_duplicate_cache_first_message);
    RUN_TEST(test_duplicate_cache_add_and_detect);
    RUN_TEST(test_duplicate_cache_different_messages);
    RUN_TEST(test_duplicate_cache_same_sender_different_timestamp);
    RUN_TEST(test_duplicate_cache_same_content_different_sender);
    RUN_TEST(test_duplicate_cache_different_message_types);
    RUN_TEST(test_duplicate_cache_overflow);
    RUN_TEST(test_duplicate_cache_hash_calculation);
    RUN_TEST(test_duplicate_cache_hash_different_payloads);
    RUN_TEST(test_duplicate_cache_empty_payload);
    
    // ========================================================================
    // Fragment Buffer Tests
    // ========================================================================
    RUN_TEST(test_fragment_buffer_add_single_fragment);
    RUN_TEST(test_fragment_buffer_multiple_fragments);
    RUN_TEST(test_fragment_buffer_get_reassembled);
    RUN_TEST(test_fragment_buffer_out_of_order);
    RUN_TEST(test_fragment_buffer_duplicate_fragment);
    RUN_TEST(test_fragment_buffer_cleanup_expired);
    RUN_TEST(test_fragment_buffer_max_payload);
    RUN_TEST(test_fragment_buffer_invalid_fragment_index);
    RUN_TEST(test_fragment_buffer_multiple_concurrent_assemblies);
    
    exit(UNITY_END());  // Stop unit testing and exit
}

void loop()
{
    // Not used in testing
    delay(1000);
}

