# Compiler and flags
MPICC = mpicc
CFLAGS = -Wall -Wextra -O2 -g
MPI_LDFLAGS = -libverbs -lrdmacm
CUDA_LDFLAGS = -lcuda -lcudart -L/usr/local/cuda/lib64 -I/usr/local/cuda/include

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
	$(MPICC) $(CFLAGS) -o $@ $< $(MPI_LDFLAGS) $(CUDA_LDFLAGS)
	scp -P 12345 $@ snail01:/app/ib_tests/
	scp -P 12345 $@ snail02:/app/ib_tests/
	scp -P 12345 $@ snail03:/app/ib_tests/
	scp -P 12345 $@ tvm01:/app/ib_tests/
	scp -P 12345 $@ tvm02:/app/ib_tests/

ib_perf_ud_unicast: ib_perf_ud_unicast.c
	$(MPICC) $(CFLAGS) -o $@ $< $(MPI_LDFLAGS) $(CUDA_LDFLAGS)
	scp -P 12345 $@ snail01:/app/ib_tests/
	scp -P 12345 $@ snail02:/app/ib_tests/
	scp -P 12345 $@ snail03:/app/ib_tests/
	scp -P 12345 $@ tvm01:/app/ib_tests/
	scp -P 12345 $@ tvm02:/app/ib_tests/

ib_perf_rc_unicast: ib_perf_rc_unicast.c
	$(MPICC) $(CFLAGS) -o $@ $< $(MPI_LDFLAGS) $(CUDA_LDFLAGS)
	scp -P 12345 $@ snail01:/app/ib_tests/
	scp -P 12345 $@ snail02:/app/ib_tests/
	scp -P 12345 $@ snail03:/app/ib_tests/
	scp -P 12345 $@ tvm01:/app/ib_tests/
	scp -P 12345 $@ tvm02:/app/ib_tests/

test-host: test-mcast-host test-ud_unicast-host test-rc_unicast-host
test-cuda: test-mcast-cuda test-ud_unicast-cuda test-rc_unicast-cuda
perf-host: run-mcast-host run-ud_unicast-host run-rc_unicast-host
perf-cuda: run-mcast-cuda run-ud_unicast-cuda run-rc_unicast-cuda

# Run performance test with specific parameters
# ------------------------------------------------------------------------------------------------------------------------------------
# Multicast performance test
# ------------------------------------------------------------------------------------------------------------------------------------
run-mcast-host: ib_perf_multicast
	mpirun \
	: -n 1 --host snail01:1 $(FLAGS) -- /app/ib_tests/ib_perf_multicast -d mlx5_0 -w 20 -i 100 -m host -l 1024 -u 33554432 \
	: -n 1 --host snail01:1 $(FLAGS) -- /app/ib_tests/ib_perf_multicast -d mlx5_1 -w 20 -i 100 -m host -l 1024 -u 33554432 \
	: -n 1 --host snail02:1 $(FLAGS) -- /app/ib_tests/ib_perf_multicast -d mlx5_1 -w 20 -i 100 -m host -l 1024 -u 33554432 \
	: -n 1 --host snail02:1 $(FLAGS) -- /app/ib_tests/ib_perf_multicast -d mlx5_2 -w 20 -i 100 -m host -l 1024 -u 33554432 \
	: -n 1 --host snail03:1 $(FLAGS) -- /app/ib_tests/ib_perf_multicast -d mlx5_1 -w 20 -i 100 -m host -l 1024 -u 33554432 \
	: -n 1 --host snail03:1 $(FLAGS) -- /app/ib_tests/ib_perf_multicast -d mlx5_2 -w 20 -i 100 -m host -l 1024 -u 33554432 \
	: -n 1 --host tvm01:1   $(FLAGS) -- /app/ib_tests/ib_perf_multicast -d mlx5_1 -w 20 -i 100 -m host -l 1024 -u 33554432 \
	: -n 1 --host tvm01:1   $(FLAGS) -- /app/ib_tests/ib_perf_multicast -d mlx5_2 -w 20 -i 100 -m host -l 1024 -u 33554432 \
	: -n 1 --host tvm02:1   $(FLAGS) -- /app/ib_tests/ib_perf_multicast -d mlx5_1 -w 20 -i 100 -m host -l 1024 -u 33554432 \
	: -n 1 --host tvm02:1   $(FLAGS) -- /app/ib_tests/ib_perf_multicast -d mlx5_2 -w 20 -i 100 -m host -l 1024 -u 33554432 \

