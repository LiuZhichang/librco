#include "processor.h"

#include "runtime.h"
#include "scheduler.h"
#include <mutex>
#include <utility>

rco::Processor::Processor(rco::Scheduler* scheduler, int id)
	: own_scheduler(scheduler)
	, thread_id(id)
	, running_task(nullptr)
	, next_task(nullptr)
	, wait_flag(false)
	, notified(false)
	  , active(true)
	  , quota(0)
	  , gc_threshold(Runtime::GC_threshold()){

		  // 等待队列和运行队列使用同一个锁
		  wait_queue.set_lock(&runnable_queue.lock_ref());
	  }

rco::Processor* & rco::Processor::CurrentProcessor() {
	RCO_STATIC thread_local Processor* s_tl_processor = nullptr;
	return s_tl_processor;
}

rco::Scheduler* rco::Processor::CurrentScheduler() {
	Processor* processor = CurrentProcessor();
	return processor ? processor->own_scheduler : nullptr;
}

rco::Task* rco::Processor::CurrentTask() {
	Processor* proc = CurrentProcessor();
	return proc ? proc->running_task : nullptr;
}

bool rco::Processor::Working() {
	// 如果有任务表示在工作中
	return !!CurrentTask();
}

void rco::Processor::CoYield() {
	Processor* proc = CurrentProcessor();
	if(proc) {
		// 切出
		proc->coyield();
	}
}

void rco::Processor::coyield() {
	Task* task = CurrentTask();

	assert(task);
	// 协程切出
	task->yield();
}

size_t rco::Processor::runnable_count() {
	// 可运行的协程数
	// 可运行协程队列大小 + 就绪协程队列大小
	// 当可运行协程队列中协程不够时，将会把就绪队列中的协程取出
	// 因此可运行协程数 为 两个队列长度之和
	return runnable_queue.size() + ready_queue.size();
}

void rco::Processor::add_task(Task* task) {
	std::unique_lock<TaskQueue_ts::lock_t> scope_lock(ready_queue.lock_ref());

	// 放入就绪队列
	ready_queue.nolock_push(task);

	// 如果处于阻塞状态(执行器中没有可运行的协程)
	if(wait_flag) {
		// 此时加入了新的协程，因此唤醒执行器
		cv.notify_all();
	} else {
		notified = true;
	}
}

// 与上面功能一致，只不过能一次添加多个协程
void rco::Processor::add_task(TSList<Task> && list) {
	std::unique_lock<TaskQueue_ts::lock_t> scope_lock(ready_queue.lock_ref());

	ready_queue.nolock_push(std::move(list));

	if(wait_flag) {
		cv.notify_all();
	} else {
		notified = true;
	}
}

void rco::Processor::scheduling() {
	CurrentProcessor() = this;

	// 所属调度器正在运行
	while(own_scheduler->running) {
		// 从可运行队列中取出一个协程并开始执行
        runnable_queue.front(running_task);

		// 如果该协程无效, 说明队列可能为空
		if(!running_task && runnable_queue.ts_empty()) {
			// 将准备队列中的协程全部取出放到可运行队列中
			readyToRunnable();
			// 取出一个协程
            runnable_queue.front(running_task);

			// 如果该协程无效，说明准备队列也为空，此时执行器无协程可执行
			if(!running_task) {
				// 此时线程休眠，等待任务来临，唤醒
				// 更新状态标志
				wait_notify();
				// 再次尝试
				readyToRunnable();
				continue;
			}
		}
	
		quota = 1;
	
		while(running_task && own_scheduler->running) {

			// 协程状态更新为运行中
			running_task->set_state(Task::State::eRunnable);
			// 更新协程所属的执行器
			running_task->set_own_proc(this);
			
			++switch_count;

			// 协程开始执行
			running_task->resume(); // wait for until task execute finish

			// 协程切出后，根据其状态作出处理
			switch (running_task->state()) {
				case Task::State::eRunnable:
					if(!state_runnable()) {
                        break;
                    }
					break;
				case Task::State::eWait:
					state_wait();
					break;
				case Task::State::eFinish:
					state_finish();
					break;
			}
		}

	}
}

bool rco::Processor::blocking() {
	return false;
}

void rco::Processor::readyToRunnable() {
	// 将就绪队列中的协程加入到可执行协程队列
	// 一般用于 可执行协程不够的情况
	runnable_queue.push(ready_queue.pop_all());
}

// 唤醒执行器
void rco::Processor::notify() {
	std::unique_lock<TaskQueue_ts::lock_t> scope_lock(ready_queue.lock_ref());
	
	// 如果处于阻塞状态
	if(wait_flag) {
		// 唤醒
		cv.notify_all();
	} else {
		// 更新唤醒标志
		// 此时执行器已经被唤醒
		notified = true;
	}
}

void rco::Processor::wait_notify() {
	// 在等待唤醒时，清理垃圾
	gc();

	std::unique_lock<TaskQueue_ts::lock_t> scope_lock(ready_queue.lock_ref());

	// 如果已经被唤醒
	if(notified) {
		// 更新唤醒标志
		notified = false;
		return;
	}
	
	// 更新等待标志
	wait_flag = true;
	cv.wait(scope_lock);
	// 等待结束更新标志
	wait_flag = false;
}

void rco::Processor::gc() {
	TSList<Task> list = gc_queue.pop_all();

	// 减少引用计数
	for(Task& task : list) {
		task.decrement_ref();
	}
	// 清理gc队列
	list.clear();
}

