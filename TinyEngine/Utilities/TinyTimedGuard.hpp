#pragma once
#ifndef TINY_ENGINE_TINYTIMEDGUARD
#define TINY_ENGINE_TINYTIMEDGUARD
	#include "./TinyEngine.hpp"

	namespace TINY_ENGINE_NAMESPACE {
        /// @brief Timed-LockGuard (Non-Blocking), check signaled() state after call to constructor.
        template<size_t timeout = 100>
        class _NODISCARD_LOCK TinyTimedGuard {
        public:
            /// @brief Atomic signal for checking the locking state: call .load() or .store() to get/set state.
            std::atomic_bool signal;
            /// @brief Unlderying acquired mutex for this timed guard.
            std::timed_mutex& lock;
            
            /// @brief TinyTimedGuard is non-copyable to avoid multi-lock undefined behavior on a single thread.
            TinyTimedGuard(const TinyTimedGuard&) = delete;
			
            /// @brief TinyTimedGuard is non-copyable to avoid multi-lock undefined behavior on a single thread.
            TinyTimedGuard& operator=(const TinyTimedGuard&) = delete;

            /// @brief Unlocks the attached mutex on deconstruct.
            ~TinyTimedGuard() noexcept { if (signal) lock.unlock(); }
            
            /// @brief Single attempt to signal lock acquired mutex (non-blocking, will fail and return on timeout).
            explicit TinyTimedGuard(std::timed_mutex& lock) : lock(lock) { signal.store(lock.try_lock_for(std::chrono::milliseconds(timeout))); }
            
            /// @brief Returns the signaled locking state for this mutex.
            bool signaled() { return signal.load(); }
        };
    }
#endif