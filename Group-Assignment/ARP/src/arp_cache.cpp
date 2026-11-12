#include "arp_cache.hpp"
#include <iomanip>
#include <sstream>
#include <algorithm>

namespace arp {

std::string entry_state_to_string(ArpEntryState state) {
    switch (state) {
        case ArpEntryState::INCOMPLETE: return "INCOMPLETE";
        case ArpEntryState::REACHABLE:  return "REACHABLE";
        case ArpEntryState::STATIC:     return "STATIC";
        case ArpEntryState::EXPIRED:    return "EXPIRED";
    }
    return "UNKNOWN";
}

bool ArpCacheEntry::is_expired() const {
    if (state == ArpEntryState::STATIC) {
        return false;
    }
    auto now = std::chrono::steady_clock::now();
    return (now - timestamp) > ttl;
}

int ArpCacheEntry::remaining_ttl_seconds() const {
    if (state == ArpEntryState::STATIC) {
        return -1; // Infinite
    }
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - timestamp);
    if (elapsed >= ttl) {
        return 0;
    }
    return static_cast<int>((ttl - elapsed).count());
}

ArpCache::ArpCache(std::chrono::seconds default_ttl) : default_ttl_(default_ttl) {}

bool ArpCache::lookup(const IpAddress& ip, MacAddress& out_mac) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = table_.find(ip);
    if (it == table_.end()) {
        return false;
    }

    if (it->second.state == ArpEntryState::STATIC) {
        out_mac = it->second.mac;
        return true;
    }

    if (it->second.state == ArpEntryState::REACHABLE) {
        if (it->second.is_expired()) {
            it->second.state = ArpEntryState::EXPIRED;
            return false;
        }
        out_mac = it->second.mac;
        return true;
    }

    return false;
}

bool ArpCache::update_or_insert(const IpAddress& ip,
                                const MacAddress& mac,
                                ArpEntryState state,
                                std::chrono::seconds ttl) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = table_.find(ip);
    auto now = std::chrono::steady_clock::now();
    std::chrono::seconds effective_ttl = (ttl.count() > 0) ? ttl : default_ttl_;

    if (it != table_.end()) {
        // If existing entry is STATIC, do not let dynamic ARP overwrite it (prevents trivial ARP spoofing)
        if (it->second.state == ArpEntryState::STATIC && state != ArpEntryState::STATIC) {
            return false;
        }
        it->second.mac = mac;
        it->second.state = state;
        it->second.timestamp = now;
        it->second.ttl = effective_ttl;
    } else {
        ArpCacheEntry entry;
        entry.ip = ip;
        entry.mac = mac;
        entry.state = state;
        entry.timestamp = now;
        entry.ttl = effective_ttl;
        table_[ip] = entry;
    }
    return true;
}

void ArpCache::mark_incomplete(const IpAddress& ip) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = table_.find(ip);
    auto now = std::chrono::steady_clock::now();
    if (it != table_.end()) {
        if (it->second.state != ArpEntryState::STATIC) {
            it->second.state = ArpEntryState::INCOMPLETE;
            it->second.mac = MacAddress::zero();
            it->second.timestamp = now;
            it->second.ttl = std::chrono::seconds(5); // 5 sec timeout for unresolved
        }
    } else {
        ArpCacheEntry entry;
        entry.ip = ip;
        entry.mac = MacAddress::zero();
        entry.state = ArpEntryState::INCOMPLETE;
        entry.timestamp = now;
        entry.ttl = std::chrono::seconds(5);
        table_[ip] = entry;
    }
}

void ArpCache::set_static(const IpAddress& ip, const MacAddress& mac) {
    update_or_insert(ip, mac, ArpEntryState::STATIC, std::chrono::seconds(0));
}

bool ArpCache::remove(const IpAddress& ip) {
    std::lock_guard<std::mutex> lock(mutex_);
    return table_.erase(ip) > 0;
}

size_t ArpCache::clean_expired() {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t removed = 0;
    for (auto it = table_.begin(); it != table_.end(); ) {
        if (it->second.state != ArpEntryState::STATIC && it->second.is_expired()) {
            it = table_.erase(it);
            ++removed;
        } else {
            ++it;
        }
    }
    return removed;
}

void ArpCache::flush_dynamic() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = table_.begin(); it != table_.end(); ) {
        if (it->second.state != ArpEntryState::STATIC) {
            it = table_.erase(it);
        } else {
            ++it;
        }
    }
}

void ArpCache::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    table_.clear();
}

bool ArpCache::contains(const IpAddress& ip) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return table_.find(ip) != table_.end();
}

std::vector<ArpCacheEntry> ArpCache::get_all_entries() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ArpCacheEntry> list;
    list.reserve(table_.size());
    for (const auto& kv : table_) {
        list.push_back(kv.second);
    }
    std::sort(list.begin(), list.end(), [](const ArpCacheEntry& a, const ArpCacheEntry& b) {
        return a.ip < b.ip;
    });
    return list;
}

std::string ArpCache::format_table(const std::string& host_name,
                                   const MacAddress& host_mac,
                                   const IpAddress& host_ip) const {
    std::ostringstream oss;
    auto entries = get_all_entries();

    oss << "========================================================================\n";
    if (!host_name.empty() || !host_ip.is_zero()) {
        oss << " ARP Cache Table for Host: " << (host_name.empty() ? "LocalHost" : host_name)
            << " [IP: " << host_ip << ", MAC: " << host_mac << "]\n";
        oss << "------------------------------------------------------------------------\n";
    }
    oss << std::left
        << std::setw(18) << "IP Address"
        << std::setw(20) << "MAC Address"
        << std::setw(14) << "State"
        << std::setw(12) << "TTL (sec)"
        << "HW Type\n";
    oss << "------------------------------------------------------------------------\n";

    if (entries.empty()) {
        oss << " (ARP cache is currently empty)\n";
    } else {
        for (const auto& e : entries) {
            std::string ttl_str;
            if (e.state == ArpEntryState::STATIC) {
                ttl_str = "Permanent";
            } else if (e.is_expired()) {
                ttl_str = "Expired";
            } else {
                ttl_str = std::to_string(e.remaining_ttl_seconds()) + "s";
            }

            std::string mac_str = (e.state == ArpEntryState::INCOMPLETE) ? "<incomplete>" : e.mac.to_string();

            oss << std::left
                << std::setw(18) << e.ip.to_string()
                << std::setw(20) << mac_str
                << std::setw(14) << entry_state_to_string(e.state)
                << std::setw(12) << ttl_str
                << "ether (10Mbps/100Mbps/1Gbps)\n";
        }
    }
    oss << "========================================================================\n";
    return oss.str();
}

void ArpCache::print_table(std::ostream& os,
                           const std::string& host_name,
                           const MacAddress& host_mac,
                           const IpAddress& host_ip) const {
    os << format_table(host_name, host_mac, host_ip);
}

size_t ArpCache::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return table_.size();
}

} // namespace arp
