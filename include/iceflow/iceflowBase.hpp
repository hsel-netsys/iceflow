/*
 * Copyright 2024 The IceFlow Authors.
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

#ifndef ICEFLOWBasePubSub_HPP
#define ICEFLOWBasePubSub_HPP

#include "ndn-svs/core.hpp"
#include "ndn-svs/mapping-provider.hpp"
#include "ndn-svs/security-options.hpp"
#include "ndn-svs/store.hpp"
#include "ndn-svs/svsync.hpp"

#include <ndn-cxx/security/validator-null.hpp>
#include <ndn-cxx/util/segment-fetcher.hpp>

#include <chrono>
#include <iostream>

using namespace std::chrono_literals;
namespace iceflow {

struct SubscriptionData {
  /** @brief Name of the received publication */
  const ndn::Name &name;

  /** @brief Payload of received data */
  const ndn::span<const uint8_t> data;

  /** @brief Producer of the publication */
  const ndn::Name &producerPrefix;

  /** @brief The sequence number of the publication */
  const ndn::svs::SeqNo seqNo;

  /** @brief Received data packet, only for "packet" subscriptions */
  const std::optional<ndn::Data> &packet;
};

/** Callback returning the received data, producer and sequence number */
using SubscriptionCallback = std::function<void(const SubscriptionData &)>;

class Iceflow;

struct IceflowPubSubOptions {
  /// @brief Interface to store data packets
  std::shared_ptr<ndn::svs::DataStore> dataStore =
      ndn::svs::SVSync::DEFAULT_DATASTORE;

  /**
   * @brief Send publication timestamp in mapping blocks.
   *
   * This option should be enabled in all instances for
   * correct usage of the maxPubAge option.
   */
  bool useTimestamp = true;

  /**
   * @brief Maximum age of publications to be fetched.
   *
   * The useTimestamp option should be enabled for this to work.
   */
  std::chrono::milliseconds maxPubAge = std::chrono::milliseconds(0);
};
/**
 * @brief A pub/sub interface for IceFlow.
 *
 * This interface provides a high level API to use IceFlow for pub/sub
 * applications. Every node can produce data under a prefix which is served to
 * subscribers for that stream.
 *
 */

