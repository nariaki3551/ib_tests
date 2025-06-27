CC = gcc
MPICC = mpicc
CFLAGS = -Wall -Wextra -O2 -g
LDFLAGS = -libverbs -lrdmacm
MPI_LDFLAGS = -libverbs -lrdmacm

# Targets
PERF_TARGET = ib_multicast_perf

# Sources
PERF_SOURCES = ib_multicast_perf.c

# Objects
PERF_OBJECTS = $(PERF_SOURCES:.c=.o)

# Unicast performance test
ib_unicast_perf: ib_unicast_perf.c
	$(MPICC) $(CFLAGS) -o $@ $< $(LDFLAGS)

.PHONY: all clean perf test run run-perf

all: $(PERF_TARGET)

perf: $(PERF_TARGET)
	scp -P 12345 /app/ib_tests/$(PERF_TARGET) snail01:/app/ib_tests/
	scp -P 12345 /app/ib_tests/$(PERF_TARGET) snail02:/app/ib_tests/
	scp -P 12345 /app/ib_tests/$(PERF_TARGET) snail03:/app/ib_tests/
	scp -P 12345 /app/ib_tests/$(PERF_TARGET)   tvm01:/app/ib_tests/
	scp -P 12345 /app/ib_tests/$(PERF_TARGET)   tvm02:/app/ib_tests/

test-perf: $(PERF_TARGET)
	@echo "Running performance test..."
	mpirun -np 4 ./$(PERF_TARGET)

$(PERF_TARGET): $(PERF_OBJECTS)
	$(MPICC) $(PERF_OBJECTS) -o $(PERF_TARGET) $(MPI_LDFLAGS)

%.o: %.c
	$(MPICC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(PERF_OBJECTS) $(PERF_TARGET) ib_unicast_perf

FLAGS = --mca plm_rsh_args "-p 12345"

run-perf:
	mpirun \
	: -n 1 --host snail01:1 $(FLAGS) -x LOG_LEVEL=ERROR -- /app/ib_tests/$(PERF_TARGET) -d mlx5_0 -w 20 -i 100 \
	: -n 1 --host snail01:1 $(FLAGS) -x LOG_LEVEL=ERROR -- /app/ib_tests/$(PERF_TARGET) -d mlx5_1 -w 20 -i 100 \
	: -n 1 --host snail02:1 $(FLAGS) -x LOG_LEVEL=ERROR -- /app/ib_tests/$(PERF_TARGET) -d mlx5_1 -w 20 -i 100 \
	: -n 1 --host snail02:1 $(FLAGS) -x LOG_LEVEL=ERROR -- /app/ib_tests/$(PERF_TARGET) -d mlx5_2 -w 20 -i 100 \
	: -n 1 --host snail03:1 $(FLAGS) -x LOG_LEVEL=ERROR -- /app/ib_tests/$(PERF_TARGET) -d mlx5_1 -w 20 -i 100 \
	: -n 1 --host snail03:1 $(FLAGS) -x LOG_LEVEL=ERROR -- /app/ib_tests/$(PERF_TARGET) -d mlx5_2 -w 20 -i 100 \
	: -n 1 --host tvm01:1   $(FLAGS) -x LOG_LEVEL=ERROR -- /app/ib_tests/$(PERF_TARGET) -d mlx5_1 -w 20 -i 100 \
	: -n 1 --host tvm01:1   $(FLAGS) -x LOG_LEVEL=ERROR -- /app/ib_tests/$(PERF_TARGET) -d mlx5_2 -w 20 -i 100 \
	: -n 1 --host tvm02:1   $(FLAGS) -x LOG_LEVEL=ERROR -- /app/ib_tests/$(PERF_TARGET) -d mlx5_1 -w 20 -i 100 \
	: -n 1 --host tvm02:1   $(FLAGS) -x LOG_LEVEL=ERROR -- /app/ib_tests/$(PERF_TARGET) -d mlx5_2 -w 20 -i 100 \

run-perf-quick:
	# mpirun -np 2 $(PERF_TARGET) -d mlx5_0 -l 1048576 -u 1048576
	# mpirun -np 2 $(PERF_TARGET) -d mlx5_0 -l 1024 -u 1024
	mpirun \
	: -n 1 --host snail01:1 $(FLAGS) -x LOG_LEVEL=DEBUG -- /app/ib_tests/$(PERF_TARGET) -d mlx5_0 -l 1048576 -u 1048576 -w 1 -i 2 \
	: -n 1 --host snail01:1 $(FLAGS) -x LOG_LEVEL=DEBUG -- /app/ib_tests/$(PERF_TARGET) -d mlx5_1 -l 1048576 -u 1048576 -w 1 -i 2 \

# Help target
help:
	@echo "Available targets:"
	@echo "  all          - Build both test and performance programs"
	@echo "  perf         - Build performance test program"
	@echo "  test         - Run performance test (4 ranks)"
	@echo "  clean        - Remove build artifacts"
	@echo "  help         - Show this help message"
