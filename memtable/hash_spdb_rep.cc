// Copyright (C) 2022 Speedb Ltd. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <algorithm>
#include <atomic>
#include <list>
#include <vector>

#include "db/memtable.h"
#include "memory/arena.h"
#include "memtable/spdb_sorted_vector.h"
#include "memtable/stl_wrappers.h"
#include "monitoring/histogram.h"
#include "port/port.h"
#include "rocksdb/memtablerep.h"
#include "rocksdb/slice.h"
#include "rocksdb/slice_transform.h"
#include "rocksdb/utilities/options_type.h"
#include "util/hash.h"
#include "util/heap.h"
#include "util/murmurhash.h"

namespace ROCKSDB_NAMESPACE {
namespace {
struct SpdbKeyHandle {
  SpdbKeyHandle* GetNextBucketItem() {
    return next_.load(std::memory_order_acquire);
  }
  void SetNextBucketItem(SpdbKeyHandle* handle) {
    next_.store(handle, std::memory_order_release);
  }
  std::atomic<SpdbKeyHandle*> next_ = nullptr;
  char key_[1];
};

struct BucketHeader {
  port::RWMutexWr rwlock_;  // this mutex probably wont cause delay
  std::atomic<SpdbKeyHandle*> items_ = nullptr;
  std::atomic<uint32_t> elements_num_ = 0;

  BucketHeader() {}

  bool Contains(const char* check_key,
                const MemTableRep::KeyComparator& comparator, bool needs_lock) {
    bool index_exist = false;
    if (elements_num_.load() == 0) {
      return false;
    }
    if (needs_lock) {
      rwlock_.ReadLock();
    }
    SpdbKeyHandle* anchor = items_.load(std::memory_order_acquire);
    for (auto k = anchor; k != nullptr; k = k->GetNextBucketItem()) {
      const int cmp_res = comparator(k->key_, check_key);
      if (cmp_res == 0) {
        index_exist = true;
        break;
      }
      if (cmp_res > 0) {
        break;
      }
    }
    if (needs_lock) {
      rwlock_.ReadUnlock();
    }
    return index_exist;
  }

  bool Add(SpdbKeyHandle* handle,
           const MemTableRep::KeyComparator& comparator) {
    WriteLock wl(&rwlock_);
    SpdbKeyHandle* iter = items_.load(std::memory_order_acquire);
    SpdbKeyHandle* prev = nullptr;

    for (size_t i = 0; i < elements_num_; i++) {
      const int cmp_res = comparator(iter->key_, handle->key_);
      if (cmp_res == 0) {
        // exist!
        return false;
      }
      if (cmp_res > 0) {
        // need to insert before
        break;
      }
      prev = iter;
      iter = iter->GetNextBucketItem();
    }
    handle->SetNextBucketItem(iter);
    if (prev) {
      prev->SetNextBucketItem(handle);
    } else {
      items_ = handle;
    }
    elements_num_++;
    return true;
  }

  void Get(const LookupKey& k, const MemTableRep::KeyComparator& comparator,
           void* callback_args,
           bool (*callback_func)(void* arg, const char* entry),
           bool needs_lock) {
    if (elements_num_.load() == 0) {
      return;
    }

    if (needs_lock) {
      rwlock_.ReadLock();
    }
    auto iter = items_.load(std::memory_order_acquire);
    for (; iter != nullptr; iter = iter->GetNextBucketItem()) {
      if (comparator(iter->key_, k.internal_key()) >= 0) {
        break;
      }
    }
    for (; iter != nullptr; iter = iter->GetNextBucketItem()) {
      if (!callback_func(callback_args, iter->key_)) {
        break;
      }
    }

    if (needs_lock) {
      rwlock_.ReadUnlock();
    }
  }
};

struct SpdbHashTable {
  std::vector<BucketHeader> buckets_;

  SpdbHashTable(size_t n_buckets) : buckets_(n_buckets) {}

  bool Add(SpdbKeyHandle* handle,
           const MemTableRep::KeyComparator& comparator) {
    BucketHeader* bucket = GetBucket(handle->key_, comparator);
    return bucket->Add(handle, comparator);
  }

