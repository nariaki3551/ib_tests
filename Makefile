# Compiler and flags
MPICC = mpicc
CFLAGS = -Wall -Wextra -O2 -g
MPI_LDFLAGS = -libverbs -lrdmacm

# Targets
PERF_TARGETS = ib_multicast_perf ib_ud_unicast_perf ib_rc_unicast_perf

# Sources
PERF_SOURCES = ib_multicast_perf.c ib_ud_unicast_perf.c ib_rc_unicast_perf.c

# Objects
PERF_OBJECTS = $(PERF_SOURCES:.c=.o)

# Default target
all: $(PERF_TARGETS)

# Individual targets
ib_multicast_perf: ib_multicast_perf.c
	$(MPICC) $(CFLAGS) -o $@ $< $(MPI_LDFLAGS)
	scp -P 12345 $@ snail01:/app/ib_tests/
	scp -P 12345 $@ snail02:/app/ib_tests/
	scp -P 12345 $@ snail03:/app/ib_tests/
	scp -P 12345 $@ tvm01:/app/ib_tests/
	scp -P 12345 $@ tvm02:/app/ib_tests/

ib_ud_unicast_perf: ib_ud_unicast_perf.c
	$(MPICC) $(CFLAGS) -o $@ $< $(MPI_LDFLAGS)
	scp -P 12345 $@ snail01:/app/ib_tests/
	scp -P 12345 $@ snail02:/app/ib_tests/
	scp -P 12345 $@ snail03:/app/ib_tests/
	scp -P 12345 $@ tvm01:/app/ib_tests/
	scp -P 12345 $@ tvm02:/app/ib_tests/

ib_rc_unicast_perf: ib_rc_unicast_perf.c
	$(MPICC) $(CFLAGS) -o $@ $< $(MPI_LDFLAGS)
	scp -P 12345 $@ snail01:/app/ib_tests/
	scp -P 12345 $@ snail02:/app/ib_tests/
	scp -P 12345 $@ snail03:/app/ib_tests/
	scp -P 12345 $@ tvm01:/app/ib_tests/
	scp -P 12345 $@ tvm02:/app/ib_tests/

test: run-mcast-quick run-ud_unicast-quick run-rc_unicast-quick

# Run performance test with specific parameters
run-mcast: ib_multicast_perf
	mpirun \
	: -n 1 --host snail01:1 $(FLAGS) -x LOG_LEVEL=ERROR -- /app/ib_tests/ib_multicast_perf -d mlx5_0 -w 20 -i 100 \
	: -n 1 --host snail01:1 $(FLAGS) -x LOG_LEVEL=ERROR -- /app/ib_tests/ib_multicast_perf -d mlx5_1 -w 20 -i 100 \
	: -n 1 --host snail02:1 $(FLAGS) -x LOG_LEVEL=ERROR -- /app/ib_tests/ib_multicast_perf -d mlx5_1 -w 20 -i 100 \
	: -n 1 --host snail02:1 $(FLAGS) -x LOG_LEVEL=ERROR -- /app/ib_tests/ib_multicast_perf -d mlx5_2 -w 20 -i 100 \
	: -n 1 --host snail03:1 $(FLAGS) -x LOG_LEVEL=ERROR -- /app/ib_tests/ib_multicast_perf -d mlx5_1 -w 20 -i 100 \
	: -n 1 --host snail03:1 $(FLAGS) -x LOG_LEVEL=ERROR -- /app/ib_tests/ib_multicast_perf -d mlx5_2 -w 20 -i 100 \
	: -n 1 --host tvm01:1   $(FLAGS) -x LOG_LEVEL=ERROR -- /app/ib_tests/ib_multicast_perf -d mlx5_1 -w 20 -i 100 \
	: -n 1 --host tvm01:1   $(FLAGS) -x LOG_LEVEL=ERROR -- /app/ib_tests/ib_multicast_perf -d mlx5_2 -w 20 -i 100 \
	: -n 1 --host tvm02:1   $(FLAGS) -x LOG_LEVEL=ERROR -- /app/ib_tests/ib_multicast_perf -d mlx5_1 -w 20 -i 100 \
	: -n 1 --host tvm02:1   $(FLAGS) -x LOG_LEVEL=ERROR -- /app/ib_tests/ib_multicast_perf -d mlx5_2 -w 20 -i 100

