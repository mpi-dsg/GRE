#include "../indexInterface.h"

// DyTIS (Yang et al., EuroSys'23) declares its types (Key, Value, Pair, Key_t,
// Value_t, NONE, ...) at global scope with very generic names. Several other
// competitors in this benchmark (e.g. artsync's own global `class Key`) do the
// same, and since every competitor header lands in one translation unit via
// competitor.h, that collides. Rather than patch the vendored upstream files,
// wrap the whole include chain in a private namespace -- the preprocessor just
// inlines the tokens, so every unqualified name inside these headers resolves
// within dytis_vendor instead of polluting the global namespace.
//
// Upstream's own DyTIS.h include order is also broken in isolation: it pulls
// in util/util.h and src/Directory.h (both reference Key_t/Value_t/Pair)
// before its own "util/pair.h" (which defines them). Upstream benchmarks only
// work because their main.cpp includes util/pair.h before src/DyTIS.h; we
// reproduce that same order here.
//
// Boost itself must be included at true global scope, BEFORE the namespace
// below opens: Boost's headers contain fully-qualified "::boost::..." lookups
// that only resolve against the real global ::boost namespace. If Boost were
// first pulled in from inside dytis_vendor, that would declare a *nested*
// dytis_vendor::boost instead, and Boost's own "::boost::" references would
// fail to find it. Pre-including it here means Directory.h's own include of
// the same header (further down, inside the namespace) is a no-op thanks to
// Boost's include guards, while unqualified `boost::` lookups from within
// dytis_vendor still find the real ::boost via ordinary enclosing-scope
// namespace lookup.
#include <boost/pool/pool_alloc.hpp>

namespace dytis_vendor {
#include "./src/util/pair.h"
#include "./src/src/DyTIS.h"
#include "./src/src/DyTIS_impl.h"
}

// DyTIS is not templated upstream: its Key_t/Value_t are hardcoded to uint64_t
// (util/pair.h). This benchmark always instantiates indexInterface<uint64_t,
// uint64_t> (see benchmark/microbench.cpp), so the static_casts below are
// identity casts in practice, not a real narrowing risk.
template<class KEY_TYPE, class PAYLOAD_TYPE>
class DyTISInterface : public indexInterface<KEY_TYPE, PAYLOAD_TYPE> {
public:
  void init(Param *param = nullptr) {}

  void bulk_load(std::pair<KEY_TYPE, PAYLOAD_TYPE> *key_value, size_t num, Param *param = nullptr);

  bool get(KEY_TYPE key, PAYLOAD_TYPE &val, Param *param = nullptr);

  bool put(KEY_TYPE key, PAYLOAD_TYPE value, Param *param = nullptr);

  bool update(KEY_TYPE key, PAYLOAD_TYPE value, Param *param = nullptr);

  bool remove(KEY_TYPE key, Param *param = nullptr);

  size_t scan(KEY_TYPE key_low_bound, size_t key_num, std::pair<KEY_TYPE, PAYLOAD_TYPE> *result,
              Param *param = nullptr);

  long long memory_consumption() { return 0; } // upstream exposes no live size accounting

private:
  dytis_vendor::DyTIS dytis;
};

template<class KEY_TYPE, class PAYLOAD_TYPE>
void DyTISInterface<KEY_TYPE, PAYLOAD_TYPE>::bulk_load(std::pair<KEY_TYPE, PAYLOAD_TYPE> *key_value, size_t num,
                                                        Param *param) {
  // Upstream has no dedicated bulk-load path; its own benchmarks (main.cpp,
  // ycsb_style_main.cpp) construct the index by calling Insert() in a loop.
  for (size_t i = 0; i < num; i++) {
    dytis_vendor::Key_t k = static_cast<dytis_vendor::Key_t>(key_value[i].first);
    dytis.Insert(k, static_cast<dytis_vendor::Value_t>(key_value[i].second));
  }
}

template<class KEY_TYPE, class PAYLOAD_TYPE>
bool DyTISInterface<KEY_TYPE, PAYLOAD_TYPE>::get(KEY_TYPE key, PAYLOAD_TYPE &val, Param *param) {
  dytis_vendor::Key_t k = static_cast<dytis_vendor::Key_t>(key);
  dytis_vendor::Value_t v = dytis.Get(k);
  // NONE (0x0) is upstream's not-found sentinel; it is indistinguishable from
  // a real stored value of 0. Same ambiguity as several other wrapped indexes.
  if (v == dytis_vendor::NONE) return false;
  val = static_cast<PAYLOAD_TYPE>(v);
  return true;
}

template<class KEY_TYPE, class PAYLOAD_TYPE>
bool DyTISInterface<KEY_TYPE, PAYLOAD_TYPE>::put(KEY_TYPE key, PAYLOAD_TYPE value, Param *param) {
  dytis_vendor::Key_t k = static_cast<dytis_vendor::Key_t>(key);
  dytis.Insert(k, static_cast<dytis_vendor::Value_t>(value));
  return true; // Insert() is void upstream -- no failure signal exists to propagate
}

template<class KEY_TYPE, class PAYLOAD_TYPE>
bool DyTISInterface<KEY_TYPE, PAYLOAD_TYPE>::update(KEY_TYPE key, PAYLOAD_TYPE value, Param *param) {
  dytis_vendor::Key_t k = static_cast<dytis_vendor::Key_t>(key);
  return dytis.Update(k, static_cast<dytis_vendor::Value_t>(value));
}

template<class KEY_TYPE, class PAYLOAD_TYPE>
bool DyTISInterface<KEY_TYPE, PAYLOAD_TYPE>::remove(KEY_TYPE key, Param *param) {
  dytis_vendor::Key_t k = static_cast<dytis_vendor::Key_t>(key);
  // NOTE: upstream DyTIS::Delete() returns true (not false) when the top-level
  // bucket for this key was never initialized, i.e. deleting a key that was
  // never inserted reports success. Documented as-is in CRUD_AUDIT.md.
  return dytis.Delete(k);
}

template<class KEY_TYPE, class PAYLOAD_TYPE>
size_t DyTISInterface<KEY_TYPE, PAYLOAD_TYPE>::scan(KEY_TYPE key_low_bound, size_t key_num,
                                                     std::pair<KEY_TYPE, PAYLOAD_TYPE> *result,
                                                     Param *param) {
  dytis_vendor::Key_t k = static_cast<dytis_vendor::Key_t>(key_low_bound);
  dytis_vendor::Value_t *values = dytis.Scan(k, key_num);
  // Upstream Scan() returns ONLY values in ascending-key order -- it never
  // returns the keys themselves, and it reports no count of how many slots
  // were actually filled (a short scan near the top of the keyspace leaves
  // the tail of the array uninitialized, with no signal to the caller).
  // We cannot reconstruct real (key, value) pairs from this API; this is
  // the same "fabricated keys" limitation CRUD_AUDIT.md already documents
  // for dili/dilax. Treat DyTIS scan-correctness results as unreliable by
  // construction, not just empirically.
  for (size_t i = 0; i < key_num; i++) {
    result[i] = std::make_pair(static_cast<KEY_TYPE>(key_low_bound + i), static_cast<PAYLOAD_TYPE>(values[i]));
  }
  delete[] values;
  return key_num; // upstream provides no real fill count; this always claims a full scan
}
