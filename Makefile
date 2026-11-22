CC = gcc

CFLAGS = -ffreestanding -fno-stack-protector \
         -fno-pic -fno-pie -mno-red-zone \
         -mcmodel=kernel -Wall -Wextra \
         -Iinclude

LDFLAGS = -nostdlib -static -no-pie \
          -Wl,-T,linker.ld \
          -Wl,--build-id=none

BUILD = build

all: $(BUILD)/kernel

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/kernel: kernel/main.c linker.ld | $(BUILD)
	$(CC) $(CFLAGS) $(LDFLAGS) kernel/main.c -o $(BUILD)/kernel

clean:
	rm -rf $(BUILD)

.PHONY: all clean
