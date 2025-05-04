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

#include "plugin/speedb/pinning_policy/scoped_pinning_policy.h"

#include <inttypes.h>

#include <cstdio>
#include <unordered_map>

#include "port/port.h"
#include "rocksdb/utilities/options_type.h"

namespace ROCKSDB_NAMESPACE {
static std::unordered_map<std::string, OptionTypeInfo>
    scoped_pinning_type_info = {
        {"capacity",
         {offsetof(struct ScopedPinningOptions, capacity), OptionType::kSizeT,
          OptionVerificationType::kNormal, OptionTypeFlags::kNone}},
        {"last_level_with_data_percent",
         {offsetof(struct ScopedPinningOptions, last_level_with_data_percent),
          OptionType::kUInt32T, OptionVerificationType::kNormal,
          OptionTypeFlags::kNone}},
        {"mid_percent",
         {offsetof(struct ScopedPinningOptions, mid_percent),
          OptionType::kUInt32T, OptionVerificationType::kNormal,
          OptionTypeFlags::kNone}},
};

ScopedPinningPolicy::ScopedPinningPolicy() {
  RegisterOptions(&options_, &scoped_pinning_type_info);
}

ScopedPinningPolicy::ScopedPinningPolicy(const ScopedPinningOptions& options)
    : options_(options) {
  RegisterOptions(&options_, &scoped_pinning_type_info);
}

std::string ScopedPinningPolicy::GetId() const {
  return GenerateIndividualId();
}

bool ScopedPinningPolicy::CheckPin(const TablePinningOptions& tpo,
                                   uint8_t /* type */, size_t size,
                                   size_t usage) const {
  auto proposed = usage + size;
  if (tpo.is_last_level_with_data &&
      options_.last_level_with_data_percent > 0) {
    if (proposed >
        (options_.capacity * options_.last_level_with_data_percent / 100)) {
      return false;
    }
  } else if (tpo.level > 0 && options_.mid_percent > 0) {
    if (proposed > (options_.capacity * options_.mid_percent / 100)) {
      return false;
    }
  } else if (proposed > options_.capacity) {
    return false;
  }

  return true;
}

std::string ScopedPinningPolicy::GetPrintableOptions() const {
  std::string ret;
  const int kBufferSize = 200;
  char buffer[kBufferSize];

  snprintf(buffer, kBufferSize,
           "    pinning_policy.capacity: %" ROCKSDB_PRIszt "\n",
           options_.capacity);
  ret.append(buffer);

  snprintf(buffer, kBufferSize,
           "    pinning_policy.last_level_with_data_percent: %" PRIu32 "\n",
           options_.last_level_with_data_percent);
  ret.append(buffer);

  snprintf(buffer, kBufferSize, "    pinning_policy.mid_percent: %" PRIu32 "\n",
           options_.mid_percent);
  ret.append(buffer);

  return ret;
}

}  // namespace ROCKSDB_NAMESPACE
