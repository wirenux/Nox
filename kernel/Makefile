# builds kernel/nox.elf

CC      := clang --target=x86_64-pc-none-elf
LD      := ld.lld
NASM    := nasm

BUILD   := ../build

CFLAGS  := \
    -ffreestanding      \
    -nostdlib           \
    -mno-red-zone       \
    -mcmodel=kernel     \
    -mno-mmx            \
    -mno-sse            \
    -mno-sse2           \
    -fno-stack-protector\
    -fno-pie            \
    -fno-pic            \
    -std=c11            \
    -Wall -Wextra       \
    -O2 -g              \
    -Iinclude           \
    -Ilib

NASMFLAGS := -f elf64 -g -F dwarf

LDFLAGS   := \
    -T linker.ld        \
    -nostdlib           \
    --no-dynamic-linker \
    -z max-page-size=0x1000

C_SRCS   := main.c lib/printf.c drivers/display.c drivers/serial.c arch/gdt.c arch/idt.c drivers/pic.c drivers/pit.c mm/pmm.c
ASM_SRCS := arch/boot.asm arch/gdt.asm arch/isr.asm

C_OBJS   := $(patsubst %.c,   $(BUILD)/%.c.o,   $(C_SRCS))
ASM_OBJS := $(patsubst %.asm, $(BUILD)/%.asm.o, $(ASM_SRCS))

KERNEL_ELF := $(BUILD)/nox.elf

.PHONY: all clean

all: $(KERNEL_ELF)

$(KERNEL_ELF): $(C_OBJS) $(ASM_OBJS)
	@mkdir -p $(dir $@)
	$(LD) $(LDFLAGS) $^ -o $@

$(BUILD)/%.c.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/%.asm.o: %.asm
	@mkdir -p $(dir $@)
	$(NASM) $(NASMFLAGS) $< -o $@

-include $(C_OBJS:.o=.d)

clean:
	rm -rf $(BUILD)
