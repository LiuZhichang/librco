#include "task.h"
#include <functional>

rco::Task::Task(const Execute& exec, const Attribute& attr)
	: Intrusive_queue()
    , ctx(&Task::DoWork, this, attr.stack_size)
	, execute(std::move(exec))
	, exec_state(State::eRunnable)
	  , processor(nullptr)
	  , unique_id(0){

	  }

rco::Task::~Task() {

}

void rco::Task::yield() {
	ctx.swap_out();
}

void rco::Task::resume() {
	ctx.swap_in();
}

void rco::Task::run() {
	try {
		execute();
        execute = Execute();
	} catch(...) {
		execute = Execute();
	}

	exec_state = State::eFinish;
	yield();
}

void rco::Task::DoWork(void* arg) {
	Task* task = (Task*)arg;
	task->run();
}
