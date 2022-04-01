#pragma once

#include <atomic>

#include "internal.h"

namespace rco {

	class Spin_lock {
		public:
			Spin_lock()
				: flag{false} {

				}

			RCO_INLINE void lock() {
				while(flag.test_and_set(std::memory_order_acquire));
			}

			RCO_INLINE void unlock() {
				flag.clear(std::memory_order_release);
			}

			RCO_INLINE bool try_lock() {
				return !flag.test_and_set(std::memory_order_acquire);
			}

		private:
			std::atomic_flag flag;
	};

    class Spin_lock_2 {
    public:
        Spin_lock_2() {
            pthread_spin_init(&spin_lock, 0);
        }

        ~Spin_lock_2() {
            pthread_spin_destroy(&spin_lock);
        }

        RCO_INLINE void lock() {
            pthread_spin_lock(&spin_lock);
        }

        RCO_INLINE void unlock() {
            pthread_spin_unlock(&spin_lock);
        }

        RCO_INLINE bool try_lock() {
            return (pthread_spin_trylock(&spin_lock) == 0);
        }

    private:
        pthread_spinlock_t spin_lock;
    };

	class Virtual_lock {
		public:
			Virtual_lock() {

			}

			RCO_INLINE void lock() {

			}

			RCO_INLINE void unlock() {

			}

			RCO_INLINE bool try_lock() {
				return false;
			}
	};

	class Virtual_lock_guard {
		public:
		template<typename Mutex_t>
			explicit Virtual_lock_guard(Mutex_t&) {}
	};

}
