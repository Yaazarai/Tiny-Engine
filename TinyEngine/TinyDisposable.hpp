#pragma once
#ifndef TINY_ENGINE_TINYDISPOSABLE
#define TINY_ENGINE_TINYDISPOSABLE
	#include "./TinyEngine.hpp"

	namespace TINY_ENGINE_NAMESPACE {
		#ifndef DISPOSABLE_BOOL_DEFAULT
			#define DISPOSABLE_BOOL_DEFAULT true
		#endif

		class TinyDisposable {
		protected:
			std::atomic_bool disposed = false;

		public:
			TinyInvokable<bool> onDispose;

			void Dispose() {
				if (disposed) return;
				onDispose.invoke(DISPOSABLE_BOOL_DEFAULT);
				disposed = true;
			}

			bool IsDisposed() { return disposed; }
		};
	}

#endif