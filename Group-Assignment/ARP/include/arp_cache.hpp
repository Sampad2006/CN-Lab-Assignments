#ifndef ARP_CACHE_HPP
#define ARP_CACHE_HPP

#include "types.hpp"
#include <chrono>
#include <unordered_map>
#include <mutex>
#include <string>
#include <vector>

namespace arp {

enum class ArpEntryState {
    INCOMPLETE,  // ARP request sent, awaiting reply
    REACHABLE,   // Valid dynamically learned mapping
    STATIC,      // Permanently configured mapping
    EXPIRED      // TTL elapsed, needs re-resolution
};

std::string entry_state_to_string(ArpEntryState state);

struct ArpCacheEntry {
    IpAddress ip;
    MacAddress mac;
    ArpEntryState state{ArpEntryState::INCOMPLETE};
    std::chrono::steady_clock::time_point timestamp;
    std::chrono::seconds ttl{60}; // Default 60s TTL

    bool is_expired() const;
    int remaining_ttl_seconds() const;
};

class ArpCache {
private:
    mutable std::mutex mutex_;
    std::unordered_map<IpAddress, ArpCacheEntry> table_;
    std::chrono::seconds default_ttl_{60};

public:
    explicit ArpCache(std::chrono::seconds default_ttl = std::chrono::seconds(60));

    // Lookup: returns true if entry exists and is REACHABLE or STATIC
    bool lookup(const IpAddress& ip, MacAddress& out_mac);

    // Update or insert dynamic mapping. Returns false if rejected (e.g. entry is STATIC)
    bool update_or_insert(const IpAddress& ip,
                          const MacAddress& mac,
                          ArpEntryState state = ArpEntryState::REACHABLE,
                          std::chrono::seconds ttl = std::chrono::seconds(0));

    // Mark as incomplete (ARP Request in flight)
    void mark_incomplete(const IpAddress& ip);

    // Set static mapping (will not expire)
    void set_static(const IpAddress& ip, const MacAddress& mac);

    // Remove entry
    bool remove(const IpAddress& ip);

    // Periodic cleanup of expired dynamic entries
    size_t clean_expired();

    // Flush all dynamic entries
    void flush_dynamic();

    // Clear everything
    void clear();

    // Check if entry exists in any state
    bool contains(const IpAddress& ip) const;

    // Retrieve snapshot of all entries
    std::vector<ArpCacheEntry> get_all_entries() const;

    // Formats ARP table identical to standard 'arp -a' / 'ip neigh'
    std::string format_table(const std::string& host_name = "",
                             const MacAddress& host_mac = MacAddress::zero(),
                             const IpAddress& host_ip = IpAddress::any()) const;

    // Print to stream
    void print_table(std::ostream& os,
                     const std::string& host_name = "",
                     const MacAddress& host_mac = MacAddress::zero(),
                     const IpAddress& host_ip = IpAddress::any()) const;

    size_t size() const;
};

} // namespace arp

#endif // ARP_CACHE_HPP
