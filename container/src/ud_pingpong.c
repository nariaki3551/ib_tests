#include <infiniband/verbs.h>
#include <stdio.h>
#include <unistd.h>
#include <malloc.h>
#include <mpi.h>


enum {
    PINGPONG_RECV_WRID = 1,
    PINGPONG_SEND_WRID = 2,
};

struct pingpong_context {
    struct ibv_context		*context;
    struct ibv_pd			*pd;
    struct ibv_mr       	*mr;
    struct ibv_cq       	*cq;
    struct ibv_qp       	*qp;
    struct ibv_ah       	*ah;
    char                	*buf;
    int                     size;
    struct ibv_port_attr    portinfo;
};

struct pingpong_dest {
    int lid;
    int qpn;
};

static int pp_connect_ctx(struct pingpong_context *ctx, struct pingpong_dest *dest)
{
    struct ibv_ah_attr ah_attr = {
        .is_global     = 0,
        .dlid          = dest->lid,
        .sl            = 0,
        .src_path_bits = 0,
        .port_num      = 1
    };
    struct ibv_qp_attr attr = {
        .qp_state       = IBV_QPS_RTR
    };

    if (ibv_modify_qp(ctx->qp, &attr, IBV_QP_STATE)) {
        fprintf(stderr, "Failed to modify QP to RTR\n");
        return 1;
    }

    attr.qp_state   = IBV_QPS_RTS;
    attr.sq_psn     = 0;
    int flags       = IBV_QP_STATE | IBV_QP_SQ_PSN;

    if (ibv_modify_qp(ctx->qp, &attr, flags)) {
        fprintf(stderr, "Failed to modify QP to RTS\n");
        return 1;
    }

    ctx->ah = ibv_create_ah(ctx->pd, &ah_attr);
    if (!ctx->ah) {
        fprintf(stderr, "Failed to create AH\n");
        return 1;
    }

    return 0;
}

static struct pingpong_context *pp_init_ctx(struct ibv_device *ib_dev, int size)
{
    struct pingpong_context *ctx;

    ctx = malloc(sizeof *ctx);

	int page_size = sysconf(_SC_PAGESIZE);
    ctx->size = size;
    ctx->buf  = memalign(page_size, size + 40);
    if (!ctx->buf) {
        fprintf(stderr, "Couldn't allocate work buf.\n");
		return NULL;
    }

    /* FIXME memset(ctx->buf, 0, size + 40); */
    memset(ctx->buf, 0x7b, size + 40);

    ctx->context = ibv_open_device(ib_dev);
    if (!ctx->context) {
        fprintf(stderr, "Couldn't get context for %s\n", ibv_get_device_name(ib_dev));
		return NULL;
    }

    ctx->pd = ibv_alloc_pd(ctx->context);
    if (!ctx->pd) {
        fprintf(stderr, "Couldn't allocate PD\n");
		return NULL;
    }

    ctx->mr = ibv_reg_mr(ctx->pd, ctx->buf, size + 40, IBV_ACCESS_LOCAL_WRITE);
    if (!ctx->mr) {
        fprintf(stderr, "Couldn't register MR\n");
		return NULL;
    }

    // TODO
    ctx->cq = ibv_create_cq(ctx->context, 1 + 1, NULL, NULL, 0);
    if (!ctx->cq) {
        fprintf(stderr, "Couldn't create CQ\n");
		return NULL;
    }

    {
        struct ibv_qp_init_attr init_attr = {
            .send_cq = ctx->cq,
            .recv_cq = ctx->cq,
            .cap     = {
                .max_send_wr  = 1,
                .max_recv_wr  = 1,
                .max_send_sge = 1,
                .max_recv_sge = 1
            },
            .qp_type = IBV_QPT_UD,
        };

        ctx->qp = ibv_create_qp(ctx->pd, &init_attr);
        if (!ctx->qp)  {
            fprintf(stderr, "Couldn't create QP\n");
			return NULL;
        }
    }

    {
        struct ibv_qp_attr attr = {
            .qp_state        = IBV_QPS_INIT,
            .pkey_index      = 0,
            .port_num        = 1,
            .qkey            = 0x11111111
        };
        int flags =
            IBV_QP_STATE |
            IBV_QP_PKEY_INDEX |
            IBV_QP_PORT |
            IBV_QP_QKEY;

        if (ibv_modify_qp(ctx->qp, &attr, flags)) {
            fprintf(stderr, "Failed to modify QP to INIT\n");
        }
    }

    return ctx;
}

static int pp_close_ctx(struct pingpong_context *ctx)
{
    ibv_destroy_qp(ctx->qp);
    ibv_destroy_cq(ctx->cq);
    ibv_dereg_mr(ctx->mr);
    ibv_destroy_ah(ctx->ah);
    ibv_dealloc_pd(ctx->pd);
    ibv_close_device(ctx->context);
    free(ctx->buf);
    free(ctx);

    return 0;
}