  bool Contains(const char* check_key,
                const MemTableRep::KeyComparator& comparator,
                bool needs_lock) const {
    BucketHeader* bucket = GetBucket(check_key, comparator);
    return bucket->Contains(check_key, comparator, needs_lock);
  }

  void Get(const LookupKey& k, const MemTableRep::KeyComparator& comparator,
           void* callback_args,
           bool (*callback_func)(void* arg, const char* entry),
           bool needs_lock) const {
    BucketHeader* bucket = GetBucket(k.internal_key(), comparator);
    bucket->Get(k, comparator, callback_args, callback_func, needs_lock);
  }

 private:
  static size_t GetHash(const Slice& user_key_without_ts) {
    return MurmurHash(user_key_without_ts.data(),
                      static_cast<int>(user_key_without_ts.size()), 0);
  }

  static Slice UserKeyWithoutTimestamp(
      const Slice internal_key, const MemTableRep::KeyComparator& compare) {
    auto key_comparator = static_cast<const MemTable::KeyComparator*>(&compare);
    const Comparator* user_comparator =
        key_comparator->comparator.user_comparator();
    const size_t ts_sz = user_comparator->timestamp_size();
    return ExtractUserKeyAndStripTimestamp(internal_key, ts_sz);
  }

  BucketHeader* GetBucket(const char* key,
                          const MemTableRep::KeyComparator& comparator) const {
    return GetBucket(comparator.decode_key(key), comparator);
  }

