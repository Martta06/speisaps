CC      = gcc
CFLAGS  = -O3 -Wall -Wextra -std=c11 -Wpedantic
LDFLAGS = -lm

SRCS_CORE = params.c mars_geo.c terrain_cost.c edge_cost.c dem_reader.c geojson_export.c pathfinder_astar.c
OBJS_CORE = $(SRCS_CORE:.c=.o)

.PHONY: all test clean run

all: test_cost martian_pathfinder

test: test_cost
	./test_cost

test_cost: $(OBJS_CORE) test_cost.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

martian_pathfinder: $(OBJS_CORE) main_route.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

run: martian_pathfinder
	./martian_pathfinder

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o test_cost martian_pathfinder route_output.geojson
