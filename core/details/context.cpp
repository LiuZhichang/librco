#include "context.h"

#include <assert.h>
#include <cstring>

#define REG_IDX_SP 0
#define REG_IDX_AX 2
#define REG_IDX_DI 4 

//----- --------
// 32 bit
// | regs[0]: esp |
// | regs[1]: ebp |
// | regs[2]: eax |
// | regs[3]: ebx |
// | regs[4]: edi |
//
//-------------
// 64 bit
// | regs[0]: rsp |
// | regs[1]: rbp |
// | regs[2]: rax |
// | regs[3]: rbx |
// | regs[4]: rdi |
// | regs[5]: r12 |
// | regs[6]: r13 |
// | regs[7]: r14 |
// | regs[8]: r15 |


void rco::core::rco_make_context(Context* ctx,  Context::rco_func pfn, void* arg) {
	assert(ctx);

	if(!ctx->stack_ptr) {
		return;
	}

	std::memset(ctx->regs, 0, sizeof(ctx->regs));

	char* sp = nullptr;
#if defined(__i386__)
	sp = (char*)ctx->stack_ptr + ctx->stack_size - (sizeof(void*) << 1);
#elif defined(__x86_64__)
	sp = (char*)ctx->stack_ptr + ctx->stack_size;
#endif
	sp = (char*)((unsigned long)sp & -16LL);

	ctx->regs[REG_IDX_SP] = sp;

	void** ret = (void**)sp;

	*ret = (void*)pfn;
	
	ctx->regs[REG_IDX_AX] = *ret;
	ctx->regs[REG_IDX_DI] = (void*)arg;
}