  BucketHeader* GetBucket(const Slice& internal_key,
                          const MemTableRep::KeyComparator& comparator) const {
    const size_t hash =
        GetHash(UserKeyWithoutTimestamp(internal_key, comparator));
    BucketHeader* bucket =
        const_cast<BucketHeader*>(&buckets_[hash % buckets_.size()]);
    return bucket;
  }
};

// SpdbVector implemntation

bool SpdbVector::Add(const char* key) {
  ReadLock rl(&add_rwlock_);
  if (sorted_) {
    // it means this  entry arrived after an iterator was created and this
    // vector is immutable return with false
    return false;
  }
  const size_t location = n_elements_.fetch_add(1, std::memory_order_relaxed);
  if (location < items_.size()) {
    items_[location] = key;
    return true;
  }
  return false;
}

bool SpdbVector::Sort(const MemTableRep::KeyComparator& comparator) {
  if (sorted_.load(std::memory_order_acquire)) {
    return true;
  }

  WriteLock wl(&add_rwlock_);
  if (n_elements_ == 0) {
    return false;
  }
  if (sorted_.load(std::memory_order_relaxed)) {
    return true;
  }

  const size_t num_elements = std::min(n_elements_.load(), items_.size());
  n_elements_.store(num_elements);
  if (num_elements < items_.size()) {
    items_.resize(num_elements);
  }
  std::sort(items_.begin(), items_.end(), stl_wrappers::Compare(comparator));
  sorted_.store(true, std::memory_order_release);
  return true;
}

SpdbVector::Iterator SpdbVector::SeekForward(
    const MemTableRep::KeyComparator& comparator, const Slice* seek_key) {
  if (seek_key == nullptr || comparator(items_.front(), *seek_key) >= 0) {
    return items_.begin();
  } else if (comparator(items_.back(), *seek_key) >= 0) {
    return std::lower_bound(items_.begin(), items_.end(), *seek_key,
                            stl_wrappers::Compare(comparator));
  }
  return items_.end();
}

SpdbVector::Iterator SpdbVector::SeekBackword(
    const MemTableRep::KeyComparator& comparator, const Slice* seek_key) {
  if (seek_key == nullptr || comparator(items_.back(), *seek_key) <= 0) {
    return std::prev(items_.end());
  } else if (comparator(items_.front(), *seek_key) <= 0) {
    auto ret = std::lower_bound(items_.begin(), items_.end(), *seek_key,
                                stl_wrappers::Compare(comparator));
    if (comparator(*ret, *seek_key) > 0) {
      --ret;
    }
    return ret;
  }
  return items_.end();
}

SpdbVector::Iterator SpdbVector::Seek(
    const MemTableRep::KeyComparator& comparator, const Slice* seek_key,
    bool up_iter_direction) {
  SpdbVector::Iterator ret = items_.end();
  if (!IsEmpty()) {
    assert(sorted_);
    if (up_iter_direction) {
      ret = SeekForward(comparator, seek_key);
    } else {
      ret = SeekBackword(comparator, seek_key);
    }
  }
  return ret;
}

// SpdbVectorContainer implemanmtation
bool SpdbVectorContainer::InternalInsert(const char* key) {
  return curr_vector_.load()->Add(key);
}

void SpdbVectorContainer::Insert(const char* key) {
  num_elements_.fetch_add(1, std::memory_order_relaxed);
  {
    ReadLock rl(&spdb_vectors_add_rwlock_);

    if (InternalInsert(key)) {
      return;
    }
  }

  // add wasnt completed. need to add new add vector
  bool notify_sort_thread = false;
  {
    WriteLock wl(&spdb_vectors_add_rwlock_);

    if (InternalInsert(key)) {
      return;
    }

    {
      MutexLock l(&spdb_vectors_mutex_);
      SpdbVectorPtr spdb_vector(new SpdbVector(switch_spdb_vector_limit_));
      spdb_vectors_.push_back(spdb_vector);
      spdb_vector->SetVectorListIter(std::prev(spdb_vectors_.end()));
      curr_vector_.store(spdb_vector.get());
    }

    notify_sort_thread = true;

    if (!InternalInsert(key)) {
      assert(false);
      return;
    }
  }
  if (notify_sort_thread) {
    sort_thread_cv_.notify_one();
  }
}

// copy the list of vectors to the iter_anchors
bool SpdbVectorContainer::InitIterator(IterAnchors& iter_anchor,
                                       bool part_of_flush) {
  if (IsEmpty(part_of_flush)) {
    return false;
  }
  bool immutable = immutable_.load();

  auto last_iter = curr_vector_.load()->GetVectorListIter();
  bool notify_sort_thread = false;
  if (!immutable) {
    if (!(*last_iter)->IsEmpty()) {
      {
        MutexLock l(&spdb_vectors_mutex_);
        SpdbVectorPtr spdb_vector(new SpdbVector(switch_spdb_vector_limit_));
        spdb_vectors_.push_back(spdb_vector);
        spdb_vector->SetVectorListIter(std::prev(spdb_vectors_.end()));
        curr_vector_.store(spdb_vector.get());
      }
      notify_sort_thread = true;
    } else {
      --last_iter;
    }
  }
  ++last_iter;
  InitIterator(iter_anchor, spdb_vectors_.begin(), last_iter);
  if (!immutable) {
    if (notify_sort_thread) {
      sort_thread_cv_.notify_one();
    }
  }
  return true;
}

void SpdbVectorContainer::InitIterator(
    IterAnchors& iter_anchor, std::list<SpdbVectorPtr>::iterator start,
    std::list<SpdbVectorPtr>::iterator last) {
  for (auto iter = start; iter != last; ++iter) {
    SortHeapItem* item = new SortHeapItem(*iter, (*iter)->End());
    iter_anchor.push_back(item);
  }
}

void SpdbVectorContainer::SeekIter(const IterAnchors& iter_anchor,
                                   IterHeapInfo* iter_heap_info,
                                   const Slice* seek_key,
                                   bool up_iter_direction) {
  iter_heap_info->Reset(up_iter_direction);
  for (auto const& iter : iter_anchor) {
    if (iter->spdb_vector_->Sort(comparator_)) {
      iter->curr_iter_ =
          iter->spdb_vector_->Seek(comparator_, seek_key, up_iter_direction);
      if (iter->Valid()) {
        iter_heap_info->Insert(iter);
      }
    }
  }
}

void SpdbVectorContainer::SortThread() {
  std::unique_lock<std::mutex> lck(sort_thread_mutex_);
  std::list<SpdbVectorPtr>::iterator sort_iter_anchor = spdb_vectors_.begin();

  for (;;) {
    sort_thread_cv_.wait(lck);

    if (immutable_) {
      break;
    }

    std::list<SpdbVectorPtr>::iterator last;
    last = std::prev(spdb_vectors_.end());

    if (last == sort_iter_anchor) {
      continue;
    }

    for (; sort_iter_anchor != last; ++sort_iter_anchor) {
      (*sort_iter_anchor)->Sort(comparator_);
    }
  }
}

class HashSpdbRep : public MemTableRep {
 public:
  HashSpdbRep(const MemTableRep::KeyComparator& compare, Allocator* allocator,
              size_t bucket_size, bool use_merge);

