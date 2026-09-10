// sudo arm-none-eabi-gcc -march=armv7-a -marm -O0 -nostdlib -ffreestanding -c asmCode/code_c.c -o asmCode/code_c.o
// arm-none-eabi-objcopy -O binary asmCode/code.o asmCode/code.bin
// arm-none-eabi-objdump -s asmCode/code_c.o

typedef void (*SetDefault_Item_t)(void *item, int type, int stack);
#define SetDefault_Item ((SetDefault_Item_t)0x00390F48)

#define JUMP_TO(upper, lower) asm volatile ("mov r12, #" #lower "\n\tmovt r12, #" #upper "\n\tbx r12\n\t" ::: "r12"); __builtin_unreachable();

__attribute__((noreturn, naked)) void CodeCave(void){

	register void *item asm("r0");

	register SetDefault_Item_t fn asm("r12") = SetDefault_Item;

	fn(item, 0xA9, 1, 0);

    JUMP_TO(0x003d, 0x96bc)
}
