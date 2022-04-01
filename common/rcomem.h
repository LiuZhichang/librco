#pragma once

#include <atomic>
#include <type_traits>

#include "internal.h"

#include "noncopyable.h"

namespace rco {

	class Ref_obj;
	class Ref_obj_impl;
	class Shared_ref;

	class Destructor {
		public:
			typedef void(*cntl_func) (Ref_obj* ptr, void* arg);

			Destructor()
				:Destructor(nullptr, nullptr) {

				}
			Destructor(cntl_func func, void* arg)
				: ctl_fun(func)
				  , fun_arg(arg) {}

			RCO_INLINE void operator() (Ref_obj* ptr);

		private:
			cntl_func ctl_fun;
			void* fun_arg;
	};

	class Ref_obj {
		public:
			Ref_obj()
				:ref_impl{1} {
					ref = &ref_impl;
				}

			virtual ~Ref_obj() {}

			bool is_shared() const {
				return ref != &ref_impl;
			}

			void increment_ref() {
				++(*ref);
			}

			virtual bool decrement_ref() {
				if( --(*ref) == 0) {
					destructor(this);
					return true;
				}
				return false;
			}

			long use_count() {
				return *ref;
			}

			void set_destructor(Destructor d) {
				destructor = d;
			}

			Ref_obj(const Ref_obj&) = delete;
			Ref_obj& operator = (const Ref_obj &) = delete;

		protected:
			std::atomic_long* ref;
			std::atomic_long  ref_impl;
			Destructor destructor;
	};

	class Ref_obj_impl : public Noncopyable {
		friend Shared_ref;
		public:
		Ref_obj_impl() : ref{1}, weak{1} {}

		void increment_weak()
		{
			++weak;
		}

		void decrement_weak()
		{
			if (--weak == 0) {
				delete this;
			}
		}

		bool lock()
		{
			long count = ref.load(std::memory_order_relaxed);
			do {
				if (count == 0)
					return false;
			} while (!ref.compare_exchange_weak(count, count + 1,
						std::memory_order_acq_rel, std::memory_order_relaxed));

			return true;
		}

		std::atomic_long ref;
		std::atomic_long weak;
	};


	template<class T>
		class Intrusive_ptr_base {

			public:
				Intrusive_ptr_base()
					: ptr(nullptr) {}

				Intrusive_ptr_base(T* p)
					: ptr(p) {
						if(ptr) {
							ptr->increment_ref();
						}
					}

				Intrusive_ptr_base(const T& oth)
					: ptr(oth.ptr) {
						if(ptr) {
							ptr->increment_ref();
						}
					}

				Intrusive_ptr_base(T&& oth)
					: ptr(oth.ptr) {
						oth.ptr = nullptr;
					}

				Intrusive_ptr_base& operator = (const Intrusive_ptr_base& oth) {

					if(this == &oth) return *this;

					reset();

					ptr = oth.ptr;
					if(ptr) {
						ptr.increment_ref();
					}

					return *this;
				}

				Intrusive_ptr_base& operator = (Intrusive_ptr_base&& oth) {

					if(this == &oth) return *this;

					reset();

					std::swap(ptr, oth.ptr);

					return *this;
				}

				T& operator * () const {
					return *ptr;
				}

				T* operator -> () const {
					return ptr;
				}

				explicit operator bool () const {
					return !!ptr;
				}

				void reset() {
					if(ptr) {
						ptr->decrement_ref();
						ptr = nullptr;
					}
				}

				long use_count() {
					return ptr ? ptr->use_count() : 0;
				}

				bool unique() {
					return (use_count() == 1);
				}

				void swap(Intrusive_ptr_base& oth) {
					std::swap(ptr, oth.ptr);
				}

				friend RCO_INLINE bool operator==(Intrusive_ptr_base const& lhs, Intrusive_ptr_base const& rhs) {
					return lhs.ptr_ == rhs.ptr_;
				}
				friend RCO_INLINE bool operator!=(Intrusive_ptr_base const& lhs, Intrusive_ptr_base const& rhs) {
					return lhs.ptr_ != rhs.ptr_;
				}
				friend RCO_INLINE bool operator<(Intrusive_ptr_base const& lhs, Intrusive_ptr_base const& rhs) {
					return lhs.ptr_ < rhs.ptr_;
				}
			private:

				T* ptr;

		};

	class Shared_ref : public Ref_obj {
		public:
			Shared_ref() : impl(new Ref_obj_impl) {
				this->ref = &impl->ref;
			}

			virtual bool decrement_ref()
			{
				Ref_obj_impl * _impl = impl;
				if (Ref_obj::decrement_ref()) {
					std::atomic_thread_fence(std::memory_order_acq_rel);
					_impl->decrement_weak();
					return true;
				}
				return false;
			}

			Ref_obj_impl* impl;
	};

	template <typename T>
		class Weak_ptr {
			public:
				Weak_ptr()
					: impl(nullptr)
					  , ptr(nullptr) {

					  }

				explicit Weak_ptr(T* ptr)
					: impl(nullptr)
					  , ptr(nullptr) {
						  reset(ptr);
					  }

				explicit Weak_ptr(const Intrusive_ptr_base<T>& intrusive_ptr)
					: Weak_ptr() {

					}

				void reset() {
					if(impl) {
						impl->decrement_weak();
						impl = nullptr;
						ptr = nullptr;
					}
				}

				void reset(T* p) {
					if(impl) {
						impl->decrement_weak();
						impl = nullptr;
						ptr = nullptr;
					}

					if(!p) return;
					if(!p->is_shared()) return;

					impl = static_cast<Shared_ref*>(p)->impl;
					ptr = p;
					impl->increment_weak();
				}

				Intrusive_ptr_base<T> lock() const {
					if(!impl) return Intrusive_ptr_base<T>();
					if(!impl->lock()) return Intrusive_ptr_base<T>();

					Intrusive_ptr_base<T> intrusive_ptr(ptr);
					ptr->decrement_ref();

					return intrusive_ptr;
				}

				explicit operator bool() const {
					return !!impl;
				}

				long use_count() {
					return impl ? impl->ref.load() : 0;
				}

				friend bool operator==(Weak_ptr<T> const& lhs, Weak_ptr<T> const& rhs) {
					return lhs.impl_ == rhs.impl_ && lhs.ptr_ == rhs.ptr_;
				}

				friend bool operator<(Weak_ptr<T> const& lhs, Weak_ptr<T> const& rhs) {
					return lhs.impl_ < rhs.impl_;
				}

			private:
				Ref_obj_impl* impl;
				T* ptr;
		};

	RCO_INLINE void Destructor::operator() (Ref_obj* ptr) {

		if(ctl_fun) {
			ctl_fun(ptr, fun_arg);
		} else {
			delete ptr;
		}
	}

	template <typename T>
		typename std::enable_if<std::is_base_of<Ref_obj, T>::value>::type
		IncrementRef(T * ptr)
		{
			ptr->increment_ref();
		}
	template <typename T>
		typename std::enable_if<!std::is_base_of<Ref_obj, T>::value>::type
		IncrementRef(T * ptr)
		{
		}
	template <typename T>
		typename std::enable_if<std::is_base_of<Ref_obj, T>::value>::type
		DecrementRef(T * ptr)
		{
			ptr->decrement_ref();
		}
	template <typename T>
		typename std::enable_if<!std::is_base_of<Ref_obj, T>::value>::type
		DecrementRef(T * ptr)
		{
		}
}
