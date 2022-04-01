#pragma once

#include <cstdint>
#include <atomic>

namespace rco {
	class Runtime {
		struct Env {
			std::atomic<uint16_t> proc_count;
			std::atomic<uint16_t> gc_threshold;
			std::atomic<float>	  load_balance_rate;
			Env();
		};
		public:

		Runtime() = delete;

		static void Sched();
		static void Rco_Exit();
		static void Set_max_procs(uint16_t n);
		static uint16_t CPU_count();
		static void GC();
		static void Set_GC_threshold(std::size_t val);
		static uint16_t GC_threshold();
		static void Set_load_balance(std::size_t rate);
		static float Load_balance_rate();
		private:
		static Env env;
	};
}
