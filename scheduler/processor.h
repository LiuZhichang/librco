#pragma once

#include "../task/task.h"

#include "runtime.h"

#include <atomic>
#include <cstdint>
#include <condition_variable>

namespace rco {
	class Runtime;
	class Scheduler;
	class Processor {
		friend class Scheduler;

		using TaskQueue_ts = TSQueue<Task, true>;
		using TaskQueue    = TSQueue<Task, false>;

		using Cond_Var	   = std::condition_variable_any;
		using Atomic_Flag  = std::atomic_bool;

		public:

		/**
		 * @brief 获取线程id(唯一标识)
		 *
		 * @return 线程id
		 */
		RCO_INLINE uint16_t id() const {
			return thread_id;
		}

		/**
		 * @brief 获取当前工作中的执行器
		 *
		 * @return 执行器
		 */
		RCO_STATIC Processor* & CurrentProcessor();

		/**
		 * @brief 获取当前工作中的执行器所属的调度器
		 *
		 * @return 调度器
		 */
		RCO_STATIC Scheduler* CurrentScheduler();

		/**
		 * @brief 获取所属的调度器
		 *
		 * @return 调度器
		 */
		RCO_INLINE Scheduler* belong_scheduler() const {
			return own_scheduler;
		}

		/**
		 * @brief 获取当前运行的任务
		 *
		 * @return 任务对象
		 */
		RCO_STATIC Task* CurrentTask();

		/**
		 * @brief 当前执行器是否在工作中
		 *
		 * @return 是 ? true : false
		 */
		RCO_STATIC bool Working();

		RCO_INLINE bool waiting() const {
			return wait_flag;
		}

		bool blocking();

		/**
		 * @brief 切出当前协程
		 *
		 * @return void
		 */
		RCO_STATIC void CoYield();

		private:

		/**
		 * @brief processer的有参构造
		 *
		 * @param[in] scheduler 执行器所属的调度器
		 * @param[in] id 线程id
		 */
		explicit Processor(Scheduler* scheduler, int id);

		/**
		 * @brief 获取待执行协程数
		 *
		 * @return 待执行协程数
		 */
		size_t runnable_count();

		/**
		 * @brief 协程切出(让出执行权)
		 */
		void coyield();

		/**
		 * @brief 添加协程
		 *
		 * @param[in] task 任务对象(对应于协程)
		 */
		void add_task(Task* task);

		/**
		 * @brief 批量添加协程
		 *
		 * @param[in] list 协程列表
		 */
		void add_task(TSList<Task> && list);

		/**
		 * @brief 开始调度
		 */

		void scheduling();

		/**
		 * @brief 唤醒线程
		 */
		void notify();

		void readyToRunnable();

		void wait_notify();

		void make_tag();

		void gc();

		TSList<Task> steal(std::size_t n);

		bool state_runnable();
		void state_wait();
		void state_finish();
		
		void set_threshold(size_t n) {
			gc_threshold = n;
		}

		private:
		Scheduler*		own_scheduler;  // 所属的调度器
		uint32_t		thread_id;		// 线程id

		Task*			running_task;	// 正在运行的协程
		Task*			next_task;		// 下一个要运行的协程

		TaskQueue_ts	runnable_queue; // 可运行的协程队列
		TaskQueue_ts	wait_queue;		// 阻塞的协程队列
		TaskQueue_ts	ready_queue;	// 就绪的协程队列

		TaskQueue		gc_queue;		// 垃圾回收队列

		Cond_Var		cv;				// 条件变量
		Atomic_Flag		wait_flag;		// 等待标记

		bool			notified;		// 唤醒标志
		volatile bool	active;

		size_t			gc_threshold;

		int				quota;

		volatile uint64_t tag_tick;
		volatile uint64_t tag_switch;

		volatile uint64_t switch_count;
	};
}