rco::TSList<rco::Task> rco::Processor::steal(std::size_t n) {
	if(n > 0) {

		// 偷取一些协程

		// 从队列尾部截断n个协程
		TSList<Task> list = ready_queue.trunc_back(n);

		// 如果截断的协程数，大于等于n，则直接返回 list
		if(list.size() >= n) {
			return list;
		}

		std::unique_lock<TaskQueue_ts::lock_t> scope_lock(runnable_queue.lock_ref());

		// 此时，就绪队列中截取的协程数不够(list.size() < n)

		std::pair<bool, bool> push_sate{false,false};
		// 如果可执行协程有效 step 1
		if(running_task) {
			// 从可执行协程队列中删除
			 push_sate.first = runnable_queue.nolock_erase(running_task, true) || list.erase(running_task, ready_queue.check);
//			runnable_queue.nolock_erase(running_task);
		}
		// 如果下个可执行协程有效 step2
		if(next_task) {
			// 从可执行协程队列中删除
			 push_sate.second = runnable_queue.nolock_erase(next_task, true) || list.erase(next_task, ready_queue.check);
//			runnable_queue.nolock_erase(next_task);
		}
		// 以上两步是为了保证 可执行协程 与 下一个可执行协程 不被取出

		// 从可执行协程队列中取出 n-list.size() 个协程(即：补够缺少的协程个数)
		TSList<Task> target_list = runnable_queue.nolock_trunc_back(n - list.size());

		// 如果可执行协程删除成功
		if(push_sate.first) {
		    // 将可执行协程重新加入可执行协程队列
	    	runnable_queue.nolock_push(running_task);
		}
		// 如果下一个可执行协程删除成功
		if(push_sate.second) {
		    // 将下一个可执行协程重新加入可执行协程队列
    		runnable_queue.nolock_push(next_task);
		}

		scope_lock.unlock();

		target_list.append(std::move(list));

		return target_list;
	} else {
		// 偷取全部协程
		//
		TSList<Task> list = ready_queue.pop_all();

		std::unique_lock<TaskQueue_ts::lock_t> scope_lock(runnable_queue.lock_ref());

        std::pair<bool, bool> push_sate{false,false};
        // 如果可执行协程有效 step 1
        if(running_task) {
            // 从可执行协程队列中删除
            push_sate.first = runnable_queue.nolock_erase(running_task, true) || list.erase(running_task, ready_queue.check);
//			runnable_queue.nolock_erase(running_task);
        }
        // 如果下个可执行协程有效 step2
        if(next_task) {
            // 从可执行协程队列中删除
            push_sate.second = runnable_queue.nolock_erase(next_task, true) || list.erase(next_task, ready_queue.check);
//			runnable_queue.nolock_erase(next_task);
        }
        // 以上两步是为了保证 可执行协程 与 下一个可执行协程 不被取出

        // 从可执行协程队列中取出 n-list.size() 个协程(即：补够缺少的协程个数)
        TSList<Task> target_list = runnable_queue.nolock_trunc_back(n - list.size());

        // 如果可执行协程删除成功
        if(push_sate.first) {
            // 将可执行协程重新加入可执行协程队列
            runnable_queue.nolock_push(running_task);
        }
        // 如果下一个可执行协程删除成功
        if(push_sate.second) {
            // 将下一个可执行协程重新加入可执行协程队列
            runnable_queue.nolock_push(next_task);
        }

		scope_lock.unlock();

		target_list.append(std::move(list));

		return target_list;
	}
}

bool rco::Processor::state_runnable() {
	std::unique_lock<TaskQueue_ts::lock_t> scope_lock(runnable_queue.lock_ref());
	
	// 如果协程处于此状态，说明在协程内部调用了yield，切出去了

	// 获取下一个可执行的协程
	Task* next = static_cast<Task*>(running_task->next);
	if(next) {
		// 运行协程切换为 next 协程（此时运行中协程并未删除掉，它会等待下次调度执行）
		running_task = next;
        running_task->check = runnable_queue.check;
		return false;
	}

	// 无下一个可执行协程
	// 就绪队列也为空
	if(quota < 1 || ready_queue.empty()) {
		// 无可运行的协程，运行协程置为空
		running_task = nullptr;
	} else {
		scope_lock.unlock();

		// 将就绪队列中的协程取出放到可执行协程队列中
		readyToRunnable();

		// 可执行协程置为下一个协程(此时就绪队列已经并入可执行队列中，
		// 即：此时运行协程的next节点一定是有效协程)
        runnable_queue.next(running_task, running_task);

		--quota;
	}

    return true;
}

void rco::Processor::state_wait() {
	std::unique_lock<TaskQueue_ts::lock_t> scope_lock(runnable_queue.lock_ref());

	running_task = next_task;
	next_task = nullptr;
}

void rco::Processor::state_finish() {

	// 可执行协程执行结束，更新下一个可执行协程
    runnable_queue.next(running_task, next_task);

	// 如果下一个协程无效
	if(!next_task && quota > 0) {
		readyToRunnable();
        runnable_queue.next(running_task, next_task);
		--quota;
	}

	runnable_queue.erase(running_task);

	// 如果垃圾回收队列大小 大于阈值，开始回收垃圾
	if(gc_queue.size() > gc_threshold) {
		gc();
	}

	// 将当前执行完的协程放入垃圾回收队列
	gc_queue.push(running_task);

	std::unique_lock<TaskQueue_ts::lock_t> scope_lock(runnable_queue.lock_ref());
	running_task = next_task;
	next_task = nullptr;
}



void rco::Processor::make_tag() {

}	
