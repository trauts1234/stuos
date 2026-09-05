#ifndef IDT_H
#define IDT_H

void setup_idt();
//returns a free IDT entry (HCF on failure)
int allocate_free_idt_entry();
// free_idt_entry must be a return value of allocate_free_idt_entry()
//
// will call handler(free_idt_entry); when the interrupt is fired
void initialise_idt_entry(int free_idt_entry, void (*handler)(int));

#endif