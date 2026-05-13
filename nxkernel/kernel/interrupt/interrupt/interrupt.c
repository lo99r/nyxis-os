#include"include/inturrupt.h"

static interrupt_error_frame64_t* current_stack;
static interrupt_error_frame64_ring_t* current_stack_user;

struct idtr init_reg;
//struct idt_entry idt[256];

//

void idt_set_gate(
	i32 vec,
	void* handler,
	struct idt_entry* idt,
	u8 flags,
	u8 ist
)
{
	idt[vec].ist         = ist                                      ;
	idt[vec].offset_low  = (u16)(handler & 0xFFFF)                  ;
	idt[vec].offset_mid  = (u16)(handler & 0xFFFF0000) >> 16        ;
	idt[vec].offset_high = (u16)(handler & 0xFFFFFFFF00000000) >> 32;
	idt[vec].selector    = 0x08                                     ;
	idt[vec].type_attr   = flags                                    ;
}
void idt_init(u16 limit, u64 base){
	__asm__ volatile (
		"lidt %0"
		:
		: "m"(init_reg)
	);
}
/*;void* idt_get_entry(){
	return idt;
}*/

static struct status find_current_status(){
	u8 return_type = 0;
	__asm__ volatile (
		"mov %%rsp, %0"
		: "=m"(current_stack)
	);
	if (current_stack->cs&0x03 == 0x03) {
		return_type = 1;
		current_stack_user = current_stack;
	}
	return {return_type, current_stack};
}