run-mcast-quick: ib_multicast_perf
	# mpirun -np 2 ib_multicast_perf -d mlx5_0 -l 1048576 -u 1048576
	# mpirun -np 2 ib_multicast_perf -d mlx5_0 -l 1024 -u 1024
	mpirun \
	: -n 1 --host snail01:1 $(FLAGS) -x LOG_LEVEL=DEBUG -- /app/ib_tests/ib_multicast_perf -d mlx5_0 -l 1048576 -u 1048576 -w 1 -i 2 \
	: -n 1 --host snail01:1 $(FLAGS) -x LOG_LEVEL=DEBUG -- /app/ib_tests/ib_multicast_perf -d mlx5_1 -l 1048576 -u 1048576 -w 1 -i 2

# Run UD unicast performance test
run-ud_unicast: ib_ud_unicast_perf
	mpirun \
	: -n 1 --host snail01:1 $(FLAGS) -x LOG_LEVEL=ERROR -- /app/ib_tests/ib_ud_unicast_perf -d mlx5_0 -w 20 -i 100 \
	: -n 1 --host snail02:1 $(FLAGS) -x LOG_LEVEL=ERROR -- /app/ib_tests/ib_ud_unicast_perf -d mlx5_1 -w 20 -i 100 \

# Run UD unicast performance test with specific parameters
run-ud_unicast-quick: ib_ud_unicast_perf
	mpirun -np 2 -x LOG_LEVEL=DEBUG ./ib_ud_unicast_perf -d mlx5_0 -l 128 -u 128 -w 1 -i 2

# Run RC unicast performance test
run-rc_unicast: ib_rc_unicast_perf
	mpirun \
	: -n 1 --host snail01:1 $(FLAGS) -x LOG_LEVEL=ERROR -- /app/ib_tests/ib_rc_unicast_perf -d mlx5_0 -w 20 -i 100 \
	: -n 1 --host snail02:1 $(FLAGS) -x LOG_LEVEL=ERROR -- /app/ib_tests/ib_rc_unicast_perf -d mlx5_1 -w 20 -i 100 \

# Run RC unicast performance test with specific parameters
run-rc_unicast-quick: ib_rc_unicast_perf
	mpirun -np 2 -x LOG_LEVEL=DEBUG ./ib_rc_unicast_perf -d mlx5_0 -l 8192 -u 8192 -w 1 -i 2

# Help target
help:
	@echo "Available targets:"
	@echo "  all                - Build all targets"
	@echo "  ib_multicast_perf  - Build multicast performance test"
	@echo "  ib_ud_unicast_perf - Build UD unicast performance test"
	@echo "  ib_rc_unicast_perf - Build RC unicast performance test"
	@echo "  run-mcast          - Run multicast performance test on multiple hosts"
	@echo "  run-mcast-quick    - Run quick multicast performance test"
	@echo "  run-ud_unicast     - Run UD unicast performance test"
	@echo "  run-ud_unicast-quick - Run quick UD unicast performance test"
	@echo "  run-rc_unicast     - Run RC unicast performance test"
	@echo "  run-rc_unicast-quick - Run quick RC unicast performance test"
	@echo "  clean              - Clean build artifacts"
	@echo "  help               - Show this help"

# Clean target
clean:
	rm -f $(PERF_OBJECTS) $(PERF_TARGETS)

# Flags for multi-host execution
FLAGS = --mca plm_rsh_args "-p 12345"

.PHONY: all clean perf test run run-perf run-perf-quick run-ud-unicast run-ud-unicast-quick run-rc-unicast run-rc-unicast-quick help