run-mcast-cuda: ib_perf_multicast
	mpirun \
	: -n 1 --host snail01:1 $(FLAGS) -- /app/ib_tests/ib_perf_multicast -d mlx5_0 -w 20 -i 100 -m cuda -g 2 -l 1024 -u 33554432 \
	: -n 1 --host snail01:1 $(FLAGS) -- /app/ib_tests/ib_perf_multicast -d mlx5_1 -w 20 -i 100 -m cuda -g 3 -l 1024 -u 33554432 \
	: -n 1 --host snail02:1 $(FLAGS) -- /app/ib_tests/ib_perf_multicast -d mlx5_1 -w 20 -i 100 -m cuda -g 0 -l 1024 -u 33554432 \
	: -n 1 --host snail02:1 $(FLAGS) -- /app/ib_tests/ib_perf_multicast -d mlx5_2 -w 20 -i 100 -m cuda -g 1 -l 1024 -u 33554432 \
	: -n 1 --host snail03:1 $(FLAGS) -- /app/ib_tests/ib_perf_multicast -d mlx5_1 -w 20 -i 100 -m cuda -g 0 -l 1024 -u 33554432 \
	: -n 1 --host snail03:1 $(FLAGS) -- /app/ib_tests/ib_perf_multicast -d mlx5_2 -w 20 -i 100 -m cuda -g 1 -l 1024 -u 33554432 \
	: -n 1 --host tvm01:1   $(FLAGS) -- /app/ib_tests/ib_perf_multicast -d mlx5_1 -w 20 -i 100 -m cuda -g 0 -l 1024 -u 33554432 \
	: -n 1 --host tvm01:1   $(FLAGS) -- /app/ib_tests/ib_perf_multicast -d mlx5_2 -w 20 -i 100 -m cuda -g 1 -l 1024 -u 33554432 \
	: -n 1 --host tvm02:1   $(FLAGS) -- /app/ib_tests/ib_perf_multicast -d mlx5_1 -w 20 -i 100 -m cuda -g 0 -l 1024 -u 33554432 \
	: -n 1 --host tvm02:1   $(FLAGS) -- /app/ib_tests/ib_perf_multicast -d mlx5_2 -w 20 -i 100 -m cuda -g 1 -l 1024 -u 33554432 \

test-mcast-host: ib_perf_multicast
	mpirun \
	: -n 1 --host snail01:1 $(FLAGS) -x LOG_LEVEL=2 -- /app/ib_tests/ib_perf_multicast -d mlx5_0 -l 8192 -u 8192 -w 1 -i 2 -m host \
	: -n 1 --host snail01:1 $(FLAGS) -x LOG_LEVEL=2 -- /app/ib_tests/ib_perf_multicast -d mlx5_1 -l 8192 -u 8192 -w 1 -i 2 -m host \

test-mcast-cuda: ib_perf_multicast
	mpirun \
	: -n 1 --host snail01:1 $(FLAGS) -x LOG_LEVEL=2 -- /app/ib_tests/ib_perf_multicast -d mlx5_0 -l 8192 -u 8192 -w 1 -i 2 -m cuda -g 2 \
	: -n 1 --host snail01:1 $(FLAGS) -x LOG_LEVEL=2 -- /app/ib_tests/ib_perf_multicast -d mlx5_1 -l 8192 -u 8192 -w 1 -i 2 -m cuda -g 3 \

test-mcast-cuda-perf: ib_perf_multicast
	mpirun \
	: -n 1 --host snail01:1 $(FLAGS) -x LOG_LEVEL=2 -- perf record -g /app/ib_tests/ib_perf_multicast -d mlx5_0 -l 8192 -u 8192 -w 1 -i 2 -m cuda -g 2 \
	: -n 1 --host snail02:1 $(FLAGS) -x LOG_LEVEL=2 -- /app/ib_tests/ib_perf_multicast -d mlx5_0 -l 8192 -u 8192 -w 1 -i 0 -m cuda -g 3 \


# ------------------------------------------------------------------------------------------------------------------------------------
# UD unicast performance test
# ------------------------------------------------------------------------------------------------------------------------------------
run-ud_unicast-host: ib_perf_ud_unicast
	mpirun \
	: -n 1 --host snail01:1 $(FLAGS) -- /app/ib_tests/ib_perf_ud_unicast -d mlx5_0 -w 20 -i 100 -m host -l 1024 -u 33554432 \
	: -n 1 --host snail02:1 $(FLAGS) -- /app/ib_tests/ib_perf_ud_unicast -d mlx5_1 -w 20 -i 100 -m host -l 1024 -u 33554432 \

