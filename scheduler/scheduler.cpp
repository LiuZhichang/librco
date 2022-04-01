#include "scheduler.h"

#include <iostream>

#include <atomic>
#include <cstdlib>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>
#include <chrono>

#include "processor.h"
#include "runtime.h"

struct Exit_Op {
	std::mutex ex_mtx;
	std::vector<std::function<void()> >* ex_cbs;

	Exit_Op() {
		ex_cbs = new std::vector<std::function<void()> >;
	}

	~Exit_Op() {
		delete ex_cbs;
	}

	RCO_STATIC Exit_Op& Instance() {
		RCO_STATIC Exit_Op ex_op;
		return ex_op;
	}

	RCO_INLINE void exec_cb() {
		for(auto cb : *ex_cbs) {
			cb();
		}
		ex_cbs->clear();
	}

	RCO_INLINE void register_cb(std::function<void()>&& cb) {
		ex_cbs->push_back(std::move(cb));
	}
};

struct ID_Generator {

	RCO_STATIC ID_Generator& Instance() {
		RCO_STATIC ID_Generator generator;
		return generator;
	}

	RCO_INLINE uint64_t make() {
		return ++id;
	}

	ID_Generator()
		: id(0) {

		}

	private:
	std::atomic<uint64_t> id;
};

void rco::Scheduler::OnExit() {
	atexit(&OnExitDoWork);
}

void rco::Scheduler::OnExitDoWork() {
	Exit_Op::Instance().exec_cb();
}

rco::Scheduler::Scheduler()
	: running(true)
	  , min_thread_count(1)
	  , max_thread_count(1)
      , task_count(0)
      , last_active(0){
		  // 初始执行器
		  processors.push_back(new Processor(this, 0));
	  }

rco::Scheduler::~Scheduler() {
	stop();
}

rco::Scheduler& rco::Scheduler::Instance() {
	RCO_STATIC Scheduler s_sched;
	return s_sched;
}

rco::Scheduler* rco::Scheduler::Make() {
	Scheduler* sched = new Scheduler;
	std::unique_lock<std::mutex> scope_lock(Exit_Op::Instance().ex_mtx);
	// 注册资源回收回调
	Exit_Op::Instance().register_cb(
			std::move([=]{
				delete sched;
				})
			);

	return sched;
}

void rco::Scheduler::make_task(const Task::Execute& execute, const Task::Attribute& attr) {
	
	Task* task = new Task(execute, attr);
	// 注册资源回收回调
	task->set_destructor(Destructor(&Scheduler::DelTask, this));
	// 生成协程id
	uint64_t id = ID_Generator::Instance().make();
	task->set_id(id);

	++task_count;

	add_task(task);
}

bool rco::Scheduler::working() {
	return !!Processor::CurrentTask();
}

bool rco::Scheduler::empty() {
	return (task_count == 0);
}

void rco::Scheduler::start(uint16_t min_thread_cnt, uint16_t max_thread_cnt) {
	if(!started.try_lock())	{
		throw std::logic_error("repeated call start func.");
	}

	if(min_thread_cnt == 0) {
		min_thread_cnt = Runtime::CPU_count();
	}

	if(max_thread_cnt == 0 || max_thread_cnt < min_thread_cnt) {
		max_thread_cnt = min_thread_cnt;
	}

	min_thread_count = min_thread_cnt;
	max_thread_count = max_thread_cnt;

	// 主执行器
	Processor* main_proc = processors[0];

	// 创建执行器
	for(int i = 0; i < min_thread_count - 1; ++i) {
		make_processor_thread();
	}

	// 如果最大线程数大于1
	if(max_thread_count > 1) {
		// 开启调度线程(也可叫分发线程，用来委派任务)
		std::thread th([this]{
				this->do_dispatch();
				});
		dispatch_thread.swap(th);
	} else {
//		throw std::runtime_error("no dispatch thread");
	}
	// 主执行器开始调度
	main_proc->scheduling();
}

void rco::Scheduler::stop() {
	std::unique_lock<std::mutex> scope_lock(mutex);

	// 如果已经停止 直接返回
	if(!running) return;

	// 更新运行状态
	running = false;

	// 唤醒所有执行器，执行任务
	for(Processor* p : processors) {
		if(p) {
			p->wait_notify();
		}
	}

	// 任务分发线程
	if(dispatch_thread.joinable()) {
		dispatch_thread.join();
	}

	// 定时器线程
//	if(timer_thread.joinable()) {
//		timer_thread.join();
//	}
}

