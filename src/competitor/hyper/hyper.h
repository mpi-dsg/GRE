#pragma once

#include "../indexInterface.h"
#include "./src/include/hyper_index.h"
#include <vector>
#include <cstdio>
#include <optional>

template<class KEY_TYPE, class PAYLOAD_TYPE>
class HyperInterface : public indexInterface<KEY_TYPE, PAYLOAD_TYPE> {
public:
    HyperInterface() : hyper_index_(128.0, 0.1, 16*1024*1024) {
        // Paper §6.1: disable locking in single-thread evaluation.
        Hyper::setLockingEnabled(false);
    }
    
    void init(Param *param = nullptr) {
        Hyper::setLockingEnabled(false);
    }

    void bulk_load(std::pair<KEY_TYPE, PAYLOAD_TYPE> *key_value, size_t num, Param *param = nullptr);

    bool get(KEY_TYPE key, PAYLOAD_TYPE &val, Param *param = nullptr);

    bool put(KEY_TYPE key, PAYLOAD_TYPE value, Param *param = nullptr);

    bool update(KEY_TYPE key, PAYLOAD_TYPE value, Param *param = nullptr);

    bool remove(KEY_TYPE key, Param *param = nullptr);

    size_t scan(KEY_TYPE key_low_bound, size_t key_num, std::pair<KEY_TYPE, PAYLOAD_TYPE> *result,
                Param *param = nullptr);

    long long memory_consumption();

private:
    Hyper hyper_index_;
};

template<class KEY_TYPE, class PAYLOAD_TYPE>
void HyperInterface<KEY_TYPE, PAYLOAD_TYPE>::bulk_load(std::pair<KEY_TYPE, PAYLOAD_TYPE> *key_value, size_t num, Param *param) {
    // Convert the input data to the format expected by Hyper
    std::vector<std::pair<KeyType, ValueType>> data;
    data.reserve(num);
    
    for (size_t i = 0; i < num; ++i) {
        data.emplace_back(static_cast<KeyType>(key_value[i].first), 
                         static_cast<ValueType>(key_value[i].second));
    }
    
    // Bulk load into the Hyper index
    hyper_index_.bulkLoad(data);
}

template<class KEY_TYPE, class PAYLOAD_TYPE>
bool HyperInterface<KEY_TYPE, PAYLOAD_TYPE>::get(KEY_TYPE key, PAYLOAD_TYPE &val, Param *param) {
    KeyType hyper_key = static_cast<KeyType>(key);
    std::optional<ValueType> result = hyper_index_.find(hyper_key);
    
    if (result.has_value()) {
        val = static_cast<PAYLOAD_TYPE>(result.value());
        return true;
    }
    return false;
}

template<class KEY_TYPE, class PAYLOAD_TYPE>
bool HyperInterface<KEY_TYPE, PAYLOAD_TYPE>::put(KEY_TYPE key, PAYLOAD_TYPE value, Param *param) {
    try {
        KeyType hyper_key = static_cast<KeyType>(key);
        ValueType hyper_value = static_cast<ValueType>(value);
        hyper_index_.insert(hyper_key, hyper_value);
        return true;
    } catch (const std::exception& e) {
        // Hyper may throw exceptions on insert failure
        return false;
    }
}

template<class KEY_TYPE, class PAYLOAD_TYPE>
bool HyperInterface<KEY_TYPE, PAYLOAD_TYPE>::update(KEY_TYPE key, PAYLOAD_TYPE value, Param *param) {
    return hyper_index_.update(static_cast<KeyType>(key), static_cast<ValueType>(value));
}

template<class KEY_TYPE, class PAYLOAD_TYPE>
bool HyperInterface<KEY_TYPE, PAYLOAD_TYPE>::remove(KEY_TYPE key, Param *param) {
    return hyper_index_.erase(static_cast<KeyType>(key));
}

template<class KEY_TYPE, class PAYLOAD_TYPE>
size_t HyperInterface<KEY_TYPE, PAYLOAD_TYPE>::scan(KEY_TYPE key_low_bound, size_t key_num,
                                                    std::pair<KEY_TYPE, PAYLOAD_TYPE> *result, Param *param) {
    try {
        auto range_results = hyper_index_.scan(static_cast<KeyType>(key_low_bound), key_num);
        size_t actual_count = 0;
        for (const auto& pair : range_results) {
            if (actual_count >= key_num) break;
            result[actual_count] = std::make_pair(
                static_cast<KEY_TYPE>(pair.first),
                static_cast<PAYLOAD_TYPE>(pair.second)
            );
            actual_count++;
        }
        return actual_count;
    } catch (const std::exception& e) {
        return 0;
    }
}

template<class KEY_TYPE, class PAYLOAD_TYPE>
long long HyperInterface<KEY_TYPE, PAYLOAD_TYPE>::memory_consumption() {
    auto m = hyper_index_.memoryStats();
    // Fig. 11 reports both index (inner) and total; GRE has one field, so return
    // total for fair comparison with ALEX and log index separately.
    std::fprintf(stderr, "[hyper] memory index_bytes=%zu leaf_bytes=%zu total_bytes=%zu\n",
                 m.index_bytes, m.leaf_bytes, m.total_bytes());
    return static_cast<long long>(m.total_bytes());
}