run-ud_unicast-cuda: ib_perf_ud_unicast
	mpirun \
	: -n 1 --host snail01:1 $(FLAGS) -- /app/ib_tests/ib_perf_ud_unicast -d mlx5_0 -w 20 -i 100 -m cuda -g 2 -l 1024 -u 8192 \
	: -n 1 --host snail02:1 $(FLAGS) -- /app/ib_tests/ib_perf_ud_unicast -d mlx5_1 -w 20 -i 100 -m cuda -g 0 -l 1024 -u 8192 \

test-ud_unicast-host: ib_perf_ud_unicast
	mpirun \
	: -n 1 --host snail01:1 $(FLAGS) -x LOG_LEVEL=2 -- /app/ib_tests/ib_perf_ud_unicast -d mlx5_0 -l 128 -u 128 -w 1 -i 2 -m host \
	: -n 1 --host snail01:1 $(FLAGS) -x LOG_LEVEL=2 -- /app/ib_tests/ib_perf_ud_unicast -d mlx5_1 -l 128 -u 128 -w 1 -i 2 -m host \

test-ud_unicast-cuda: ib_perf_ud_unicast
	mpirun \
	: -n 1 --host snail01:1 $(FLAGS) -x LOG_LEVEL=2 -- /app/ib_tests/ib_perf_ud_unicast -d mlx5_0 -l 128 -u 128 -w 1 -i 2 -m cuda -g 2 \
	: -n 1 --host snail01:1 $(FLAGS) -x LOG_LEVEL=2 -- /app/ib_tests/ib_perf_ud_unicast -d mlx5_1 -l 128 -u 128 -w 1 -i 2 -m cuda -g 3 \

# ------------------------------------------------------------------------------------------------------------------------------------
# RC unicast performance test
# ------------------------------------------------------------------------------------------------------------------------------------
run-rc_unicast-host: ib_perf_rc_unicast
	mpirun \
	: -n 1 --host snail01:1 $(FLAGS) -- /app/ib_tests/ib_perf_rc_unicast -d mlx5_0 -w 20 -i 100 -m host -l 1024 -u 33554432 \
	: -n 1 --host snail02:1 $(FLAGS) -- /app/ib_tests/ib_perf_rc_unicast -d mlx5_1 -w 20 -i 100 -m host -l 1024 -u 33554432 \

run-rc_unicast-cuda: ib_perf_rc_unicast
	mpirun \
	: -n 1 --host snail01:1 $(FLAGS) -- /app/ib_tests/ib_perf_rc_unicast -d mlx5_0 -w 20 -i 100 -m cuda -g 2 -l 1024 -u 33554432 \
	: -n 1 --host snail02:1 $(FLAGS) -- /app/ib_tests/ib_perf_rc_unicast -d mlx5_1 -w 20 -i 100 -m cuda -g 0 -l 1024 -u 33554432 \

test-rc_unicast-host: ib_perf_rc_unicast
	mpirun \
	: -n 1 --host snail01:1 $(FLAGS) -x LOG_LEVEL=2 -- /app/ib_tests/ib_perf_rc_unicast -d mlx5_0 -l 8192 -u 8192 -w 1 -i 2 -m host \
	: -n 1 --host snail02:1 $(FLAGS) -x LOG_LEVEL=2 -- /app/ib_tests/ib_perf_rc_unicast -d mlx5_1 -l 8192 -u 8192 -w 1 -i 2 -m host \

test-rc_unicast-cuda: ib_perf_rc_unicast
	mpirun \
	: -n 1 --host snail01:1 $(FLAGS) -x LOG_LEVEL=2 -- /app/ib_tests/ib_perf_rc_unicast -d mlx5_0 -l 8192 -u 8192 -w 1 -i 2 -m cuda -g 2 \
	: -n 1 --host snail01:1 $(FLAGS) -x LOG_LEVEL=2 -- /app/ib_tests/ib_perf_rc_unicast -d mlx5_1 -l 8192 -u 8192 -w 1 -i 2 -m cuda -g 3 \

# ------------------------------------------------------------------------------------------------------------------------------------
# Help target
# ------------------------------------------------------------------------------------------------------------------------------------
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

.PHONY: all clean perf-host perf-cuda test-host test-cuda help