  HashSpdbRep(Allocator* allocator, size_t bucket_size);

  void PostCreate(const MemTableRep::KeyComparator& compare,
                  Allocator* allocator, bool use_merge);

  KeyHandle Allocate(const size_t len, char** buf) override;

  void Insert(KeyHandle handle) override { InsertKey(handle); }

  bool InsertKey(KeyHandle handle) override;

  bool InsertKeyWithHint(KeyHandle handle, void**) override {
    return InsertKey(handle);
  }

  bool InsertKeyWithHintConcurrently(KeyHandle handle, void**) override {
    return InsertKey(handle);
  }

  bool InsertKeyConcurrently(KeyHandle handle) override {
    return InsertKey(handle);
  }

  void MarkReadOnly() override;

  bool Contains(const char* key) const override;

  size_t ApproximateMemoryUsage() override;

  void Get(const LookupKey& k, void* callback_args,
           bool (*callback_func)(void* arg, const char* entry)) override;

  ~HashSpdbRep() override;

  MemTableRep::Iterator* GetIterator(Arena* arena = nullptr,
                                     bool part_of_flush = false) override;

  const MemTableRep::KeyComparator& GetComparator() const {
    return spdb_vectors_cont_->GetComparator();
  }

 private:
  SpdbHashTable spdb_hash_table_;
  std::shared_ptr<SpdbVectorContainer> spdb_vectors_cont_ = nullptr;
};

HashSpdbRep::HashSpdbRep(const MemTableRep::KeyComparator& compare,
                         Allocator* allocator, size_t bucket_size,
                         bool use_merge)
    : HashSpdbRep(allocator, bucket_size) {
  spdb_vectors_cont_ =
      std::make_shared<SpdbVectorContainer>(compare, use_merge);
}

HashSpdbRep::HashSpdbRep(Allocator* allocator, size_t bucket_size)
    : MemTableRep(allocator), spdb_hash_table_(bucket_size) {}

void HashSpdbRep::PostCreate(const MemTableRep::KeyComparator& compare,
                             Allocator* allocator, bool use_merge) {
  allocator_ = allocator;
  spdb_vectors_cont_ =
      std::make_shared<SpdbVectorContainer>(compare, use_merge);
}

HashSpdbRep::~HashSpdbRep() {
  if (spdb_vectors_cont_) {
    MarkReadOnly();
  }
}

KeyHandle HashSpdbRep::Allocate(const size_t len, char** buf) {
  // constexpr size_t kInlineDataSize =
  //     sizeof(SpdbKeyHandle) - offsetof(SpdbKeyHandle, key_);

  size_t alloc_size = sizeof(SpdbKeyHandle) + len;
  // alloc_size =
  //     std::max(len, kInlineDataSize) - kInlineDataSize +
  //     sizeof(SpdbKeyHandle);
  SpdbKeyHandle* h =
      reinterpret_cast<SpdbKeyHandle*>(allocator_->AllocateAligned(
          alloc_size, ArenaTracker::ArenaStats::HashSpdb));
  *buf = h->key_;
  return h;
}

bool HashSpdbRep::InsertKey(KeyHandle handle) {
  SpdbKeyHandle* spdb_handle = static_cast<SpdbKeyHandle*>(handle);
  if (!spdb_hash_table_.Add(spdb_handle, GetComparator())) {
    return false;
  }
  // insert to later sorter list
  spdb_vectors_cont_->Insert(spdb_handle->key_);
  return true;
}

bool HashSpdbRep::Contains(const char* key) const {
  if (spdb_vectors_cont_->IsEmpty()) {
    return false;
  }
  return spdb_hash_table_.Contains(key, GetComparator(),
                                   !spdb_vectors_cont_->IsReadOnly());
}

void HashSpdbRep::MarkReadOnly() { spdb_vectors_cont_->MarkReadOnly(); }

size_t HashSpdbRep::ApproximateMemoryUsage() {
  // Memory is always allocated from the allocator.
  return 0;
}

void HashSpdbRep::Get(const LookupKey& k, void* callback_args,
                      bool (*callback_func)(void* arg, const char* entry)) {
  if (spdb_vectors_cont_->IsEmpty()) {
    return;
  }
  spdb_hash_table_.Get(k, GetComparator(), callback_args, callback_func,
                       !spdb_vectors_cont_->IsReadOnly());
}

MemTableRep::Iterator* HashSpdbRep::GetIterator(Arena* arena,
                                                bool part_of_flush) {
  if (arena != nullptr) {
    void* mem;
    mem = arena->AllocateAligned(sizeof(SpdbVectorIterator),
                                 ArenaTracker::ArenaStats::HashSpdbIterator);
    return new (mem)
        SpdbVectorIterator(spdb_vectors_cont_, GetComparator(), part_of_flush);
  }
  return new SpdbVectorIterator(spdb_vectors_cont_, GetComparator(),
                                part_of_flush);
}
struct HashSpdbRepOptions {
  static const char* kName() { return "HashSpdbRepOptions"; }
  size_t hash_bucket_count;
  bool use_merge;
};

static std::unordered_map<std::string, OptionTypeInfo> hash_spdb_factory_info =
    {
        {"hash_bucket_count",
         {offsetof(struct HashSpdbRepOptions, hash_bucket_count),
          OptionType::kSizeT, OptionVerificationType::kNormal,
          OptionTypeFlags::kNone}},
        {"use_merge",
         {offsetof(struct HashSpdbRepOptions, use_merge), OptionType::kBoolean,
          OptionVerificationType::kNormal, OptionTypeFlags::kNone}},
};

class HashSpdbRepFactory : public MemTableRepFactory {
 public:
  explicit HashSpdbRepFactory(size_t hash_bucket_count = 1000000,
                              bool use_merge = true) {
    options_.hash_bucket_count = hash_bucket_count;
    options_.use_merge = use_merge;
    RegisterOptions(&options_, &hash_spdb_factory_info);
  }

