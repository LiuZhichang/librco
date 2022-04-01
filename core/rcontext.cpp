#include "rcontext.h"
#include "details/context.h"

//#include <jemalloc/jemalloc.h>

#include <assert.h>

struct ContextIniter {
	rco::RContext ctx;
	ContextIniter()
		: ctx(nullptr, nullptr,0) {

		}
};

rco::RContext::RContext(rctx_fn pfn, void* arg, size_t stack_size) {

	ctx.stack_size = stack_size;
	ctx.stack_ptr = (char*)malloc(stack_size);

	core::rco_make_context(&ctx, pfn, arg);
}

rco::RContext::~RContext() {

}

void rco::RContext::swap_out() {
//	assert(&GetCtx().ctx == &ctx);
	core::rco_jump_context(&ctx, &GetCtx().ctx);
}

void rco::RContext::swap_in() {
//	assert(&GetCtx().ctx == &ctx);
	core::rco_jump_context(&GetCtx().ctx, &ctx);
}

rco::RContext& rco::RContext::GetCtx() {
	RCO_STATIC thread_local ContextIniter s_tl_ctx_initer;
//    RCO_STATIC thread_local core::Context s_tl_ctx;
	return s_tl_ctx_initer.ctx;
}

