.global context_swap
.text
.type context_swap, @funtion

context_swap:

#ifdef __i386__
mov     eax,DWORD PTR [esp+0x4]     // from_co
mov     edx,DWORD PTR [esp]         // retaddr
lea     ecx,[esp+0x4]               // esp
mov     DWORD PTR [eax+0x8],ebp     //<ebp
mov     DWORD PTR [eax+0x4],ecx     //<esp
mov     DWORD PTR [eax+0x0],edx     //<retaddr
mov     DWORD PTR [eax+0xc],edi     //<edi
mov     ecx,DWORD PTR [esp+0x8]     // to_co
mov     DWORD PTR [eax+0x10],esi    //<esi
mov     DWORD PTR [eax+0x14],ebx    //<ebx

mov     edx,DWORD PTR [ecx+0x4]     //>esp
mov     ebp,DWORD PTR [ecx+0x8]     //>ebp
mov     eax,DWORD PR [ecx+0x0]     //>retaddr
mov     edi,DWORD PTR [ecx+0xc]     //>edi
mov     esi,DWORD PTR [ecx+0x10]    //>esi
mov     ebx,DWORD PTR [ecx+0x14]    //>ebx

xor     ecx,ecx
mov     esp,edx
xor     edx,edx
jmp     eax
#elif __x86_64__

.align 16

/*
   0x00                  -->                  0xff
   r12 r13 r14 r15 rip rsp rbx rbp fpucw16 mxcsr32
   0   8   10  18  20  28  30  38  40      44
 */
// rdi - from | rsi - to
mov     rdx,QWORD PTR [rsp]      // retaddr
lea     rcx,[rsp+0x8]            // rsp

mov     QWORD PTR [rdi+0x0], r12
mov     QWORD PTR [rdi+0x8], r13
mov     QWORD PTR [rdi+0x10],r14
mov     QWORD PTR [rdi+0x18],r15
mov     QWORD PTR [rdi+0x20],rdx // retaddr
mov     QWORD PTR [rdi+0x28],rcx // rsp
mov     QWORD PTR [rdi+0x30],rbx
mov     QWORD PTR [rdi+0x38],rbp

mov     r12,QWORD PTR [rsi+0x0]
mov     r13,QWORD PTR [rsi+0x8]
mov     r14,QWORD PTR [rsi+0x10]
mov     r15,QWORD PTR [rsi+0x18]
mov     rax,QWORD PTR [rsi+0x20] // retaddr
mov     rcx,QWORD PTR [rsi+0x28] // rsp
mov     rbx,QWORD PTR [rsi+0x30]
mov     rbp,QWORD PTR [rsi+0x38]

mov     rsp,rcx
jmp     rax
#else
#error "platform not support"
#endifT
