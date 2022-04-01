#pragma once

#include <cstdint>

#include "../common/internal.h"
#include "../common/noncopyable.h"
#include "../common/flags.h"

#include "../rcds/cas_rbuf.h"
#include "../rcds/tsqueue.h"

namespace rco {

	template <typename T>
		class Channel : public Noncopyable {

//			using CoQueue = TSQueue<T, true>;

			public:
			enum class State {
				eEmpty,
				eFull,
				eClosed
			};
			using FlagBits = Flags<State>;

			explicit Channel(size_t size)
				: buf(size){

				};

			RCO_INLINE void operator << (T& param) {
				buf.push(std::move(param));
			}

			RCO_INLINE void operator >> (T& param) {
				buf.pop(param);
			}
			private:

			bool has_buf() {
				return (length != 0);
			}

			uint64_t count;
			uint64_t length;
			uint16_t element_size;
			FlagBits state;
	//		CoQueue  recvq;
	//		CoQueue	 sendq;


			cas::LockFreeRingBuf<T> buf;
		};

}
