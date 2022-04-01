#include "runtime.h"

#include <thread>

#include "processor.h"
#include "scheduler.h"

rco::Runtime::Env rco::Runtime::env;

rco::Runtime::Env::Env()
	: proc_count(std::thread::hardware_concurrency())
	  ,gc_threshold(16)
	  , load_balance_rate(0.01) {

	  }

void rco::Runtime::Sched() {
	Processor::CoYield();
}

void rco::Runtime::Rco_Exit() {

}

void rco::Runtime::Set_max_procs(uint16_t n) {
	unsigned int max = std::thread::hardware_concurrency();
	env.proc_count = (n > max) ? max : n;
}

uint16_t rco::Runtime::CPU_count() {
	return env.proc_count;
}

void rco::Runtime::GC() {
	Processor::CurrentScheduler()->GC();
}

void rco::Runtime::Set_GC_threshold(std::size_t val) {
	env.gc_threshold = val;
	if(!Processor::CurrentScheduler()) return;
	Processor::CurrentScheduler()->update_threshold(env.gc_threshold);
}

uint16_t rco::Runtime::GC_threshold() {
	return env.gc_threshold;
}


void rco::Runtime::Set_load_balance(std::size_t rate) {
	if(rate) {
		env.load_balance_rate = rate;
	}
}

float rco::Runtime::Load_balance_rate() {
	return env.load_balance_rate;
}
