# === Docker Configuration ===
IMAGE_NAME = ib-test
IMAGE_TAG  = 1.0
CONTAINER  = ib-test

.PHONY: build_image run_container clean

all:
	make build_image
	make run_container

build_image:
	docker build -t $(IMAGE_NAME):$(IMAGE_TAG) .

run_container:
	docker run -itd --rm --name $(CONTAINER) --gpus all \
		--privileged \
		--device=/dev/infiniband \
		-v /sys/class/infiniband:/sys/class/infiniband \
		-v /dev/infiniband:/dev/infiniband \
		-v /etc/libibverbs.d:/etc/libibverbs.d \
		-v /etc/rdma:/etc/rdma \
		$(IMAGE_NAME):$(IMAGE_TAG)

clean:
	docker stop $(CONTAINER)
	docker rm $(CONTAINER)