static int pp_post_recv(struct pingpong_context *ctx)
{
    struct ibv_sge list = {
        .addr   = (uintptr_t) ctx->buf,
        .length = ctx->size + 40,
        .lkey   = ctx->mr->lkey
    };
    struct ibv_recv_wr wr = {
        .wr_id      = PINGPONG_RECV_WRID,
        .sg_list    = &list,
        .num_sge    = 1,
    };
    struct ibv_recv_wr *bad_wr;

    ibv_post_recv(ctx->qp, &wr, &bad_wr);

    return 1;
}

static int pp_post_send(struct pingpong_context *ctx, uint32_t qpn)
{
    struct ibv_sge list = {
        .addr   = (uintptr_t) ctx->buf + 40,
        .length = ctx->size,
        .lkey   = ctx->mr->lkey
    };
    struct ibv_send_wr wr = {
        .wr_id      = PINGPONG_SEND_WRID,
        .sg_list    = &list,
        .num_sge    = 1,
        .opcode     = IBV_WR_SEND,
        .send_flags = IBV_SEND_SIGNALED,
        .wr         = {
            .ud = {
                .ah          = ctx->ah,
                .remote_qpn  = qpn,
                .remote_qkey = 0x11111111
            }
        }
    };
    struct ibv_send_wr *bad_wr;

    return ibv_post_send(ctx->qp, &wr, &bad_wr);
}

int main(int argc, char *argv[]) {
    struct ibv_device      	**dev_list;
    struct ibv_device   	*ib_dev;
    struct pingpong_context *ctx;
    struct pingpong_dest    my_dest;
    struct pingpong_dest    *rem_dest;
    char                    *ib_devname = NULL;
    char                    *servername = NULL;
    unsigned int            size = 1024;
	int rank, world_size;

	MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    if (world_size != 2) {
		printf("This program requires exactly 2 processes.\n");
        MPI_Finalize();
        return 1;
    }

	if (rank == 1) {
		servername = "snail01";
	}

    if (servername) {
        ib_devname = "mlx5_2";
	} else {
        ib_devname = "mlx5_1";
	}

    dev_list = ibv_get_device_list(NULL);
    if (!dev_list) {
        perror("Failed to get IB devices list");
        return 1;
    }

    if (!ib_devname) {
        ib_dev = *dev_list;
        if (!ib_dev) {
            fprintf(stderr, "No IB devices found\n");
            return 1;
        }
    } else {
        int i;
        for (i = 0; dev_list[i]; ++i)
            if (!strcmp(ibv_get_device_name(dev_list[i]), ib_devname))
                break;
        ib_dev = dev_list[i];
        if (!ib_dev) {
            fprintf(stderr, "IB device %s not found\n", ib_devname);
            return 1;
        }
    }

    ctx = pp_init_ctx(ib_dev, size);
    if (!ctx)
        return 1;

    if (!servername) {
        pp_post_recv(ctx);
    }

	int port = 1;
	if (ibv_query_port(ctx->context, port, &ctx->portinfo)) {
        fprintf(stderr, "Couldn't get port info\n");
        return 1;
    }
    my_dest.lid = ctx->portinfo.lid;
    my_dest.qpn = ctx->qp->qp_num;

    printf("  local address:  LID %d, QPN %d\n", my_dest.lid, my_dest.qpn);

	rem_dest = malloc(sizeof(struct pingpong_dest));
    if (servername) {
        // Receiver
        MPI_Recv(rem_dest, sizeof(struct pingpong_dest), MPI_BYTE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Send(&my_dest, sizeof(struct pingpong_dest), MPI_BYTE, 0, 0, MPI_COMM_WORLD);
    } else {
        // Sender
        MPI_Send(&my_dest, sizeof(struct pingpong_dest), MPI_BYTE, 1, 0, MPI_COMM_WORLD);
        MPI_Recv(rem_dest, sizeof(struct pingpong_dest), MPI_BYTE, 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }

    printf("  remote address: LID %d, QPN %d\n", rem_dest->lid, rem_dest->qpn);

    if (pp_connect_ctx(ctx, rem_dest))
        return 1;

    if (servername) {
        if (pp_post_send(ctx, rem_dest->qpn)) {
            fprintf(stderr, "Couldn't post send\n");
            return 1;
        }
    }

    struct ibv_wc wc;
    int ne;

    do {
        ne = ibv_poll_cq(ctx->cq, 1, &wc);
        if (ne < 0) {
            fprintf(stderr, "poll CQ failed %d\n", ne);
            return 1;
        }
    } while (ne < 1);

    if (wc.status != IBV_WC_SUCCESS) {
            fprintf(stderr, "Failed status %s (%d) for wr_id %d\n",
                ibv_wc_status_str(wc.status),
                wc.status, (int) wc.wr_id);
            return 1;
        }
    
    if (servername) {
        printf("client: wc.wr_id: %ld\n", wc.wr_id);
    } else {
        printf("server: wc.wr_id: %ld\n", wc.wr_id);
    }

    if (pp_close_ctx(ctx))
        return 1;

    ibv_free_device_list(dev_list);
    free(rem_dest);
	MPI_Finalize();

    return 0;
}