class IceflowPubSub {
public:
  /**
   * @brief Constructor.
   * @param syncPrefix The prefix of the sync group
   * @param nodePrefix Default prefix to publish data under
   * @param face The face used to communication
   * @param updateCallback The callback function to handle state updates
   * @param securityOptions Signing and validation for sync interests and data
   * @param dataStore Interface to store data packets
   */
  IceflowPubSub(const ndn::Name &syncPrefix, const ndn::Name &nodePrefix,
                ndn::Face &face, ndn::svs::UpdateCallback updateCallback,
                const IceflowPubSubOptions &options,
                const ndn::svs::SecurityOptions &securityOptions =
                    ndn::svs::SecurityOptions::DEFAULT)
      : m_face(face), m_syncPrefix(syncPrefix), m_dataPrefix(nodePrefix),
        m_onUpdate(std::move(updateCallback)), m_opts(options),
        m_securityOptions(securityOptions),
        m_svsync(syncPrefix, nodePrefix, face,
                 std::bind(&IceflowPubSub::updateCallbackInternal, this, _1),
                 securityOptions, options.dataStore),
        m_mappingProvider(syncPrefix, nodePrefix, face, securityOptions) {

    m_svsync.getCore().setGetExtraBlockCallback(
        std::bind(&IceflowPubSub::onGetExtraData, this, _1));
    m_svsync.getCore().setRecvExtraBlockCallback(
        std::bind(&IceflowPubSub::onRecvExtraData, this, _1));
  }

public:
  ndn::svs::SeqNo publish(const ndn::Name &name, ndn::span<const uint8_t> value,
                          const ndn::Name &nodePrefix,
                          ndn::time::milliseconds freshnessPeriod,
                          std::vector<ndn::Block> mappingBlocks = {}) {
    // Segment the data if larger than MAX_DATA_SIZE
    if (value.size() > MAX_DATA_SIZE) {
      size_t nSegments = (value.size() / MAX_DATA_SIZE) + 1;
      auto finalBlock = ndn::name::Component::fromSegment(nSegments - 1);

      ndn::svs::NodeID nid =
          nodePrefix == EMPTY_NAME ? m_dataPrefix : nodePrefix;
      ndn::svs::SeqNo seqNo = m_svsync.getCore().getSeqNo(nid) + 1;

      for (size_t i = 0; i < nSegments; i++) {
        // Create encapsulated segment
        auto segmentName = ndn::Name(name).appendVersion(0).appendSegment(i);
        auto segment = ndn::Data(segmentName);
        segment.setFreshnessPeriod(freshnessPeriod);

        const uint8_t *segVal = value.data() + i * MAX_DATA_SIZE;
        const size_t segValSize =
            std::min(value.size() - i * MAX_DATA_SIZE, MAX_DATA_SIZE);
        segment.setContent(ndn::make_span(segVal, segValSize));

        segment.setFinalBlock(finalBlock);
        m_securityOptions.dataSigner->sign(segment);

        // Insert outer segment
        m_svsync.insertDataSegment(segment.wireEncode(), freshnessPeriod, nid,
                                   seqNo, i, finalBlock, ndn::tlv::Data);
      }

      // Insert mapping and manually update the sequence number
      insertMapping(nid, seqNo, name, mappingBlocks);
      m_svsync.getCore().updateSeqNo(seqNo, nid);
      return seqNo;
    } else {
      ndn::Data data(name);
      data.setContent(value);
      data.setFreshnessPeriod(freshnessPeriod);
      m_securityOptions.dataSigner->sign(data);
      return publishPacket(data, nodePrefix);
    }
  }

  ndn::svs::SeqNo publishPacket(const ndn::Data &data,
                                const ndn::Name &nodePrefix,
                                std::vector<ndn::Block> mappingBlocks = {}) {
    ndn::svs::NodeID nid = nodePrefix == EMPTY_NAME ? m_dataPrefix : nodePrefix;
    ndn::svs::SeqNo seqNo = m_svsync.publishData(
        data.wireEncode(), data.getFreshnessPeriod(), nid, ndn::tlv::Data);
    insertMapping(nid, seqNo, data.getName(), mappingBlocks);
    return seqNo;
  }

  void insertMapping(const ndn::svs::NodeID &nid, ndn::svs::SeqNo seqNo,
                     const ndn::Name &name,
                     std::vector<ndn::Block> additional) {
    // additional is a copy deliberately
    // this way we can add well-known mappings to the list

    // add timestamp block
    if (m_opts.useTimestamp) {
      unsigned long now =
          std::chrono::duration_cast<std::chrono::microseconds>(
              std::chrono::system_clock::now().time_since_epoch())
              .count();
      auto timestamp = ndn::Name::Component::fromNumber(
          now, ndn::tlv::TimestampNameComponent);
      additional.push_back(timestamp);
    }

    // create mapping entry
    ndn::svs::MappingEntryPair entry = {name, additional};

    // notify subscribers in next sync interest
    if (m_notificationMappingList.nodeId == EMPTY_NAME ||
        m_notificationMappingList.nodeId == nid) {
      m_notificationMappingList.nodeId = nid;
      m_notificationMappingList.pairs.push_back({seqNo, entry});
    }

    // send mapping to provider
    m_mappingProvider.insertMapping(nid, seqNo, entry);
  }