void rco::Scheduler::DelTask(rco::Ref_obj* task, void* arg) {
	Scheduler* self = static_cast<Scheduler*>(arg);
	delete task;
	--self->task_count;
}

void rco::Scheduler::make_processor_thread() {
	Processor* p = new Processor(this, processors.size());

	// 开启调度线程
	std::thread([this,p]{
			p->scheduling();
			}).detach();

	// 记录该执行器
	processors.push_back(p);
}

void rco::Scheduler::add_task(Task* task) {
	Processor* proc = task->own_proc();

	// 如果所属执行器有效(因此有可能是第一次创建的Task，不是旧的Task)
	// 如果执行器有效且处于激活状态
	if(proc && proc->active) {
		// 直接将Task放入
		proc->add_task(task);
		return;
	}

	// 此时，proc可能无效，也可能是未激活的
	// 获取当前运行的执行器
	proc = Processor::CurrentProcessor();
	// 如果执行器有效 并且处于激活态 并且属于当前调度器
	if(proc && proc->active && (proc->belong_scheduler() == this) ) {
		// 则 直接加入到当前proc中
		proc->add_task(task);
		return;
	}

	std::size_t count = processors.size();
	std::size_t last_actvive_index = last_active;

	for(std::size_t i = 0; i < count; ++i, ++last_actvive_index) {
		last_actvive_index = last_actvive_index % count;
		proc = processors.at(last_actvive_index);

		if(proc && proc->active) {
			break;
		}
	}

	proc->add_task(task);
}

void rco::Scheduler::GC() {
	for(Processor* p : processors) {
		p->gc();
	}
}

void rco::Scheduler::update_threshold(size_t n) {
    if(processors.empty()) return;
	for(Processor* p : processors) {
		p->set_threshold(n);
	}
}

void rco::Scheduler::do_dispatch() {
	while(running) {

		// 每1000毫秒调度一次
		std::this_thread::sleep_for(std::chrono::microseconds(1000));

		// 1. 收集负载值, 记录阻塞状态的p，设置阻塞标记，唤醒处于等待但是有任务的p
		std::size_t proc_count = processors.size();
		std::size_t total_load_average = 0;

		using Pos = std::size_t;
		// 记录激活队列
		std::multimap<std::size_t, Pos> active;
		// 记录阻塞队列
		std::map<Pos, std::size_t> blocking;

		int active_count = 0;
		for(std::size_t i = 0; i < proc_count; ++i) {
			Processor* p = processors[i];
			// 不处于等待状态但是阻塞的执行器
			if(!p->waiting() && p->blocking()) {
				// 记录阻塞队列 中可执行协程的个数(同时记录其在processor表中的位置)
				blocking[i] = p->runnable_count();
				// p->active 只在任务分发线程中更改
				if(p->active) {
					p->active = false;
				}
			}

			// 有效的并且已经激活的协程 计数
			if(p->active) {
				++ active_count;
			}
		}

		// 能够激活的 执行器(Processor) 个数
		// proc_count 记录的是激活未阻塞的 执行器
		// min_thread_count 记录的是 执行器 的创建数(start函数中创建了min_thread_count个执行器)
		// canActivated就是未激活的 执行器 个数
		int canActivated = active_count < min_thread_count ? (min_thread_count - active_count) : 0;

		std::size_t active_task_count = 0;
		for(std::size_t i = 0; i < proc_count; ++i) {
			Processor* p = processors[i];
			// 获取负载值 p` size(ready_queue + runnable_queue)
			std::size_t load_average = p->runnable_count();

			//	所有执行线程的负载
			total_load_average += load_average;

			if(!p->active) {
				// p 未激活，处于等待状态
				if(canActivated > 0 && (!p->blocking() || p->waiting()) ) {
					// 激活执行器
					p->active = true;
					-- canActivated;
					last_active = i;
				}
			}

			if(p->active) {
				// p 已激活
				active.insert({load_average, i});
				// 记录可执行协程的数量
				active_task_count += p->runnable_count();
				// 执行器打上标记, 标记该执行器没有阻塞
				p->make_tag();
			}

			// 如果该执行器负载不为0（有任务执行），并且为等待状态
			if(load_average > 0 && p->waiting()) {
				// 唤醒执行器
				p->notify();
			}

		}

		// 没有可用的执行器，但是可以创建更多的执行器线程(执行器数量没有达到最大)
		if(active.empty() && proc_count < max_thread_count) {
			make_processor_thread();
			active.insert({0, proc_count});
			++ proc_count;
		}

		// 没有可用执行器，且无法创建新的执行器
		if(active.empty()) {
			continue;
		}

		dispatch_task(active, blocking);

		load_balance(active, active_task_count);
	}

}


