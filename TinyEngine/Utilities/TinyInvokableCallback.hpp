/// @brief Source: https://stackoverflow.com/questions/9568150/what-is-a-c-delegate/9568485#9568485
/// @brief Source: https://en.cppreference.com/w/cpp/utility/functional/invoke
#pragma once
#ifndef TINY_ENGINE_TINYINVOKABLECALLBACK
#define TINY_ENGINE_TINYINVOKABLECALLBACK
    #include "./TinyEngine.hpp"
    #include <functional>
    #include <vector>
    #include <mutex>
    #include <utility>

    namespace TINY_ENGINE_NAMESPACE {
        /// @brief Function callback-type for event execution.
        template<typename... A>
        class TinyCallback {
        public:
            /// @brief Unique identifying hash code.
            size_t hash;
            /// @brief The function bound to this TinyCallback.
            std::function<void(A...)> bound;
            
            /// @brief Create a new TinyCallback with the specified arguments.
            TinyCallback(std::function<void(A...)> func) : hash(func.target_type().hash_code()), bound(std::move(func)) {}

            /// @brief @brief Compares the underlying hash_code of the TinyCallback function(s) for uniqueness.
            bool compare(const TinyCallback<A...>& cb) { return hash == cb.hash; }

            /// @brief Returns the unique hash code for this TinyCallback function.
            constexpr size_t hash_code() const throw() { return hash; }

            /// @brief Invoke this TinyCallback with required arguments.
            TinyCallback<A...>& invoke(A... args) { bound(static_cast<A&&>(args)...); return (*this); }
        };

        /// @brief Function-List event invoking/executing (takes TinyCallback).
        template<typename... A>
        class TinyInvokable {
        public:
            /// @brief Resource lock for thread-safe accessibility.
            std::timed_mutex safety_lock;
            /// @brief Record of stored TinyCallback(s) to invoke.
            std::vector<TinyCallback<A...>> callbacks;

            /// brief Clones this event's callbacks into another event with the same parameters.
            void clone(TinyInvokable<A...> invokable) {
                invokable.callbackk = this->callbacks;
            }
            
            /// @brief Adds a TinyCallback to this event, operator +=
            bool hook(const TinyCallback<A...> cb) {
                TinyTimedGuard<> g(safety_lock);
                bool signaled = g.signaled();
                if (signaled) callbacks.push_back(cb);
                return signaled;
            }

            /// @brief Removes a TinyCallback from this event.
            bool unhook(const TinyCallback<A...> cb) {
                TinyTimedGuard<> g(safety_lock);
                bool signaled = g.signaled();
                if (signaled) std::erase_if(callbacks, [cb](TinyCallback<A...> c){ return cb.hash_code() == c.hash_code(); });
                return signaled;
            }
            
            /// @brief Removes all registered TinyCallback from this event.
            bool empty(const TinyCallback<A...> cb) {
                TinyTimedGuard<> g(safety_lock);
                bool signaled = g.signaled();
                if (signaled) callbacks.clear();
                return signaled;
            }

            /// @brief Execute all registered TinyCallbacks (signals locking until all callbacks return).
            bool invoke(A... args) {
                TinyTimedGuard<> g(safety_lock);
                bool signaled = g.signaled();
                if (signaled) for (TinyCallback<A...> cb : callbacks) cb.invoke(static_cast<A&&>(args)...);
                return signaled;
            }
        };
    }
#endif