# ib_tests
Test programs for InfiniBand

## build

```bash
# build docker image
docker build -t ib-test .

# run container
docker run -itd --rm --name ib-test --gpus all \
	--privileged \
	--device=/dev/infiniband \
	-v /sys/class/infiniband:/sys/class/infiniband \
	-v /dev/infiniband:/dev/infiniband \
	-v /etc/libibverbs.d:/etc/libibverbs.d \
	-v /etc/rdma:/etc/rdma \
	-v $$(pwd):/app \
	-w /app \
	ib-test
```

## Tests

- ud_pingpong
  - build: `make`
  - run: `make test_run ARGS=./bin/ud_pingpong`
