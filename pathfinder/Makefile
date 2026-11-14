CC      = gcc
CFLAGS  = -O2 -Wall -Wextra -std=c11 -Wpedantic
LDFLAGS = -lm

SRCS = params.c mars_geo.c terrain_cost.c edge_cost.c
OBJS = $(SRCS:.c=.o)

.PHONY: all test clean

all: $(OBJS)

test: $(OBJS) test_cost.o
	$(CC) $(CFLAGS) $^ -o test_cost $(LDFLAGS)
	./test_cost

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o test_cost
