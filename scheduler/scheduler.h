#pragma once

#include "../common/internal.h"
#include "../common/noncopyable.h"
#include "../task/task.h"

#include <deque>
#include <map>
#include <mutex>
#include <thread>

namespace rco {

	class Runtime;
	class Processor;

	/**
	 * @brief 协程调度类，调度Processor与Task
	 */
	class Scheduler : public Noncopyable {
		friend Processor;
		friend Runtime;
		public:

		/**
		 * @brief 单例
		 *
		 * @return 调度器对象
		 */
		RCO_STATIC Scheduler& Instance();

		/**
		 * @brief 创建调度器
		 *
		 * @return 调度器对象
		 */
		RCO_STATIC Scheduler* Make();

		/**
		 * @brief 创建协程
		 *
		 * @param[in] execute 执行任务实体
		 * @param[in] attr	  协程属性
		 */
		void make_task(const Task::Execute& execute, const Task::Attribute& attr);

		/**
		 * @brief 是否在执行协程中
		 *
		 * @return 是 ? true : false
		 */
		bool working();

		/**
		 * @brief 执行器队列是否为空
		 *
		 * @return 是 ? true : false
		 */
		bool empty();

		/**
		 * @brief 开启调度
		 *
		 * @param[in] min_thread_cnt 最小线程数
		 * @param[in] max_thread_cnt 最大线程数
		 */
		void start(uint16_t min_thread_cnt, uint16_t max_thread_cnt);

		/**
		 * @brief 停止调度
		 */
		void stop();

		private:
		Scheduler();
		~Scheduler();

		/**
		 * @brief 添加协程到相应的执行器中
		 *
		 * @param[in] task 协程对象
		 */
		void add_task(Task* task);

		/**
		 * @brief 创建执行器线程(processor 与 thread 为 1 : 1)
		 */
		void make_processor_thread();

		/**
		 * @brief 删除协程
		 *
		 * @param[in] task 引用对象指针，在此为协程对象指针
		 * @param[in] arg  调度器对象指针
		 *
		 * @return void
		 */
		RCO_STATIC void DelTask(Ref_obj* task, void* arg);

		/**
		 * @brief 注册退出函数
		 *
		 * @return void
		 */
		RCO_STATIC void OnExit();

		/**
		 * @brief 退出时需要执行的工作，进行资源回收
		 *
		 * @return void
		 */
		RCO_STATIC void OnExitDoWork();

		/**
		 * @brief 使所有执行器开始gc
		 */
		void GC();

		/**
		 * @brief 更新所有执行器的gc阈值
		 *
		 * @param[in] n 阈值
		 */
		void update_threshold(size_t n);

		void do_dispatch();

		void dispatch_task(std::multimap<std::size_t, std::size_t>& active, std::map<std::size_t, std::size_t>& blocking);
		void load_balance(std::multimap<std::size_t, std::size_t>& active, std::size_t active_tasks);
		private:
		bool running;

		Spin_lock started;

		std::deque<Processor*> processors;
		std::mutex			   mutex;

		std::thread timer_thread;
		std::thread dispatch_thread;

		std::atomic<uint32_t> task_count;

		volatile uint32_t last_active;

		uint16_t min_thread_count;
		uint16_t max_thread_count;
	};

}
