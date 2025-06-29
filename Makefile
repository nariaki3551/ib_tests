# Compiler and flags
MPICC = mpicc
CFLAGS = -Wall -Wextra -O2 -g
MPI_LDFLAGS = -libverbs -lrdmacm

# Targets
PERF_TARGETS = ib_perf_multicast ib_perf_ud_unicast ib_perf_rc_unicast

# Sources
PERF_SOURCES = ib_perf_multicast.c ib_perf_ud_unicast.c ib_perf_rc_unicast.c

# Objects
PERF_OBJECTS = $(PERF_SOURCES:.c=.o)

# Default target
all: $(PERF_TARGETS)

# Individual targets
ib_perf_multicast: ib_perf_multicast.c
	$(MPICC) $(CFLAGS) -o $@ $< $(MPI_LDFLAGS)
	scp -P 12345 $@ snail01:/app/ib_tests/
	scp -P 12345 $@ snail02:/app/ib_tests/
	scp -P 12345 $@ snail03:/app/ib_tests/
	scp -P 12345 $@ tvm01:/app/ib_tests/
	scp -P 12345 $@ tvm02:/app/ib_tests/

ib_perf_ud_unicast: ib_perf_ud_unicast.c
	$(MPICC) $(CFLAGS) -o $@ $< $(MPI_LDFLAGS)
	scp -P 12345 $@ snail01:/app/ib_tests/
	scp -P 12345 $@ snail02:/app/ib_tests/
	scp -P 12345 $@ snail03:/app/ib_tests/
	scp -P 12345 $@ tvm01:/app/ib_tests/
	scp -P 12345 $@ tvm02:/app/ib_tests/

ib_perf_rc_unicast: ib_perf_rc_unicast.c
	$(MPICC) $(CFLAGS) -o $@ $< $(MPI_LDFLAGS)
	scp -P 12345 $@ snail01:/app/ib_tests/
	scp -P 12345 $@ snail02:/app/ib_tests/
	scp -P 12345 $@ snail03:/app/ib_tests/
	scp -P 12345 $@ tvm01:/app/ib_tests/
	scp -P 12345 $@ tvm02:/app/ib_tests/

test: test-mcast-host test-ud_unicast-host test-rc_unicast-host
perf: run-mcast-host run-ud_unicast-host run-rc_unicast-host

# Run performance test with specific parameters
run-mcast-host: ib_perf_multicast
	mpirun \
	: -n 1 --host snail01:1 $(FLAGS) -- /app/ib_tests/ib_perf_multicast -d mlx5_0 -w 20 -i 100 -m host \
	: -n 1 --host snail01:1 $(FLAGS) -- /app/ib_tests/ib_perf_multicast -d mlx5_1 -w 20 -i 100 -m host \
	: -n 1 --host snail02:1 $(FLAGS) -- /app/ib_tests/ib_perf_multicast -d mlx5_1 -w 20 -i 100 -m host \
	: -n 1 --host snail02:1 $(FLAGS) -- /app/ib_tests/ib_perf_multicast -d mlx5_2 -w 20 -i 100 -m host \
	: -n 1 --host snail03:1 $(FLAGS) -- /app/ib_tests/ib_perf_multicast -d mlx5_1 -w 20 -i 100 -m host \
	: -n 1 --host snail03:1 $(FLAGS) -- /app/ib_tests/ib_perf_multicast -d mlx5_2 -w 20 -i 100 -m host \
	: -n 1 --host tvm01:1   $(FLAGS) -- /app/ib_tests/ib_perf_multicast -d mlx5_1 -w 20 -i 100 -m host \
	: -n 1 --host tvm01:1   $(FLAGS) -- /app/ib_tests/ib_perf_multicast -d mlx5_2 -w 20 -i 100 -m host \
	: -n 1 --host tvm02:1   $(FLAGS) -- /app/ib_tests/ib_perf_multicast -d mlx5_1 -w 20 -i 100 -m host \
	: -n 1 --host tvm02:1   $(FLAGS) -- /app/ib_tests/ib_perf_multicast -d mlx5_2 -w 20 -i 100 -m host \

test-mcast-host: ib_perf_multicast
	mpirun \
	: -n 1 --host snail01:1 $(FLAGS) -x LOG_LEVEL=2 -- /app/ib_tests/ib_perf_multicast -d mlx5_0 -l 8192 -u 8192 -w 1 -i 2 -m host \
	: -n 1 --host snail01:1 $(FLAGS) -x LOG_LEVEL=2 -- /app/ib_tests/ib_perf_multicast -d mlx5_1 -l 8192 -u 8192 -w 1 -i 2 -m host \

# Run UD unicast performance test
run-ud_unicast-host: ib_perf_ud_unicast
	mpirun \
	: -n 1 --host snail01:1 $(FLAGS) -- /app/ib_tests/ib_perf_ud_unicast -d mlx5_0 -w 20 -i 100 -m host \
	: -n 1 --host snail02:1 $(FLAGS) -- /app/ib_tests/ib_perf_ud_unicast -d mlx5_1 -w 20 -i 100 -m host \

# Run UD unicast performance test with specific parameters
test-ud_unicast-host: ib_perf_ud_unicast
	mpirun -np 2 -x LOG_LEVEL=2 ./ib_perf_ud_unicast -d mlx5_0 -l 128 -u 128 -w 1 -i 2 -m host

# Run RC unicast performance test
run-rc_unicast-host: ib_perf_rc_unicast
	mpirun \
	: -n 1 --host snail01:1 $(FLAGS) -- /app/ib_tests/ib_perf_rc_unicast -d mlx5_0 -w 20 -i 100 -m host \
	: -n 1 --host snail02:1 $(FLAGS) -- /app/ib_tests/ib_perf_rc_unicast -d mlx5_1 -w 20 -i 100 -m host \

# Run RC unicast performance test with specific parameters
test-rc_unicast-host: ib_perf_rc_unicast
	mpirun -np 2 -x LOG_LEVEL=2 ./ib_perf_rc_unicast -d mlx5_0 -l 8192 -u 8192 -w 1 -i 2 -m host

# Help target
help:
	@echo "Available targets:"
	@echo "  all                       - Build all targets"
	@echo "  ib_perf_multicast         - Build multicast performance test"
	@echo "  ib_perf_ud_unicast        - Build UD unicast performance test"
	@echo "  ib_perf_rc_unicast        - Build RC unicast performance test"
	@echo "  run-mcast-host            - Run multicast performance test on multiple hosts"
	@echo "  run-mcast-host-quick      - Run quick multicast performance test"
	@echo "  run-ud_unicast-host       - Run UD unicast performance test"
	@echo "  run-ud_unicast-host-quick - Run quick UD unicast performance test"
	@echo "  run-rc_unicast-host       - Run RC unicast performance test"
	@echo "  run-rc_unicast-host-quick - Run quick RC unicast performance test"
	@echo "  clean                     - Clean build artifacts"
	@echo "  help                      - Show this help"

# Clean target
clean:
	rm -f $(PERF_OBJECTS) $(PERF_TARGETS)

# Flags for multi-host execution
FLAGS = --mca plm_rsh_args "-p 12345"

.PHONY: all clean perf test run-ud-unicast run-ud-unicast-quick run-rc-unicast run-rc-unicast-quick help
