#pragma once

#include "rco_impl.h"


#define __rco_impl ::rco::impl::__rco() +
#define rco_exec __rco_impl
#define rco_sched rco::Scheduler::Instance()