void rco::Scheduler::dispatch_task(std::multimap<std::size_t, std::size_t>& active, std::map<std::size_t, std::size_t>& blocking) {
	// 阻塞协程个数为0
	if(blocking.size() == 0) {
		return;
	}

	TSList<Task> tasks;
	for(auto& kv : blocking) {
		// 阻塞的执行器
		Processor* p = processors[kv.first];
		// 偷取全部协程(除了正在运行的协程和下一个要运行的协程)
		tasks.append(p->steal(0));
	}

	// 没有可以执行的协程
	if(tasks.empty()) {
		return;
	}

	using Pos = uint32_t;
	// 记录激活队列
	std::multimap<std::size_t, Pos> new_active;
	// 协程总数
	std::size_t total_task_count = tasks.size();
	// 需要平分协程的processor的数量
	std::size_t devide_num = 0;
	// 平分的协程数量
	std::size_t avg = 0;

	auto needDevide_proc = active.begin();
	for(; needDevide_proc != active.end(); ++needDevide_proc) {
		// needDevide_proc->first 就是该processor中可以执行的协程数量
		total_task_count += needDevide_proc->first;

		// 需要平分协程的processor的数量增加
		++devide_num;

		// 计算出平分的协程数
		avg = total_task_count / devide_num;

		// 如果该执行器中可执行的协程数量大于平分数，则将其移除
		if(needDevide_proc->first > avg) {

			// 协程总数更新
			total_task_count -= needDevide_proc->first;
			// 需要平分协程的processor的数量更新
			--devide_num;
			// 重新计算平分协程数
			avg = total_task_count / devide_num;
			break;
		}

		if(needDevide_proc != active.end()) {
			++needDevide_proc;
		}

		auto it = active.begin();
		for(; it != needDevide_proc; ++it) {
			// 保证需要分配协程的执行器都有平均数个协程
			TSList<Task> target_list = tasks.truncation(avg - it->first);

			if(target_list.empty()) {
				break;
			}

			Processor* proc = processors[it->second];
			if(proc) {
				proc->add_task(std::move(target_list));
			}
		}

		if(!tasks.empty()) {
			Processor* proc = processors[active.begin()->second];
			proc->add_task(std::move(tasks));
		}
	}
}

void rco::Scheduler::load_balance(std::multimap<std::size_t, std::size_t>& active, std::size_t active_tasks) {

	// 激活的平均协程数
	std::size_t avg = active_tasks / active.size();

	if(active.begin()->first > (avg * Runtime::Load_balance_rate()) ) {
		return;
	}

	TSList<Task> tasks;
	for(auto it = active.rbegin(); it != active.rend(); ++it) {
		// 执行器的可执行协程数小于平均值
		if(it->first <= avg) {
			// 直接跳出
			break;
		}

		Processor* p = processors[it->second];

		// 取出大于平均数的协程
		TSList<Task> target_list = p->steal(it->first - avg);

		tasks.append(std::move(target_list));
	}

	// 没有多余的协程分配
	if(tasks.empty()) {
		return;
	}

	for(auto& kv : active) {
		if(kv.first >= avg) {
			break;
		}

		Processor* p = processors[kv.second];

		// 此时，已经确保执行器的可执行协程数少于平均数
		TSList<Task> target_list = tasks.truncation(avg - kv.first);

		p->add_task(std::move(target_list));
	}

	if(!tasks.empty()) {
		Processor* p = processors[active.begin()->second];
		p->add_task(std::move(tasks));
	}

}
