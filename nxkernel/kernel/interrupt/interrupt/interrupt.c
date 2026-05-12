#include"include/inturrupt.h"

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