  void
  updateCallbackInternal(const std::vector<ndn::svs::MissingDataInfo> &info) {
    for (const auto &stream : info) {
      ndn::Name streamName(stream.nodeId);

      // Producer subscriptions
      for (const auto &sub : m_producerSubscriptions) {
        if (sub.prefix.isPrefixOf(streamName)) {
          // Add to fetching queue
          for (ndn::svs::SeqNo i = stream.low; i <= stream.high; i++)
            m_fetchMap[std::pair(stream.nodeId, i)].push_back(sub);

          // Prefetch next available data
          if (sub.prefetch)
            m_svsync.fetchData(stream.nodeId, stream.high + 1,
                               [](auto &&...) {}); // do nothing with prefetch
        }
      }

      // Fetch all mappings if we have prefix subscription(s)
      if (!m_prefixSubscriptions.empty()) {
        ndn::svs::MissingDataInfo remainingInfo = stream;

        // Attemt to find what we already know about mapping
        // This typically refers to the Sync Interest mapping optimization,
        // where the Sync Interest contains the notification mapping list
        for (ndn::svs::SeqNo i = remainingInfo.low; i <= remainingInfo.high;
             i++) {
          try {
            // throws if mapping not found
            this->processMapping(stream.nodeId, i);
            remainingInfo.low++;
          } catch (const std::exception &) {
            break;
          }
        }

        // Find from network what we don't yet know
        while (remainingInfo.high >= remainingInfo.low) {
          // Fetch a max of 10 entries per request
          // This is to ensure the mapping response does not overflow
          // TODO: implement a better solution to this issue
          ndn::svs::MissingDataInfo truncatedRemainingInfo = remainingInfo;
          if (truncatedRemainingInfo.high - truncatedRemainingInfo.low > 10) {
            truncatedRemainingInfo.high = truncatedRemainingInfo.low + 10;
          }

          m_mappingProvider.fetchNameMapping(
              truncatedRemainingInfo,
              [this, remainingInfo,
               streamName](const ndn::svs::MappingList &list) {
                bool queued = false;
                for (const auto &[seq, mapping] : list.pairs)
                  queued |= this->processMapping(streamName, seq);

                if (queued)
                  this->fetchAll();
              },
              -1);

          remainingInfo.low += 11;
        }
      }
    }
    // TODO: modify fetchAll()
    fetchAll();
    m_onUpdate(info);
  }

  void fetchAll() {
    for (const auto &pair : m_fetchMap) {
      // Check if already fetching this publication
      auto key = pair.first;
      if (m_fetchingMap.find(key) != m_fetchingMap.end())
        continue;
      m_fetchingMap[key] = true;

      // Fetch first data packet
      const auto &[nodeId, seqNo] = key;
      m_svsync.fetchData(nodeId, seqNo,
                         std::bind(&IceflowPubSub::onSyncData, this, _1, key),
                         12);
    }
  }

