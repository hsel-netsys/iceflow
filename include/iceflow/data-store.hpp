// TODO: Move into fork due to licensing issues...

/*
 * Copyright 2025 The IceFlow Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ICEFLOW_STORE_MEMORY_HPP
#define ICEFLOW_STORE_MEMORY_HPP

#include "ndn-svs/store.hpp"

#include <ndn-cxx/ims/in-memory-storage-persistent.hpp>

namespace iceflow {

class IceflowMemoryDataStore : public ndn::svs::DataStore {
public:
  std::shared_ptr<const ndn::Data>
  find(const ndn::Interest &interest) override {
    return m_ims.find(interest);
  }

  void insert(const ndn::Data &data) override {
    // TODO: Ignore backchannel deletions

    return m_ims.insert(data);
  }

  void erase(const ndn::Name &prefix, const bool isPrefix = true) {
    m_ims.erase(prefix, isPrefix);
  }

private:
  ndn::InMemoryStoragePersistent m_ims;
};

} // namespace iceflow

#endif // ICEFLOW_STORE_MEMORY_HPP
