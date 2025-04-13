.PHONY: all test_run clean

all:
	make -C ./src

test_run:
	@if [ -z "$(ARGS)" ]; then \
		echo "Usage: make test_run ARGS=./bin/your_binary"; \
	else \
		mpirun -n 2 --allow-run-as-root $(ARGS); \
	fi

clean:
	make -C ./src clean