  using MemTableRepFactory::CreateMemTableRep;
  MemTableRep* CreateMemTableRep(const MemTableRep::KeyComparator& compare,
                                 Allocator* allocator,
                                 const SliceTransform* transform,
                                 Logger* logger) override;
  bool IsInsertConcurrentlySupported() const override { return true; }
  bool CanHandleDuplicatedKey() const override { return true; }
  bool IsRefreshIterSupported() const override { return false; }
  MemTableRep* PreCreateMemTableRep() override;
  void PostCreateMemTableRep(MemTableRep* switch_mem,
                             const MemTableRep::KeyComparator& compare,
                             Allocator* allocator,
                             const SliceTransform* transform,
                             Logger* logger) override;

  static const char* kClassName() { return "HashSpdbRepFactory"; }
  const char* Name() const override { return kClassName(); }

 private:
  HashSpdbRepOptions options_;
};

}  // namespace

// HashSpdbRepFactory

MemTableRep* HashSpdbRepFactory::PreCreateMemTableRep() {
  return new HashSpdbRep(nullptr, options_.hash_bucket_count);
}

void HashSpdbRepFactory::PostCreateMemTableRep(
    MemTableRep* switch_mem, const MemTableRep::KeyComparator& compare,
    Allocator* allocator, const SliceTransform* /*transform*/,
    Logger* /*logger*/) {
  static_cast<HashSpdbRep*>(switch_mem)
      ->PostCreate(compare, allocator, options_.use_merge);
}

MemTableRep* HashSpdbRepFactory::CreateMemTableRep(
    const MemTableRep::KeyComparator& compare, Allocator* allocator,
    const SliceTransform* /*transform*/, Logger* /*logger*/) {
  return new HashSpdbRep(compare, allocator, options_.hash_bucket_count,
                         options_.use_merge);
}

MemTableRepFactory* NewHashSpdbRepFactory(size_t bucket_count, bool use_merge) {
  return new HashSpdbRepFactory(bucket_count, use_merge);
}

}  // namespace ROCKSDB_NAMESPACE
