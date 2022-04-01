#pragma once

#include <type_traits>
#include <initializer_list>
#include <cstdint>
#include <bitset>

#include <iostream>

#include "internal.h"

namespace rco {

	/**
	 * @brief 重定义，使用Flags类的枚举类底层类型都必须为FlagType
	 */
	using Flag_type= uint32_t;

	template <typename E>
		class Flags {

			/**
			 * @brief 静态断言，模版参数必须为枚举类，并且大小必须小于等于FlagType
			 */
			static_assert(std::is_enum<E>::value, "template type must is enum");
			static_assert(sizeof(E) <= sizeof(Flag_type), "enum underlying_type size must less than uint32_t");
			public:

			/**
			 * @brief 重定义类型
			 */
			using enum_type = typename std::decay<E>::type;
			using underlying_type = typename std::underlying_type<E>::type;
			using impl_type = typename std::make_unsigned<underlying_type>::type;

			public:

			/**
			 * @brief 无参构造
			 */
			Flags() RCO_NOEXCEPT {
				reset();
			}

			Flags(const Flags& flag) RCO_NOEXCEPT = default;
			Flags(Flags&& flag) RCO_NOEXCEPT = default;

			Flags& operator = (const Flags &flag) RCO_NOEXCEPT = default;
			Flags& operator = (Flags &&flag) RCO_NOEXCEPT = default;

			/**
			 * @brief 接受枚举类型的构造函数
			 *
			 * @param[in] e 枚举类型
			 *
			 */
			RCO_CONSTEXPR Flags(enum_type e) RCO_NOEXCEPT
				: value(static_cast<impl_type>(e) ) {}

			Flags &operator = (enum_type e) RCO_NOEXCEPT {
				value = static_cast<impl_type>(e);
				return *this;
			}

			RCO_CONSTEXPR explicit operator bool() const RCO_NOEXCEPT { return value != 0; }

			RCO_CONSTEXPR bool operator!() const RCO_NOEXCEPT { return !value; }

			friend RCO_CONSTEXPR bool operator==(Flags fl1, Flags fl2) {
				return fl1.value == fl2.value;
			}

			friend RCO_CONSTEXPR bool operator!=(Flags fl1, Flags fl2) {
				return fl1.vaule != fl2.value;
			}

			RCO_CONSTEXPR Flags operator~() const RCO_NOEXCEPT { return Flags(~value); }

			Flags &operator|=(const Flags &fl) RCO_NOEXCEPT {
				value |= fl.value;
				return *this;
			}

			Flags &operator&=(const Flags &fl) RCO_NOEXCEPT {
				value &= fl.value;
				return *this;
			}

			Flags &operator^=(const Flags &fl) RCO_NOEXCEPT {
				value ^= fl.value;
				return *this;
			}

			Flags &operator|=(enum_type e) RCO_NOEXCEPT {
				value |= static_cast<impl_type>(e);
				return *this;
			}

			Flags &operator&=(enum_type e) RCO_NOEXCEPT {
				value &= static_cast<impl_type>(e);
				return *this;
			}

			Flags &operator^=(enum_type e) RCO_NOEXCEPT {
				value ^= static_cast<impl_type>(e);
				return *this;
			}

			RCO_CONSTEXPR Flags operator|(Flags f) RCO_NOEXCEPT {
				return Flags{static_cast<enum_type>(value | f.value)};
			}

			RCO_CONSTEXPR Flags operator&(Flags f) RCO_NOEXCEPT {
				return Flags{static_cast<enum_type>(value & f.value)};
			}

			RCO_CONSTEXPR Flags operator^(Flags f) RCO_NOEXCEPT {
				return Flags{static_cast<enum_type>(value ^ f.value)};
			}

			void swap(Flags &fl) RCO_NOEXCEPT { std::swap(value, fl.value); }

			RCO_CONSTEXPR underlying_type underlying_value() const RCO_NOEXCEPT {
				return static_cast<underlying_type>(value);
			}

			RCO_INLINE RCO_CONSTEXPR RCO_STATIC std::size_t bit_size() { return sizeof(impl_type) * 8; }

			RCO_CONSTEXPR explicit operator std::bitset<Flags<E>::bit_size()>() const RCO_NOEXCEPT {
				return to_bitset();
			}

			RCO_CONSTEXPR std::bitset<rco::Flags<E>::bit_size()> to_bitset() const RCO_NOEXCEPT {
				return {value};
			}

			RCO_INLINE void reset() {
				value = 0;
			}
			private:
			impl_type value;
		};
}
