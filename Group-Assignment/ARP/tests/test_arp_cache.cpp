#include "arp_cache.hpp"
#include <cassert>
#include <iostream>
#include <thread>

void test_arp_cache_basic() {
    arp::ArpCache cache(std::chrono::seconds(10));
    arp::IpAddress ip("192.168.1.50");
    arp::MacAddress mac("AA:BB:CC:DD:EE:50");

    arp::MacAddress resolved;
    assert(!cache.lookup(ip, resolved));

    cache.update_or_insert(ip, mac);
    assert(cache.lookup(ip, resolved));
    assert(resolved == mac);

    assert(cache.size() == 1);
    std::cout << "[PASS] ARP Cache basic insert & lookup passed.\n";
}

void test_static_protection() {
    arp::ArpCache cache;
    arp::IpAddress ip("192.168.1.1");
    arp::MacAddress trusted_mac("00:AA:BB:CC:DD:01");
    arp::MacAddress rogue_mac("DE:AD:BE:EF:00:99");

    cache.set_static(ip, trusted_mac);

    // Try dynamic poisoning
    cache.update_or_insert(ip, rogue_mac, arp::ArpEntryState::REACHABLE);

    arp::MacAddress resolved;
    assert(cache.lookup(ip, resolved));
    assert(resolved == trusted_mac); // Static entry should NOT be overwritten!

    std::cout << "[PASS] Static ARP entry protection against poisoning passed.\n";
}

void test_cache_ttl_expiry() {
    arp::ArpCache cache(std::chrono::seconds(1));
    arp::IpAddress ip("10.0.0.99");
    arp::MacAddress mac("00:11:22:33:44:99");

    cache.update_or_insert(ip, mac, arp::ArpEntryState::REACHABLE, std::chrono::seconds(1));

    arp::MacAddress resolved;
    assert(cache.lookup(ip, resolved));

    std::cout << "Waiting 1.2s for cache expiry...\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(1200));

    assert(!cache.lookup(ip, resolved)); // Should be expired!
    size_t cleaned = cache.clean_expired();
    assert(cleaned == 1);
    assert(cache.size() == 0);

    std::cout << "[PASS] ARP Cache TTL expiration & cleanup passed.\n";
}

int main() {
    std::cout << "Running ARP Cache & State Machine Unit Tests...\n";
    test_arp_cache_basic();
    test_static_protection();
    test_cache_ttl_expiry();
    std::cout << "All ARP Cache Tests PASSED!\n";
    return 0;
}
