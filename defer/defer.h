#pragma once

namespace rco {
#pragma once

#define __SCOPEGUARD_CAT_IMPL(s1,s2) s1##s2
#define __SCOPEGUARD_CAT(s1,s2) __SCOPEGUARD_CAT_IMPL(s1,s2)
#define defer \
	auto __SCOPEGUARD_CAT(exit_block__,__LINE__) = rco::detail::ScopeGuardOnExit() + [&]()

#include <utility>

	/**
	 * @brief 利用RAII机制,当对象构造完成时,编译器会保证当程序结束时会调用对应的析构(发生异常也会)
	 *
	 * @tparam func 函数(这里使用lambda表达式)
	 */
	template <typename func>
		class ScopeGuard {
			public:
				ScopeGuard(func&& fun): fun_(std::forward<func>(fun)), dismiss_(false) {}
				ScopeGuard(ScopeGuard&& oth): fun_(std::move(oth.fun_)), dismiss_(oth.dismiss_) {}
				~ScopeGuard() {
					if(!dismiss_)
						fun_();
				}

				void dismissed() {
					dismiss_ = true;
				}

				ScopeGuard(const ScopeGuard&) = delete;
				ScopeGuard operator=(const ScopeGuard&) = delete;
			private:
				func fun_;
				bool dismiss_;
		};

	namespace detail {

		enum class ScopeGuardOnExit {};

		template <typename func>
			ScopeGuard<func> operator + (ScopeGuardOnExit, func&& fun) {
				return ScopeGuard<func>(std::forward<func>(fun));
			}

	}

}
