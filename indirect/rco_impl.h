#pragma once
#include <cstdint>

#include "../common/internal.h"
#include "../task/task.h"
#include "../scheduler/processor.h"
#include "../scheduler/scheduler.h"

#include <iostream>

namespace rco {
	class Scheduler;
	namespace impl {
		enum class Opt{
			eScheduler,
			eStackSize,
			eDispath
		};

		template <Opt Opt_t>
			struct __rco_option;

		template <>
			struct __rco_option<Opt::eScheduler> {
				Scheduler* __scheduler;
				explicit __rco_option(Scheduler* scheduler)
					: __scheduler(scheduler) {}
				explicit __rco_option(Scheduler& scheduler)
					: __scheduler(&scheduler) {}
			};
		template <>
			struct __rco_option<Opt::eStackSize> {
				std::size_t __stack_size;
				explicit __rco_option(std::size_t ss)
					: __stack_size(ss) {}
			};
		template <>
			struct __rco_option<Opt::eDispath> {
			};


		struct __rco {
            __rco() {
				rco_scheduler = nullptr;
			}

			template <typename Co_Task>
				RCO_INLINE void operator + (const Co_Task& fun) {
					if(!rco_scheduler) {
						rco_scheduler = Processor::CurrentScheduler();
					}
					if(!rco_scheduler) {
						rco_scheduler = &Scheduler::Instance();
					}
					rco_scheduler->make_task(fun, rco_task_attr);
				}

			RCO_INLINE __rco& operator - (const __rco_option<Opt::eScheduler>& opt) {
				rco_scheduler = opt.__scheduler;
				return *this;
			}
			RCO_INLINE __rco& operator - (const __rco_option<Opt::eStackSize>& opt) {
				rco_task_attr.stack_size = opt.__stack_size;
				return *this;
			}

			Task::Attribute rco_task_attr;
			Scheduler* rco_scheduler;
		};

	}
}
