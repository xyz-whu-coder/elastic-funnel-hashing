#include <iostream>
#include <vector>
#include <cmath>
#include <random>
#include <stdexcept>
#include <limits>
#include <optional>
#include <unordered_map>
#include "funnel_hashing.h"

template <typename KT, typename VT>
int FunnelHashTable<KT, VT>::_hash(const KT &key, int salt) const {
    std::hash<KT> hasher;
    return (hasher(key) ^ salt) & 0x7FFFFFFF;
}

template <typename KT, typename VT>
int FunnelHashTable<KT, VT>::_hash_level(const KT &key, int level_index) const {
    return _hash(key, level_salts[level_index]);
}

template <typename KT, typename VT>
int FunnelHashTable<KT, VT>::_hash_special(const KT &key) const {
    return _hash(key, special_salt);
}

template <typename KT, typename VT>
FunnelHashTable<KT, VT>::FunnelHashTable(int capacity, double delta) {
    if (capacity <= 0) throw std::invalid_argument("Capacity must be positive.");
    if (!(0 < delta && delta < 1)) throw std::invalid_argument("Delta must be between 0 and 1.");
    this->capacity = capacity;
    this->delta = delta;
    this->num_inserts = 0;
    this->special_occupancy = 0;

    max_inserts = capacity - static_cast<int>(delta * capacity);
    alpha = static_cast<int>(std::ceil(4 * std::log2(1 / delta) + 10));
    beta = static_cast<int>(std::ceil(2 * std::log2(1 / delta)));
    
    special_size = std::max(1, static_cast<int>(std::floor(3 * delta * capacity / 4)));
    primary_size = capacity - special_size;
    
    int total_buckets = primary_size / beta;
    double a1 = (alpha > 0) ? total_buckets / (4 * (1 - std::pow(0.75, alpha))) : total_buckets;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, std::numeric_limits<int>::max());
    
    int remaining_buckets = total_buckets;
    for (int i = 0; i < alpha; ++i) {
        int a_i = std::max(1, static_cast<int>(std::round(a1 * std::pow(0.75, i))));
        if (remaining_buckets <= 0 || a_i <= 0) break;
        a_i = std::min(a_i, remaining_buckets);
        level_bucket_counts.push_back(a_i);
        levels.emplace_back(a_i * beta);
        level_salts.push_back(dist(gen));
        remaining_buckets -= a_i;
    }
    
    special_array.resize(special_size);
    special_salt = dist(gen);
}

template <typename KT, typename VT>
bool FunnelHashTable<KT, VT>::insert(const KT &key, const VT &value) {
    if (num_inserts >= max_inserts) throw std::runtime_error("Hash table is full.");
    
    for (size_t i = 0; i < levels.size(); ++i) {
        auto &level = levels[i];
        int num_buckets = level_bucket_counts[i];
        int bucket_index = _hash_level(key, i) % num_buckets;
        int start = bucket_index * beta;
        int end = start + beta;
        
        for (int idx = start; idx < end; ++idx) {
            if (!level[idx].occupied || level[idx].key == key) {
                level[idx] = {key, value, true};
                num_inserts++;
                return true;
            }
        }
    }
    
    int size = special_array.size();
    int probe_limit = std::max(1, static_cast<int>(std::ceil(std::log(std::log(capacity + 1) + 1))));
    
    for (int j = 0; j < probe_limit; ++j) {
        int idx = (_hash_special(key) + j) % size;
        if (!special_array[idx].occupied || special_array[idx].key == key) {
            special_array[idx] = {key, value, true};
            special_occupancy++;
            num_inserts++;
            return true;
        }
    }
    
    throw std::runtime_error("Special array insertion failed; table is full.");
}

template <typename KT, typename VT>
std::optional<VT> FunnelHashTable<KT, VT>::search(const KT &key) {
    for (size_t i = 0; i < levels.size(); ++i) {
        auto &level = levels[i];
        int num_buckets = level_bucket_counts[i];
        int bucket_index = _hash_level(key, i) % num_buckets;
        int start = bucket_index * beta;
        int end = start + beta;
        
        for (int idx = start; idx < end; ++idx) {
            if (level[idx].occupied && level[idx].key == key) {
                return level[idx].value;
            }
        }
    }
    
    int size = special_array.size();
    int probe_limit = std::max(1, static_cast<int>(std::ceil(std::log(std::log(capacity + 1) + 1))));
    
    for (int j = 0; j < probe_limit; ++j) {
        int idx = (_hash_special(key) + j) % size;
        if (special_array[idx].occupied && special_array[idx].key == key) {
            return special_array[idx].value;
        }
    }
    
    return std::nullopt;
}

template <typename KT, typename VT>
bool FunnelHashTable<KT, VT>::contains(const KT &key) {
    return search(key).has_value();
}

template <typename KT, typename VT>
int FunnelHashTable<KT, VT>::size() const {
    return this->num_inserts;
}

template <class KT, class VT>
void FunnelHashTable<KT, VT>::print() {
    for (size_t i = 0; i < levels.size(); ++i) {
        std::cout << "Level " << i << ":\n";
        for (size_t j = 0; j < levels[i].size(); ++j) {
            if (levels[i][j].occupied) {
                std::cout << "  " << levels[i][j].key << " -> " << levels[i][j].value << "\n";
            }
        }
    }
}

// Explicit template instantiation
template class FunnelHashTable<std::string, int>;
template class FunnelHashTable<std::string, std::string>;
