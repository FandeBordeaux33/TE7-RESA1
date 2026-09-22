CC = cc
CFLAGS = -Wall -Werror -std=c99

%: %.c
    $(CC) $(CFLAGS) $< -o $@

.PHONY: clean
clean:
	rm -f client server

