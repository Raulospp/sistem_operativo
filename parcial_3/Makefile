CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -g
SRC     = kernel.c memory.c processes.c files.c perifericos.c
OUT     = kernel

$(OUT): $(SRC) tipos.h
	$(CC) $(CFLAGS) -o $(OUT) $(SRC)

run: $(OUT)
	./$(OUT)

clean:
	rm -f $(OUT) $(OUT).exe estado.json