  void onSyncData(const ndn::Data &firstData,
                  const std::pair<ndn::Name, ndn::svs::SeqNo> &publication) {
    // Make sure the data is encapsulated
    if (firstData.getContentType() != ndn::tlv::Data) {
      m_fetchingMap[publication] = false;
      return;
    }

    // Unwrap
    ndn::Data innerData(firstData.getContent().blockFromValue());
    auto innerContent = innerData.getContent();

    // Return data to packet subscriptions
    SubscriptionData subData = {
        innerData.getName(), innerContent.value_bytes(),
        publication.first,   publication.second,
        innerData,
    };

    // Function to return data to subscriptions
    auto returnData = [this, firstData, subData, publication]() {
      bool hasFinalBlock = subData.packet.value().getFinalBlock().has_value();
      bool hasBlobSubcriptions = false;

      for (const auto &sub : this->m_fetchMap[publication]) {
        if (sub.isPacketSubscription || !hasFinalBlock)
          sub.callback(subData);

        hasBlobSubcriptions |= !sub.isPacketSubscription;
      }

      // If there are blob subscriptions and a final block, we need to fetch
      // remaining segments
      if (hasBlobSubcriptions && hasFinalBlock &&
          firstData.getName().size() > 2) {
        // Fetch remaining segments
        auto pubName = firstData.getName().getPrefix(-2);
        ndn::Interest interest(pubName); // strip off version and segment number
        ndn::SegmentFetcher::Options opts;
        auto fetcher =
            ndn::SegmentFetcher::start(m_face, interest, m_nullValidator, opts);

        fetcher->onComplete.connectSingleShot(
            [this, publication](const ndn::ConstBufferPtr &data) {
              try {
                // Binary BLOB to return to app
                auto finalBuffer = std::make_shared<std::vector<uint8_t>>(
                    std::vector<uint8_t>(data->size()));
                auto bufSize = std::make_shared<size_t>(0);
                bool hasValidator =
                    !!m_securityOptions.encapsulatedDataValidator;

                // Read all TLVs as Data packets till the end of data buffer
                ndn::Block block(6, data);
                block.parse();

                // Number of elements validated / failed to validate
                auto numValidated = std::make_shared<size_t>(0);
                auto numFailed = std::make_shared<size_t>(0);
                auto numElem = block.elements_size();

                if (numElem == 0)
                  return this->cleanUpFetch(publication);

                // Get name of inner data
                auto innerName =
                    ndn::Data(block.elements()[0]).getName().getPrefix(-2);

                // Function to send final buffer to subscriptions if possible
                auto sendFinalBuffer = [this, innerName, publication,
                                        finalBuffer, bufSize, numFailed,
                                        numValidated, numElem] {
                  if (*numValidated + *numFailed != numElem)
                    return;

                  if (*numFailed > 0) // abort
                    return this->cleanUpFetch(publication);

                  // Resize buffer to actual size
                  finalBuffer->resize(*bufSize);

                  // Return data to packet subscriptions
                  SubscriptionData subData = {
                      innerName,          *finalBuffer, publication.first,
                      publication.second, std::nullopt,
                  };

                  for (const auto &sub : this->m_fetchMap[publication])
                    if (!sub.isPacketSubscription)
                      sub.callback(subData);

                  this->cleanUpFetch(publication);
                };

                for (size_t i = 0; i < numElem; i++) {
                  ndn::Data innerData(block.elements()[i]);

                  // Copy actual binary data to final buffer
                  auto size = innerData.getContent().value_size();
                  std::memcpy(finalBuffer->data() + *bufSize,
                              innerData.getContent().value(), size);
                  *bufSize += size;

                  // Validate inner data
                  if (hasValidator) {
                    this->m_securityOptions.encapsulatedDataValidator->validate(
                        innerData,
                        [sendFinalBuffer, numValidated](auto &&...) {
                          *numValidated += 1;
                          sendFinalBuffer();
                        },
                        [sendFinalBuffer, numFailed](auto &&...) {
                          *numFailed += 1;
                          sendFinalBuffer();
                        });
                  } else {
                    *numValidated += 1;
                  }
                }

                sendFinalBuffer();
              } catch (const std::exception &) {
                cleanUpFetch(publication);
              }
            });
        fetcher->onError.connectSingleShot(
            std::bind(&IceflowPubSub::cleanUpFetch, this, publication));
      } else {
        cleanUpFetch(publication);
      }
    };

    // Validate encapsulated packet
    if (m_securityOptions.encapsulatedDataValidator) {
      m_securityOptions.encapsulatedDataValidator->validate(
          innerData, [&](auto &&...) { returnData(); }, [](auto &&...) {});
    } else {
      returnData();
    }
  }

  uint32_t subscribe(const ndn::Name &prefix,
                     const SubscriptionCallback &callback,
                     bool packets = false) {
    uint32_t handle = ++m_subscriptionCount;
    Subscription sub = {handle, prefix, callback, packets, false};
    m_prefixSubscriptions.push_back(sub);
    return handle;
  }

