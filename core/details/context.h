#pragma once

#include <cstring>
#include <cstdint>

namespace rco {
	namespace core {
		struct Context {
			typedef void(*rco_func)(void*);
			
			Context() {
				std::memset(this, 0, sizeof(Context));
			}

#ifdef __i386__
			void*  regs[5];
#elif __x86_64__
			void*  regs[9];
#else
	#error "platform no support yet"
#endif
			void* stack_ptr;
			size_t stack_size;
		};

		extern "C" {
			void rco_make_context(Context* ctx,  Context::rco_func pfn, void* arg);
			void rco_jump_context(Context*, Context*) asm("rco_jump_context");
		}

	}
}
