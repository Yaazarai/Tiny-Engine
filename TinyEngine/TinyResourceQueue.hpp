#ifndef TINY_ENGINE_TINYRESOURCEQUEUE
#define TINY_ENGINE_TINYRESOURCEQUEUE
	#include "./TinyEngine.hpp"

	namespace TINY_ENGINE_NAMESPACE {
		/// @brief A ring-queue of resources which accepts an index/destructor function.
		template<class T, size_t S>
		class TinyResourceQueue : public TinyDisposable {
		public:
			std::array<T, S> resourceQueue;
			TinyCallback<size_t&> indexCallback;
			TinyCallback<T&> destructorCallback;
			
			/// @brief Deleted copy constructor (dynamic objects are not copyable).
			TinyResourceQueue operator=(const TinyResourceQueue&) = delete;
			/// @brief Deleted copy constructor (dynamic objects are not copyable).
			TinyResourceQueue(const TinyResourceQueue&) = delete;

			~TinyResourceQueue() { this->Dispose(); }

			/// @brief Creates a resource queue which returns an instance of type T at an frame index I for swapchain rendering.
			TinyResourceQueue(std::array<T, S> resources, TinyCallback<size_t&> indexCallback, TinyCallback<T&> destructor)
			: resourceQueue(resources), indexCallback(indexCallback), destructorCallback(destructor) {}

			/// @brief Get a resource by manual index lookup.
			T& GetResourceByIndex(size_t index) { return resourceQueue[index]; }

			/// @brief Get a resource by frame-index using the indexer callback.
			T& GetFrameResource() {
				size_t index = 0;
				indexCallback.invoke(index);
				return resourceQueue[index];
			}
		};
	}

#endif

///
///	The TinyResourceQueue is for creating a ring-buffer of resources
///	for use with concepts like the Vulkan Swapchain, where you may have
///	per-frame specific resources for rendering synchronization.
///
///	Constructor Parameters:
///		std::array<T, S> : array of resource objects (passed by value).
///		callback<size_t&> : callback with size_t out argument which outs the frame index.
///		callback<T&> : destructor callback for disposing of resource objects.
///