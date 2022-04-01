#pragma once

#include "../common/internal.h"

#include "details/context.h"

#include <cstdlib>
#include <memory>

namespace rco {
	

	/**
	 * @brief Context的包装类
	 */
	class RContext {
		public:
			typedef core::Context::rco_func rctx_fn;
		
			explicit RContext(rctx_fn pfn, void* arg, size_t stack_size);
			~RContext();

			void swap_in();
			void swap_out();

			RContext& GetCtx();
		private:
			core::Context ctx;
			rctx_fn		  pfn;
			void*		  pAllocator;
	};
}
