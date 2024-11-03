fat32: fat32.c
	gcc fat32.c -o fat32

.PHONY: clean
clean:
	rm -f test* 
	rm fat32