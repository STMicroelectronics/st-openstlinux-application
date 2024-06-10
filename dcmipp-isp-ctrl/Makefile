EXEC = dcmipp-isp-ctrl
SRC = $(wildcard *.c)
OBJ = $(SRC:.c=.o)

all: $(EXEC)

isp: $(OBJ)
	@$(CC) -o $@ $^ $(LDFLAGS) -lm

%.o: %.c
	@$(CC) -o $@ -c $< $(CFLAGS)

.PHONY: clean mrproper

clean:
	@rm -rf *.o

mrproper: clean
	@rm -rf $(EXEC)
