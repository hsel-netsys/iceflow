/*
 * Copyright 2023 The IceFlow Authors.
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

#ifndef ICEFLOW_CORE_RINGBUFFER_HPP
#define ICEFLOW_CORE_RINGBUFFER_HPP

#include <condition_variable>

#include "boost/circular_buffer.hpp"

namespace iceflow {

/**
 *@brief This class provides a thread safe implementation of a queue. It uses
 *boost::circular_buffer<T> as an internal queue data structure. Along with that
 *it makes use of the C++ standard threading library to protect the queue and
 *provide synchronization among multiple threads.
 */

template <typename T>

class RingBuffer {
public:
  /**
   *@brief Constructor of the RingBuffer class.
   */
  //   TODO: make it configurable - pass this parameter from the Config
  //   file/Constructor
  RingBuffer() : m_queue(10000) {}

  /**
   *@brief Copy assignment
   */
  RingBuffer &operator=(const RingBuffer &) = delete;

  /**
   *@brief Copy constructor of the RingBuffer class. This enables an instance of
   *this class to be passed around as value.
   *
   *@param other The instance to be copied from
   */
  RingBuffer(RingBuffer const &other) {
    std::lock_guard<std::mutex> lock(other->m__mutex);
    m_queue = other.m_queue;
  }

  int size() const { // queue size
    std::lock_guard<std::mutex> lock(m_mutex);
    int x = static_cast<int>(m_queue.size());
    return x;
  }

  /**
   *@brief Pushes a value to the queue
   *
   *@param value The item to be pushed to the queue
   */
  void push(const T &value) {
    {
      std::unique_lock<std::mutex> lock(m_mutex);
      m_queue.push_back(value);
      lock.unlock();
    }
    m_queueCondition.notify_all();
  }
  void pushData(T &value, int threshold) {
    {
      std::unique_lock<std::mutex> lock(m_mutex);
      while (m_queue.size() >= threshold) {
        m_queueCondition.wait(lock);
      }
      m_queue.push_back(value);
      lock.unlock();
    }
    m_queueCondition.notify_all();
  }

  /**
   *@brief Checks if the queue is empty
   *
   *@return true if the queue is empty, else false
   */
  bool empty() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_queue.empty();
  }

  /**
   *@brief Tries to pop a value from the queue and copy it to the value passed
   *in as an argument. This is a non blocking call and returns immediately with
   *a status of retrieval.
   *
   *@param value An instance to which the popped item is to be copied to.
   *
   *@return true if there exists an item to be popped, else false.
   */
  bool tryAndPop(T &value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_queue.empty())
      return false;
    value = m_queue.front();
    m_queue.pop();
    return true;
  }

  /**
   *@brief Tries to pop an item from the queue and returns a std::shared_ptr<T>
   *This is a non blocking call and returns immediately with a status of
   *retrieval.
   *
   *@return nullptr if there is no item to be popped, else returns a shared_ptr
   *pointing to the popped item.
   */
  std::shared_ptr<T> tryAndPop() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_queue.empty())
      return std::shared_ptr<T>();
    std::shared_ptr<T> value = std::make_shared<T>(m_queue.front());
    m_queue.pop();
    return value;
  }

  /**
   *@brief Waits and pops an item from the queue and copies it to value passed
   *in as an argument This is a blocking call and it waits until an item is
   *available to be popped.
   *
   *@param value An instance to which the popped item is to be copied to.
   */
  void waitAndPop(T &value) {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_queueCondition.wait(lock, [this] { return !m_queue.empty(); });
    value = m_queue.front();
    m_queue.pop();
  }

  /**
   *@brief Waits and pops an item from the queue and returns a shared_ptr<T>
   *This is a blocking call and it waits until an item is available to be
   *popped.
   *
   *@return returns a std::shared_ptr<T> pointing to an instance of the popped
   *item
   */
  std::shared_ptr<T> waitAndPop() {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_queueCondition.wait(lock, [this] { return !m_queue.empty(); });
    std::shared_ptr<T> value = std::make_shared<T>(m_queue.front());
    m_queue.pop();
    return value;
  }

  /**
   *@brief Waits and pops an item from the queue and returns a copy of the item
   *This is a blocking call and it waits until an item is available to be
   *popped.
   *
   *@return returns a copy of the instance of the popped item.
   */
  T waitAndPopValue() {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_queueCondition.wait(lock, [this] { return !m_queue.empty(); });
    T value = m_queue.front();
    m_queue.pop_front();
    lock.unlock();
    m_queueCondition.notify_all();
    return value;
  }

  /**
   *@brief Waits and pops an item from the queue and copies it to value passed
   *in as an argument. This is a blocking call with a timeout functionality.
   *
   *@param value An instance to which the popped item is to be copied to.
   *@param waitDuration Wait for at most waitDuration.
   *@return returns false if the wait timedout, returns true if an item was
   *popped and copied.
   */
  bool waitForAndPop(T &value, std::chrono::milliseconds waitDuration) {
    std::unique_lock<std::mutex> lock(m_mutex);
    if (m_queueCondition.wait_for(lock, waitDuration,
                                  [this] { return !m_queue.empty(); })) {
      // Got something
      value = m_queue.front();
      m_queue.pop();
      return true;
    }
    // Timed out
    return false;
  }

  /**
   *@brief Waits and pops an item from the queue and returns a
   *std::shared_ptr<T>. This is a blocking call with a timeout functionality.
   *
   *@param waitDuration Wait for at most waitDuration
   *@return returns nullptr if it timed out, else returns a std::shared_ptr<T>
   *pointing to an instance of item popped.
   */
  std::shared_ptr<T> waitForAndPop(std::chrono::milliseconds waitDuration) {
    std::unique_lock<std::mutex> lock(m_mutex);
    if (m_queueCondition.wait_for(lock, waitDuration,
                                  [this] { return !m_queue.empty(); })) {
      // Got something
      std::shared_ptr<T> value = std::make_shared<T>(m_queue.front());
      m_queue.pop();
      return value;
    }
    // Timed out
    return std::shared_ptr<T>();
  }

private:
  mutable std::mutex m_mutex;
  boost::circular_buffer<T> m_queue;

  std::condition_variable m_queueCondition;
};

} // namespace iceflow

#endif // ICEFLOW_CORE_RINGBUFFER_HPP
