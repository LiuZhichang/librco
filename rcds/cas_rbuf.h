#pragma once

#include <atomic>
#include <bits/stdint-uintn.h>
#include <limits>
#include <utility>
#include <assert.h>

#include <iostream>

#include "../common/internal.h"
#include "../common/flags.h"

namespace rco {
	namespace cas {


		/**
		 * @brief 无锁环形队列
		 *
		 * @tparam T
		 */
		template<typename T>
			class LockFreeRingBuf {
				public:
					
					/**
					 * @brief 队列状态
					 */
					enum class State: Flag_type{
						eSuccess = 0x01,
						eEmpty   = 0x02,
						eFull    = 0x04
					};
					using FlagBits = Flags<State>;
					using atomic_t = std::atomic<uint32_t>;


					/**
					 * @brief 无锁环形缓冲区构造函数
					 *
					 * @param[in] capacity 缓冲区容积
					 */
					explicit LockFreeRingBuf(uint32_t capacity)
						: buf_capacity(capacity + 1)/*预留一个单元不存储*/
						  , write(0, capacity)  /*可写范围为0,buf_cap -1*/
						  , read(0, 0) {
							  assert(capacity);
							  buffer = (T*)malloc(sizeof(T) * buf_capacity);
						  }

					//			LockFreeRingBuf()
					//				:LockFreeRingBuf(N) {
					//					static_assert(N, "use default constructor must specify the size");
					//				}

					/**
					 * @brief 析构函数，调用所有元素的析构函数并且释放内存
					 */
					~LockFreeRingBuf() {
						uint32_t r_begin = consume(read.begin);
						uint32_t r_end = consume(read.end);

						for (; r_begin < r_end; ++r_begin) {
							buffer[r_begin].~T();
						}

						free(buffer);
					}

					/**
					 * @brief 添加元素
					 *
					 * @tparam M 可选的move方法
					 * @param[in] t M的实例 
					 *
					 * @return 状态位
					 */
					template<typename M>
						FlagBits push(M &&t) {

							FlagBits state;

							// 1. 可写区间减去一个单元
							Range <uint32_t> write_range;
							do {

								write_range.begin = relaxed(write.begin);
								write_range.end = consume(write.end);
								
								// 如果写入区间大小为空，则队列已满
								if (!write_range.has_free()) {
									// 更新状态
									state |= State::eFull;
									return state;
								}

							} while (!write.begin.compare_exchange_weak(write_range.begin
										, (write_range.begin + 1) % buf_capacity
										, std::memory_order_acq_rel, std::memory_order_relaxed));

							// 2. 写入
							new(buffer + write_range.begin) T(std::forward<M>(t));

							// 3. 更新可读区间
							uint32_t read_end;
							do {
								read_end = relaxed(read.end);
							} while (!read.end.compare_exchange_weak(write_range.begin
										, (read_end + 1) % buf_capacity
										, std::memory_order_acq_rel, std::memory_order_relaxed));

							// 4. 更新状态
							//	检查写入时是否为空
							state |= (write_range.begin == ((write_range.end + 1) % buf_capacity)) ? State::eEmpty : State();
							state |= (!write.has_free()) ? State::eFull : State();
							// 写入成功
							state |= State::eSuccess;

							return state;
						}

					FlagBits pop(T &t) {
						FlagBits state;

						// 1. 写入区间缩小一个单元
						Range<uint32_t> read_range;
						do {
							read_range.begin = relaxed(read.begin);
							read_range.end = consume(read.end);

							if(!read_range.has_free()) {
								state |= State::eEmpty;
								return state;
							}

						} while(!read.begin.compare_exchange_weak(read_range.begin
									, (read_range.begin + 1) % buf_capacity
									, std::memory_order_acq_rel, std::memory_order_relaxed) );

						// 2.读数据
						t = std::move(buffer[read_range.begin]);
						buffer[read_range.begin].~T();
						
						// 3.更新写入范围
						uint32_t check = (read.begin + buf_capacity - 1) % buf_capacity;
						while(!write.end.compare_exchange_weak(check, read.begin
									, std::memory_order_acq_rel, std::memory_order_relaxed));

						// 4.检查读取时是否为full状态
						state |= (read_range.begin == ((read.end + 1) % buf_capacity)) ? State::eFull : State();
						state |= !read.has_free() ? State::eEmpty : State();
						state |= State::eSuccess;

						return state;
					}

				private:
					/**
					 * @brief 读写范围, 闭区间 [begin, end]
					 */
					template<typename Type = atomic_t, typename ArgT = uint32_t>
						struct Range {
							Type begin;
							Type end;

							Range(ArgT _begin, ArgT _end)
								: begin(_begin), end(_end) {

								}

							Range() = default;

							bool has_free() {
								return !(begin == end);
							}
						};

					/**
					 *	根据不同的内存访问顺序取出元素
					 *
					 */

					/**
					 * @brief  只保证当前操作的原子性，不考虑线程间的同步，
					 *	其他线程可能读到新值，也可能读到旧值。
					 *	比如 C++ shared_ptr 里的引用计数，
					 *	我们只关心当前的应用数量，而不关心谁在引用谁在解引用。
					 *
					 * @param[in] value
					 *
					 * @return
					 */
					RCO_INLINE uint32_t relaxed(atomic_t &value) {
						return value.load(std::memory_order_relaxed);
					}

					RCO_INLINE uint32_t acquired(atomic_t &value) {
						return value.load(std::memory_order_relaxed);
					}

					/**
					 * @brief 对读取施加 acquire 语义（load），
					 * 在代码中这条语句后面所有读写操作都无法重排到这个操作之前.
					 *
					 * 在这个原子变量上施加 release 语义的操作发生之后，
					 * acquire 可以保证读到所有在 release 前发生的写入
					 *
					 * @param[in] value
					 *
					 * @return
					 */
					RCO_INLINE uint32_t consume(atomic_t &value) {
						return value.load(std::memory_order_consume);
					}

				private:

					Range<atomic_t> write;
					Range<atomic_t> read;

					uint32_t buf_capacity;
					T *buffer;
			};

	}
}