  uint32_t subscribeToProducer(const ndn::Name &nodePrefix,
                               const SubscriptionCallback &callback,
                               bool prefetch, bool packets) {
    uint32_t handle = ++m_subscriptionCount;
    Subscription sub = {handle, nodePrefix, callback, packets, prefetch};
    m_producerSubscriptions.push_back(sub);
    return handle;
  }

  bool processMapping(const ndn::svs::NodeID &nodeId, ndn::svs::SeqNo seqNo) {
    // this will throw if mapping not found
    auto mapping = m_mappingProvider.getMapping(nodeId, seqNo);

    // check if timestamp is too old
    if (m_opts.maxPubAge.count() > 0) {
      // look for the additional timestamp block
      // if no timestamp block is present, we just skip this step
      for (const auto &block : mapping.second) {
        if (block.type() != ndn::tlv::TimestampNameComponent)
          continue;

        unsigned long now =
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch())
                .count();

        unsigned long pubTime = ndn::Name::Component(block).toNumber();
        unsigned long maxAge =
            std::chrono::microseconds(m_opts.maxPubAge).count();

        if (now - pubTime > maxAge)
          return false;
      }
    }
  }

  ndn::Block onGetExtraData(const ndn::svs::VersionVector &) {
    ndn::svs::MappingList copy = m_notificationMappingList;
    m_notificationMappingList = ndn::svs::MappingList();
    return copy.encode();
  }

  void onRecvExtraData(const ndn::Block &block) {
    try {
      ndn::svs::MappingList list(block);
      for (const auto &p : list.pairs) {
        m_mappingProvider.insertMapping(list.nodeId, p.first, p.second);
      }
    } catch (const std::exception &) {
    }
  }

  void cleanUpFetch(const std::pair<ndn::Name, ndn::svs::SeqNo> &publication) {
    m_fetchMap.erase(publication);
    m_fetchingMap.erase(publication);
  }

  void unsubscribe(uint32_t handle) {
    auto unsub = [handle](std::vector<Subscription> &subs) {
      for (auto it = subs.begin(); it != subs.end(); ++it) {
        if (it->id == handle) {
          subs.erase(it);
          return;
        }
      }
    };

    unsub(m_producerSubscriptions);
    unsub(m_prefixSubscriptions);
  }

public:
  static inline const ndn::Name EMPTY_NAME;
  static constexpr size_t MAX_DATA_SIZE = 8000;
  static constexpr ndn::time::milliseconds FRESH_FOREVER =
      ndn::time::years(10000); // well ...

private:
  struct Subscription {
    uint32_t id;
    ndn::Name prefix;
    SubscriptionCallback callback;
    bool isPacketSubscription;
    bool prefetch;
  };

private:
  ndn::Face &m_face;
  const ndn::Name m_syncPrefix;
  const ndn::Name m_dataPrefix;
  const ndn::svs::UpdateCallback m_onUpdate;
  const IceflowPubSubOptions m_opts;
  const ndn::svs::SecurityOptions m_securityOptions;
  ndn::svs::SVSync m_svsync;

  // Null validator for segment fetcher
  // TODO: use a real validator here
  ndn::security::ValidatorNull m_nullValidator;

  // Provider for mapping interests
  ndn::svs::MappingProvider m_mappingProvider;

  // MappingList to be sent in the next update with sync interest
  ndn::svs::MappingList m_notificationMappingList;

  uint32_t m_subscriptionCount;
  std::vector<IceflowPubSub::Subscription> m_producerSubscriptions;
  std::vector<IceflowPubSub::Subscription> m_prefixSubscriptions;

  // Queue of publications to fetch
  std::map<std::pair<ndn::Name, ndn::svs::SeqNo>,
           std::vector<IceflowPubSub::Subscription>>
      m_fetchMap;
  std::map<std::pair<ndn::Name, ndn::svs::SeqNo>, bool> m_fetchingMap;
};

} // namespace iceflow

#endif // ICEFLOWBase_HPP
