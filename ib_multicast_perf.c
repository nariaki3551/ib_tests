#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <fcntl.h>
#include <infiniband/verbs.h>
#include <rdma/rdma_cma.h>
#include <mpi.h>
#include <dirent.h>
#include <time.h>
#include <sys/time.h>

#define BUFFER_SIZE 1073741824
#define MAX_QP_WR 16384
#define DEF_QKEY 0x1a1a1a1a
#define GRH_HEADER_SIZE 40  // Global Routing Header size
#define FAKE_MCAST_ADDR "0000::0002"

// Multicast information structure
typedef struct {
    union ibv_gid dgid;        // Actual multicast GID
    uint16_t mlid;             // MLID
    char addr_str[INET6_ADDRSTRLEN]; // String address
} mcast_info_t;

// IB context structure
typedef struct {
    struct ibv_context *dev;
    struct ibv_pd *pd;
    struct ibv_cq *cq;
    struct ibv_qp *qp;
    struct ibv_ah *ah;
    struct ibv_mr *mr;
    struct ibv_mr *grh_buf_mr;
    void *buf;
    void *grh_buf;
    int ib_port;
    int mtu;
    uint16_t lid;
    union ibv_gid gid;
    char *devname;
    struct ibv_send_wr *send_wrs;
    struct ibv_recv_wr *recv_wrs;
    struct ibv_sge *send_sges;
    struct ibv_sge *recv_sges;
} ib_context_t;

// performance parameters structure
typedef struct {
    int warmup_iterations;
    int test_iterations;
    int size_step;
    int min_size;
    int max_size;
} perf_params_t;

// performance result structure
typedef struct {
    double send_time_usec;
    double recv_time_usec;
} perf_result_t;

// Global variables
static struct rdma_cm_id *mcast_cm_id = NULL;
static struct rdma_cm_event *event = NULL;
static int mpi_rank;
static int mpi_size;
static const char TEST_DATA_VALUE = 3;

// Log level definition
typedef enum {
    LOG_LEVEL_ERROR = 0,
    LOG_LEVEL_INFO = 1,
    LOG_LEVEL_DEBUG = 2,
} log_level_t;
static log_level_t g_log_level = LOG_LEVEL_INFO;

#define LOG_ERROR(fmt, ...) \
    do { if (g_log_level >= LOG_LEVEL_ERROR) fprintf(stderr, "\033[31mRank %d: " fmt " [%s:%s:%d]\033[0m\n", mpi_rank, ##__VA_ARGS__, __FILE__, __func__, __LINE__); } while (0)
#define LOG_INFO(fmt, ...) \
    do { if (g_log_level >= LOG_LEVEL_INFO) fprintf(stderr, "\033[36mRank %d: " fmt " [%s:%s:%d]\033[0m\n", mpi_rank, ##__VA_ARGS__, __FILE__,__func__, __LINE__); } while (0)
#define LOG_DEBUG(fmt, ...) \
    do { if (g_log_level >= LOG_LEVEL_DEBUG) fprintf(stderr, "Rank %d: " fmt " [%s:%s:%d]\n", mpi_rank, ##__VA_ARGS__, __FILE__,__func__, __LINE__); } while (0)

// Performance measurement functions
static double get_time_usec(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec * 1000000.0 + (double)tv.tv_usec;
}

// Forward declarations for functions used in run_performance_test
static int post_recv(ib_context_t *ctx, int len, int num_chunks);
static int post_send(ib_context_t *ctx, int len, int num_chunks);
static int wait_for_completion(ib_context_t *ctx, int expected_wrs);
static int verify_received_data(ib_context_t *ctx, int size);

static void print_performance_header(perf_params_t *perf_params)
{
    if (mpi_rank == 0) {
        printf("\n");
        printf("IB Multicast Performance Test\n");
        printf("=============================\n");
        printf("Warmup iterations: %d\n", perf_params->warmup_iterations);
        printf("Test iterations: %d\n", perf_params->test_iterations);
        printf("MPI ranks: %d\n", mpi_size);
        printf("\n");
        printf("%-20s %-16s %-16s %-16s %-16s\n", 
               "Size (bytes)", "Send BW (GB/s)", "Recv BW (GB/s)", "Send Lat (us)", "Recv Lat (us)");
        printf("-----------------------------------------------------------------------------------\n");
    }
}

static void print_performance_result(int size, double send_bandwidth_gbps, double recv_bandwidth_gbps, 
                                   double send_latency_usec, double recv_latency_usec)
{
    if (mpi_rank == 0) {
        char size_str[32];
        if (size < 1024) {
            snprintf(size_str, sizeof(size_str), "%d", size);
        } else if (size < 1024 * 1024) {
            snprintf(size_str, sizeof(size_str), "%d (%.0fK)", size, (double)size / 1024);
        } else if (size < 1024 * 1024 * 1024) {
            snprintf(size_str, sizeof(size_str), "%d (%.0fM)", size, (double)size / (1024 * 1024));
        } else {
            snprintf(size_str, sizeof(size_str), "%d (%.0fG)", size, (double)size / (1024 * 1024 * 1024));
        }
        
        printf("%-20s %-16.4f %-16.4f %-16.2f %-16.2f\n", 
               size_str, send_bandwidth_gbps, recv_bandwidth_gbps, send_latency_usec, recv_latency_usec);
    }
}

static int allocate_wrs_and_sges(ib_context_t *ctx, perf_params_t *perf_params)
{
    int chunk_size = ctx->mtu - GRH_HEADER_SIZE;
    int max_num_chunks = (perf_params->max_size + chunk_size - 1) / chunk_size;  // ceil(size / chunk_size)
    
    ctx->send_wrs = malloc(max_num_chunks * sizeof(struct ibv_send_wr));
    ctx->recv_wrs = malloc(max_num_chunks * sizeof(struct ibv_recv_wr));
    ctx->send_sges = malloc(max_num_chunks * sizeof(struct ibv_sge));
    ctx->recv_sges = malloc(max_num_chunks * 2 * sizeof(struct ibv_sge));
    
    if (!ctx->send_wrs || !ctx->recv_wrs || !ctx->send_sges || !ctx->recv_sges) {
        LOG_ERROR("Failed to allocate work requests and scatter-gather elements");
        return -1;
    }
    
    // Initialize buffer with test data (only for sender rank)
    if (mpi_rank == 0) {
        memset(ctx->buf, TEST_DATA_VALUE, perf_params->max_size);
    }
    
    LOG_DEBUG("Allocated WRs and SGEs for max %d chunks", max_num_chunks);
    return 0;
}

static perf_result_t run_performance_test(ib_context_t *ctx, int size, perf_params_t *perf_params)
{
    double start_time = 0.0, end_send_time = 0.0, end_recv_time = 0.0, total_send_time = 0.0, total_recv_time = 0.0;
    int chunk_size = ctx->mtu - GRH_HEADER_SIZE;
    int num_chunks = (size + chunk_size - 1) / chunk_size;  // ceil(size / chunk_size)
    perf_result_t result = {0.0, 0.0};

    // Warmup phase
    LOG_INFO("Warmup phase");
    for (int i = 0; i < perf_params->warmup_iterations; i++) {
        LOG_INFO("Warmup iteration start [%d/%d]", i+1, perf_params->warmup_iterations);
        if (mpi_rank > 0) {
            post_recv(ctx, size, num_chunks);
        }
        MPI_Barrier(MPI_COMM_WORLD);

        if (mpi_rank == 0) {
            post_send(ctx, size, num_chunks);
            wait_for_completion(ctx, 1);
        } else {
            wait_for_completion(ctx, num_chunks);
            verify_received_data(ctx, size);
            memset(ctx->buf, 0, size);
        }
    }

    // Measurement phase
    LOG_INFO("Measurement phase");
    for (int i = 0; i < perf_params->test_iterations; i++) {
        LOG_INFO("Measurement iteration start [%d/%d]", i+1, perf_params->test_iterations);
        if (mpi_rank > 0) {
            post_recv(ctx, size, num_chunks);
        }
        MPI_Barrier(MPI_COMM_WORLD);
        start_time = get_time_usec();
       
        if (mpi_rank == 0) {
            post_send(ctx, size, num_chunks);
            wait_for_completion(ctx, 1);
            end_send_time = get_time_usec();
        } else {
            wait_for_completion(ctx, num_chunks);
        }
        MPI_Barrier(MPI_COMM_WORLD);
        end_recv_time = get_time_usec();
        total_send_time += end_send_time - start_time;
        total_recv_time += end_recv_time - start_time;
    }

    result.send_time_usec = total_send_time / perf_params->test_iterations;
    result.recv_time_usec = total_recv_time / perf_params->test_iterations;
    return result;
}

static void run_performance_suite(ib_context_t *ctx, perf_params_t *perf_params)
{
    int size;
    double send_bandwidth_gbps, recv_bandwidth_gbps, send_latency_usec, recv_latency_usec;
    
    print_performance_header(perf_params);
    
    for (size = perf_params->min_size; size <= perf_params->max_size; size *= perf_params->size_step) {
        perf_result_t result = run_performance_test(ctx, size, perf_params);
        
        if (mpi_rank == 0 && (result.send_time_usec < 0 || result.recv_time_usec < 0)) {
            LOG_ERROR("Performance test failed for size %d", size);
            continue;
        }
        
        // Calculate performance metrics
        double double_size = (double)size;
        send_latency_usec = result.send_time_usec;
        recv_latency_usec = result.recv_time_usec;
        send_bandwidth_gbps = (double_size * 1000000) / (result.send_time_usec * 1024 * 1024 * 1024);
        recv_bandwidth_gbps = (double_size * 1000000) / (result.recv_time_usec * 1024 * 1024 * 1024);
        
        print_performance_result(size, send_bandwidth_gbps, recv_bandwidth_gbps, send_latency_usec, recv_latency_usec);
    }
    
    if (mpi_rank == 0) {
        printf("-----------------------------------------------------------------------------------\n");
        printf("Performance test completed\n");
    }
}

static void print_usage(const char *prog)
{
    printf("Usage: %s [options]\n", prog);
    printf("Options:\n");
    printf("  -d <device>      IB device name (default: first available)\n");
    printf("  -l <min_size>    Minimum message size in bytes (default: 1024)\n");
    printf("  -u <max_size>    Maximum message size in bytes (default: 1073741824)\n");
    printf("  -w <warmup>      Number of warmup iterations (default: 10)\n");
    printf("  -i <iterations>  Number of test iterations (default: 100)\n");
    printf("  -s <step>        Size step multiplier (default: 2)\n");
    printf("  -h               Show this help\n");
    printf("\nEnvironment Variables:\n");
    printf("  LOG_LEVEL        Set logging level (ERROR/0, INFO/1, DEBUG/2, default: INFO)\n");
    printf("\nNote: This program requires exactly 2 MPI ranks (sender and receiver)\n");
}

static int get_ib_device(const char *dev_name, struct ibv_device **dev)
{
    struct ibv_device **device_list;
    int num_devices, i;

    device_list = ibv_get_device_list(&num_devices);
    if (!device_list || !num_devices) {
        LOG_ERROR("No IB devices available");
        return -1;
    }

    if (!dev_name) {
        *dev = device_list[0];
        LOG_INFO("Using device: %s", ibv_get_device_name(*dev));
    } else {
        for (i = 0; device_list[i]; ++i) {
            if (!strcmp(ibv_get_device_name(device_list[i]), dev_name)) {
                *dev = device_list[i];
                LOG_INFO("Using device: %s", ibv_get_device_name(*dev));
                break;
            }
        }
        if (!device_list[i]) {
            LOG_ERROR("IB device %s not found", dev_name);
            ibv_free_device_list(device_list);
            return -1;
        }
    }

    return 0;
}

static int init_ib_context(ib_context_t *ctx, const char *dev_name)
{
    struct ibv_device *dev;
    struct ibv_port_attr port_attr;
    struct ibv_device_attr device_attr;
    int ret;

    ret = get_ib_device(dev_name, &dev);
    if (ret < 0) {
        LOG_ERROR("Failed to get IB device");
        return ret;
    }

    ctx->dev = ibv_open_device(dev);
    if (!ctx->dev) {
        LOG_ERROR("Failed to open IB device");
        return -1;
    }

    ctx->ib_port = 1; // Default to port 1

    ret = ibv_query_port(ctx->dev, ctx->ib_port, &port_attr);
    if (ret < 0) {
        LOG_ERROR("Failed to query port");
        goto error;
    }

    if (port_attr.state != IBV_PORT_ACTIVE) {
        LOG_ERROR("IB port is not active");
        ret = -1;
        goto error;
    }

    ret = ibv_query_device(ctx->dev, &device_attr);
    if (ret < 0) {
        LOG_ERROR("Failed to query device");
        goto error;
    }

    LOG_DEBUG("Device capabilities:");
    LOG_DEBUG("        max_qp_wr: %d", device_attr.max_qp_wr);
    LOG_DEBUG("        max_cqe: %d", device_attr.max_cqe);
    LOG_DEBUG("        max_mr: %d", device_attr.max_mr);
    LOG_DEBUG("        max_pd: %d", device_attr.max_pd);

    int mtu_enum = port_attr.active_mtu;
    switch (mtu_enum) {
        case 1: ctx->mtu = 256; break;   // IBV_MTU_256
        case 2: ctx->mtu = 512; break;   // IBV_MTU_512
        case 3: ctx->mtu = 1024; break;  // IBV_MTU_1024
        case 4: ctx->mtu = 2048; break;  // IBV_MTU_2048
        case 5: ctx->mtu = 4096; break;  // IBV_MTU_4096
        default: ctx->mtu = 1024; break; // Default
    }

    // Set Lid
    ctx->lid = port_attr.lid;

    // Get GID
    ret = ibv_query_gid(ctx->dev, ctx->ib_port, 0, &ctx->gid);
    if (ret < 0) {
        LOG_ERROR("Failed to query GID");
        goto error;
    }

    // Store device name
    ctx->devname = strdup(ibv_get_device_name(dev));
    if (!ctx->devname) {
        LOG_ERROR("Failed to allocate device name");
        goto error;
    }

    ctx->pd = ibv_alloc_pd(ctx->dev);
    if (!ctx->pd) {
        LOG_ERROR("Failed to allocate PD");
        goto error;
    }

    ctx->cq = ibv_create_cq(ctx->dev, MAX_QP_WR, NULL, NULL, 0);
    if (!ctx->cq) {
        LOG_ERROR("Failed to create CQ");
        goto error;
    }

    ctx->buf = malloc(BUFFER_SIZE);
    if (!ctx->buf) {
        LOG_ERROR("Failed to allocate buffer");
        goto error;
    }

    // Allocate buffer for GRH
    ctx->grh_buf = malloc(GRH_HEADER_SIZE);
    if (!ctx->grh_buf) {
        LOG_ERROR("Failed to allocate GRH buffer");
        ret = -1;
        goto error;
    }

    ctx->mr = ibv_reg_mr(ctx->pd, ctx->buf, BUFFER_SIZE, 
                        IBV_ACCESS_LOCAL_WRITE);
    if (!ctx->mr) {
        LOG_ERROR("Failed to register MR");
        goto error;
    }

    // Register memory for GRH
    ctx->grh_buf_mr = ibv_reg_mr(ctx->pd, ctx->grh_buf, GRH_HEADER_SIZE, 
                                IBV_ACCESS_LOCAL_WRITE);
    if (!ctx->grh_buf_mr) {
        LOG_ERROR("Failed to register GRH MR");
        goto error;
    }

    LOG_DEBUG("IB context initialized successfully");
    LOG_DEBUG("        Device: %s", ibv_get_device_name(dev));
    LOG_DEBUG("        Port: %d", ctx->ib_port);
    LOG_DEBUG("        LID: %d", ctx->lid);
    LOG_DEBUG("        MTU: %d", ctx->mtu);
    LOG_DEBUG("        Buffer: %p, size: %d", ctx->buf, BUFFER_SIZE);
    LOG_DEBUG("        GRH Buffer: %p, size: %d", ctx->grh_buf, GRH_HEADER_SIZE);
    LOG_DEBUG("        PKey table size: %d", port_attr.pkey_tbl_len);
    LOG_DEBUG("        max_srq_sge: %d", device_attr.max_srq_sge);

    return 0;

error:
    if (ctx->grh_buf_mr) ibv_dereg_mr(ctx->grh_buf_mr);
    if (ctx->mr) ibv_dereg_mr(ctx->mr);
    if (ctx->grh_buf) free(ctx->grh_buf);
    if (ctx->buf) free(ctx->buf);
    if (ctx->cq) ibv_destroy_cq(ctx->cq);
    if (ctx->pd) ibv_dealloc_pd(ctx->pd);
    if (ctx->dev) ibv_close_device(ctx->dev);
    if (ctx->send_wrs) free(ctx->send_wrs);
    if (ctx->recv_wrs) free(ctx->recv_wrs);
    if (ctx->send_sges) free(ctx->send_sges);
    if (ctx->recv_sges) free(ctx->recv_sges);
    return 1;
}

static int create_ud_qp(ib_context_t *ctx)
{
    struct ibv_qp_init_attr qp_init_attr = {
        .qp_type = IBV_QPT_UD,
        .send_cq = ctx->cq,
        .recv_cq = ctx->cq,
        .cap = {
            .max_send_wr = MAX_QP_WR,
            .max_recv_wr = MAX_QP_WR,
            .max_send_sge = 1,
            .max_recv_sge = 2,  // Support 2 SGE for GRH + Payload
            .max_inline_data = 0
        }
    };

    ctx->qp = ibv_create_qp(ctx->pd, &qp_init_attr);
    if (!ctx->qp) {
        LOG_ERROR("Failed to create QP: %s (errno=%d)", strerror(errno), errno);
        return -1;
    }

    LOG_DEBUG("QP created successfully");
    return 0;
}

static int setup_ud_qp(ib_context_t *ctx)
{
    struct ibv_qp_attr attr;
    struct ibv_port_attr port_attr;
    int flags;
    int ret;
    uint16_t pkey;
    int pkey_index;

    // Get port attributes
    ret = ibv_query_port(ctx->dev, ctx->ib_port, &port_attr);
    if (ret) {
        LOG_ERROR("Failed to query port: %s (errno=%d)", strerror(errno), errno);
        return ret;
    }

    // Find PKey
    for (pkey_index = 0; pkey_index < port_attr.pkey_tbl_len; pkey_index++) {
        ret = ibv_query_pkey(ctx->dev, ctx->ib_port, pkey_index, &pkey);
        if (ret) {
            LOG_ERROR("Failed to query PKey at index %d: %s (errno=%d)", pkey_index, strerror(errno), errno);
            return ret;
        }
        if (pkey == 0xffff) { // DEF_PKEY = 0xffff
            break;
        }
    }

    if (pkey_index >= port_attr.pkey_tbl_len) {
        pkey_index = 0;
        ret = ibv_query_pkey(ctx->dev, ctx->ib_port, pkey_index, &pkey);
        if (ret) {
            LOG_ERROR("Failed to query PKey at index 0: %s (errno=%d)", strerror(errno), errno);
            return ret;
        }
        if (!pkey) {
            LOG_ERROR("cannot find valid PKEY");
            return -1;
        }
        LOG_DEBUG("cannot find default pkey 0xffff on port %d, using index 0 pkey:0x%04x", ctx->ib_port, pkey);
    }


    // INIT
    memset(&attr, 0, sizeof(attr));
    attr.qp_state = IBV_QPS_INIT;
    attr.pkey_index = pkey_index;
    attr.port_num = ctx->ib_port;
    attr.qkey = DEF_QKEY;
    flags = IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT | IBV_QP_QKEY;
    ret = ibv_modify_qp(ctx->qp, &attr, flags);
    if (ret) {
        LOG_ERROR("Failed to modify QP to INIT: %s (errno=%d)", strerror(errno), errno);
        return ret;
    }

    // RTR
    memset(&attr, 0, sizeof(attr));
    attr.qp_state = IBV_QPS_RTR;
    flags = IBV_QP_STATE;
    ret = ibv_modify_qp(ctx->qp, &attr, flags);
    if (ret) {
        LOG_ERROR("Failed to modify QP to RTR: %s (errno=%d)", strerror(errno), errno);
        return ret;
    }

    // RTS
    memset(&attr, 0, sizeof(attr));
    attr.qp_state = IBV_QPS_RTS;
    attr.sq_psn = 0; // DEF_PSN = 0
    flags = IBV_QP_STATE | IBV_QP_SQ_PSN;
    ret = ibv_modify_qp(ctx->qp, &attr, flags);
    if (ret) {
        LOG_ERROR("Failed to modify QP to RTS: %s (errno=%d)", strerror(errno), errno);
        return ret;
    }

    
    // Check QP state and details
    struct ibv_qp_init_attr init_attr;
    ret = ibv_query_qp(ctx->qp, &attr, IBV_QP_STATE, &init_attr);
    if (ret) {
	    LOG_ERROR("Faild to query QP state: %s (errno=%d)", strerror(errno), errno);
        return ret;
    }

    LOG_DEBUG("QP setup completed");
    LOG_DEBUG("        Using PKey: 0x%04x (index: %d)", pkey, pkey_index);
    LOG_DEBUG("        QP state: %d (RTS=%d), QPN: %u, qkey: 0x%x, port: %d, lid: %d",
           attr.qp_state, IBV_QPS_RTS, ctx->qp->qp_num, DEF_QKEY, ctx->ib_port, ctx->lid);
    
    return 0;
}

static void cleanup_ib_context(ib_context_t *ctx)
{
    if (ctx->ah) {
        ibv_destroy_ah(ctx->ah);
        ctx->ah = NULL;
    }
    if (ctx->grh_buf_mr) ibv_dereg_mr(ctx->grh_buf_mr);
    if (ctx->mr) ibv_dereg_mr(ctx->mr);
    if (ctx->grh_buf) free(ctx->grh_buf);
    if (ctx->buf) free(ctx->buf);
    if (ctx->cq) ibv_destroy_cq(ctx->cq);
    if (ctx->pd) ibv_dealloc_pd(ctx->pd);
    if (ctx->dev) ibv_close_device(ctx->dev);
    if (ctx->devname) free(ctx->devname);
    if (ctx->send_wrs) free(ctx->send_wrs);
    if (ctx->recv_wrs) free(ctx->recv_wrs);
    if (ctx->send_sges) free(ctx->send_sges);
    if (ctx->recv_sges) free(ctx->recv_sges);
}

static int get_ipoib_interface_from_sysfs(const char *ib_dev_name, char *ipoib_ifname, size_t ifname_len)
{
    char sysfs_path[256];
    
    snprintf(sysfs_path, sizeof(sysfs_path), 
             "/sys/class/infiniband/%s/device/net", ib_dev_name);
    
    DIR *dir = opendir(sysfs_path);
    if (!dir) {
        LOG_ERROR("Cannot open %s: %s", sysfs_path, strerror(errno));
        return -1;
    }
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        // check if the interface name starts with "ib"
        if (strncmp(entry->d_name, "ib", 2) == 0) {
            LOG_DEBUG("Found IPoIB interface: %s", entry->d_name);
            strncpy(ipoib_ifname, entry->d_name, ifname_len - 1);
            ipoib_ifname[ifname_len - 1] = '\0';
            closedir(dir);
            return 0;
        }
    }
    
    closedir(dir);
    LOG_ERROR("No IPoIB interface found for %s", ib_dev_name);
    return -1;
}

static int get_ipoib_ipv4(const char *ifname, struct in_addr *ip_addr)
{
    struct ifaddrs *ifaddr = NULL;
    struct ifaddrs *ifa = NULL;
    int found = 0;
    
    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs failed");
        return -1;
    }
    
    // search for the IPv4 address of the specified interface
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) {
            continue;
        }
        
        // check the interface name and address family
        if (strcmp(ifa->ifa_name, ifname) == 0 && 
            ifa->ifa_addr->sa_family == AF_INET) {
            
            struct sockaddr_in *in_addr = (struct sockaddr_in *)ifa->ifa_addr;
            memcpy(ip_addr, &in_addr->sin_addr, sizeof(struct in_addr));
            LOG_DEBUG("Found IPv4 address: %s for interface %s", 
                   inet_ntoa(*ip_addr), ifname);
            found = 1;
            break;
        }
    }
    
    freeifaddrs(ifaddr);
    
    if (!found) {
        LOG_ERROR("No IPv4 address found for interface %s", ifname);
        return -1;
    }
    
    return 0;
}

static int get_ipv4_from_ib_device(const char *ib_dev_name, struct in_addr *ip_addr)
{
    char ipoib_ifname[IF_NAMESIZE];
    int ret;
    
    LOG_DEBUG("Getting IPv4 address for IB device: %s", ib_dev_name);
    
    // get the IPoIB interface from sysfs
    ret = get_ipoib_interface_from_sysfs(ib_dev_name, ipoib_ifname, sizeof(ipoib_ifname));
    if (ret != 0) {
        LOG_ERROR("Failed to get IPoIB interface for %s", ib_dev_name);
        return ret;
    }
    
    // get the IPv4 address of the IPoIB interface
    ret = get_ipoib_ipv4(ipoib_ifname, ip_addr);
    if (ret != 0) {
        LOG_ERROR("Failed to get IPv4 address for interface %s", ipoib_ifname);
        return ret;
    }
    
    return 0;
}

static int join_multicast_generic(struct sockaddr_storage *ipoib_addr, 
                                  const char *mcast_addr_str, 
                                  const union ibv_gid *dgid,
                                  mcast_info_t *mcast_info,
                                  int is_root)
{
    struct rdma_cm_id *mcast_id;
    struct rdma_event_channel *channel;
    struct sockaddr_in6 addr;
    int ret;

    // Create event channel
    channel = rdma_create_event_channel();
    if (!channel) {
        perror("rdma_create_event_channel failed");
        return -1;
    }

    // Create multicast RDMA ID
    ret = rdma_create_id(channel, &mcast_id, NULL, RDMA_PS_UDP);
    if (ret) {
        perror("rdma_create_id failed");
        rdma_destroy_event_channel(channel);
        return ret;
    }

    // Set up multicast address
    memset(&addr, 0, sizeof(addr));
    addr.sin6_family = AF_INET6;
    
    if (is_root) {
        // For root: use fake address string
        if (inet_pton(AF_INET6, mcast_addr_str, &addr.sin6_addr) != 1) {
            LOG_ERROR("Invalid fake multicast address: %s", mcast_addr_str);
            rdma_destroy_ep(mcast_id);
            rdma_destroy_event_channel(channel);
            return -1;
        }
    } else {
        // For others: use real GID
        memcpy(&addr.sin6_addr, dgid->raw, sizeof(dgid->raw));
    }

    LOG_DEBUG("Created RDMA ID successfully");

    // Bind to the IPoIB address
    ret = rdma_bind_addr(mcast_id, (struct sockaddr *)ipoib_addr);
    if (ret) {
        perror("rdma_bind_addr failed");
        rdma_destroy_ep(mcast_id);
        rdma_destroy_event_channel(channel);
        return ret;
    }

    LOG_DEBUG("RDMA ID %p, Address binding %s", mcast_id, mcast_addr_str);

    // Join the multicast group
    ret = rdma_join_multicast(mcast_id, (struct sockaddr *)&addr, NULL);
    if (ret) {
        perror("rdma_join_multicast failed");
        rdma_destroy_ep(mcast_id);
        rdma_destroy_event_channel(channel);
        return ret;
    }

    LOG_DEBUG("Multicast group join initiated");

    // Wait for join completion
    ret = rdma_get_cm_event(channel, &event);
    if (ret) {
        perror("rdma_get_cm_event failed");
        rdma_destroy_ep(mcast_id);
        rdma_destroy_event_channel(channel);
        return ret;
    }

    if (!is_root) {
        LOG_DEBUG("CM event received: %s", rdma_event_str(event->event));
    }

    if (event->event != RDMA_CM_EVENT_MULTICAST_JOIN) {
        LOG_ERROR("Unexpected event: %s", rdma_event_str(event->event));
        rdma_ack_cm_event(event);
        rdma_destroy_ep(mcast_id);
        rdma_destroy_event_channel(channel);
        return -1;
    }

    LOG_DEBUG("Successfully joined multicast group");
    
    if (is_root) {
        // Extract actual multicast information for root
        mcast_info->dgid = event->param.ud.ah_attr.grh.dgid;
        mcast_info->mlid = event->param.ud.ah_attr.dlid;
        
        // Convert GID to string
        if (inet_ntop(AF_INET6, mcast_info->dgid.raw, mcast_info->addr_str, INET6_ADDRSTRLEN)) {
            LOG_DEBUG("Actual multicast address: %s", mcast_info->addr_str);
            LOG_DEBUG("MLID: 0x%x", mcast_info->mlid);
        }
        
        rdma_ack_cm_event(event);
        rdma_destroy_ep(mcast_id);
        rdma_destroy_event_channel(channel);
    } else {
        // For non-root ranks: check details and store ID
        LOG_DEBUG("Multicast join details - MLID: 0x%x, GID: %s", 
               event->param.ud.ah_attr.dlid, mcast_info->addr_str);
        
        rdma_ack_cm_event(event);
        
        // Store the multicast ID for later use
        mcast_cm_id = mcast_id;
    }
    
    return 0;
}

static int root_join_multicast(struct sockaddr_storage *ipoib_addr, mcast_info_t *mcast_info)
{
    return join_multicast_generic(ipoib_addr, FAKE_MCAST_ADDR, NULL, mcast_info, 1);
}

static int all_join_multicast_real(struct sockaddr_storage *ipoib_addr, mcast_info_t *mcast_info)
{
    return join_multicast_generic(ipoib_addr, mcast_info->addr_str, &mcast_info->dgid, mcast_info, 0);
}

static int post_recv(ib_context_t *ctx, int len, int num_chunks)
{
    struct ibv_recv_wr *wrs = ctx->recv_wrs;
    struct ibv_sge *sges = ctx->recv_sges;
    struct ibv_recv_wr *bad_wr;
    int chunk_size = ctx->mtu - GRH_HEADER_SIZE;
    int ret = 0;
    int remaining = len;
    
    // check the buffer size
    if (len > BUFFER_SIZE) {
        LOG_ERROR("Total length %d too large for buffer %d", len, BUFFER_SIZE);
        return -1;
    }
    
    LOG_DEBUG("Posting %d receive WRs for %d bytes (chunk_size=%d)", num_chunks, len, chunk_size);
    
    for (int i = 0; i < num_chunks; i++) {
        int data_offset = i * chunk_size;
        int current_len = (remaining > chunk_size) ? chunk_size : remaining;
        
        // Set SGE (GRH + Payload)
        memset(&sges[2*i], 0, sizeof(struct ibv_sge));
        sges[2*i].addr = (uintptr_t)((char*)ctx->grh_buf);
        sges[2*i].length = GRH_HEADER_SIZE;
        sges[2*i].lkey = ctx->grh_buf_mr->lkey;

        memset(&sges[2*i+1], 0, sizeof(struct ibv_sge));
        sges[2*i+1].addr = (uintptr_t)((char*)ctx->buf + data_offset);
        sges[2*i+1].length = current_len;
        sges[2*i+1].lkey = ctx->mr->lkey;
        
        // Set WR
        memset(&wrs[i], 0, sizeof(struct ibv_recv_wr));
        wrs[i].wr_id = 2 + i; // Use the same WR ID as the sender
        wrs[i].sg_list = &sges[2*i];
        wrs[i].num_sge = 2;
        
        // Link to next WR
        if (i < num_chunks - 1) {
            wrs[i].next = &wrs[i + 1];
        } else {
            wrs[i].next = NULL;
        }
        remaining -= current_len;
    }
    
    // Post receive WR
    ret = ibv_post_recv(ctx->qp, wrs, &bad_wr);
    if (ret) {
        LOG_ERROR("Failed to post receive: %s", strerror(ret));
        return ret;
    }
    
    LOG_DEBUG("Posted %d receive WRs successfully", num_chunks);
    
    return ret;
}

static int post_send(ib_context_t *ctx, int len, int num_chunks)
{
    struct ibv_send_wr *wrs = ctx->send_wrs;
    struct ibv_sge *sges = ctx->send_sges;
    struct ibv_send_wr *bad_wr;
    int chunk_size = ctx->mtu - GRH_HEADER_SIZE;
    int ret = 0;
    
    LOG_DEBUG("Sending %d bytes in %d chunks (chunk_size=%d)", 
           len, num_chunks, chunk_size);
   
    int remaining = len;
    for (int i = 0; i < num_chunks; i++) {
        int data_offset = i * chunk_size;
        int current_len = (remaining > chunk_size) ? chunk_size : remaining;
        
        // Set SGE
        memset(&sges[i], 0, sizeof(struct ibv_sge));
        sges[i].addr = (uintptr_t)((char*)ctx->buf + data_offset);
        sges[i].length = current_len;
        sges[i].lkey = ctx->mr->lkey;
        
        // Set WR
        memset(&wrs[i], 0, sizeof(struct ibv_send_wr));
        wrs[i].wr_id = 2 + i; // Unique WR ID
        wrs[i].opcode = IBV_WR_SEND;
        wrs[i].send_flags = (i == num_chunks - 1) ? IBV_SEND_SIGNALED : 0;
        wrs[i].sg_list = &sges[i];
        wrs[i].num_sge = 1;
        wrs[i].wr.ud.remote_qpn = 0xFFFFFF; // For multicast
        wrs[i].wr.ud.remote_qkey = DEF_QKEY;
        wrs[i].wr.ud.ah = ctx->ah;
        
        // Link to next WR
        if (i < num_chunks - 1) {
            wrs[i].next = &wrs[i + 1];
        } else {
            wrs[i].next = NULL;
        }
        remaining -= current_len;
    }

    // Post batch send
    ret = ibv_post_send(ctx->qp, wrs, &bad_wr);
    if (ret) {
        LOG_ERROR("Failed to post batch send: %s (errno=%d)", 
               strerror(errno), errno);
        return ret;
    }
    LOG_DEBUG("Batch send posted successfully (%d chunks; wr_ids %lu-%lu)", num_chunks, wrs[0].wr_id, wrs[num_chunks-1].wr_id);
    
    return ret;
}

static int wait_for_completion(ib_context_t *ctx, int expected_wrs)
{
    struct ibv_wc wc;
    int ret;
    int timeout_counter = 0;
    const int MAX_TIMEOUT = 10000; // 10 seconds timeout
    int completed_wrs = 0;

    LOG_DEBUG("Waiting for completion... (expected WRs: %d)", expected_wrs);

    while (completed_wrs < expected_wrs) {
        ret = ibv_poll_cq(ctx->cq, 1, &wc);
        if (ret < 0) {
            LOG_ERROR("CQ poll failed: %s", strerror(ret));
            return 1;
        }
        if (ret == 0) {
            timeout_counter++;
            if (timeout_counter % 1000 == 0) {
                LOG_INFO("Still polling... (timeout: %d ms, completed: %d/%d)", 
                       timeout_counter, completed_wrs, expected_wrs);
            }
            if (timeout_counter > MAX_TIMEOUT) {
                LOG_INFO("Timeout waiting for completion (completed: %d/%d)", 
                       completed_wrs, expected_wrs);
                return 1;
            }
            usleep(1000);  // sleep 1ms to reduce busy wait
            continue;
        }

        LOG_DEBUG("CQ entry found: wr_id=%lu, status=%s", 
               wc.wr_id, ibv_wc_status_str(wc.status));

        if (wc.status != IBV_WC_SUCCESS) {
            LOG_ERROR("WC error: %s", ibv_wc_status_str(wc.status));
            return 1;
        }
        
        completed_wrs++;
        LOG_DEBUG("Received %d completion (wr_id=%lu)", completed_wrs, wc.wr_id);
    }
    LOG_DEBUG("Received WRs: %d, expected: %d", completed_wrs, expected_wrs);
    return 0;
}

static int attach_qp_to_multicast(ib_context_t *ctx, mcast_info_t *mcast_info)
{
    int ret;
    
    ret = ibv_attach_mcast(ctx->qp, &mcast_info->dgid, mcast_info->mlid);
    if (ret) {
        LOG_ERROR("Failed to attach QP to multicast group: %s (errno=%d)", 
               strerror(errno), errno);
        return ret;
    }
    
    LOG_DEBUG("QP attached to multicast group successfully");
    return 0;
}

static int detach_qp_from_multicast(ib_context_t *ctx, mcast_info_t *mcast_info)
{
    int ret;
    
    ret = ibv_detach_mcast(ctx->qp, &mcast_info->dgid, mcast_info->mlid);
    if (ret) {
        LOG_ERROR("Failed to detach QP from multicast group: %s (errno=%d)", 
               strerror(errno), errno);
        return ret;
    }
    
    LOG_DEBUG("QP detached from multicast group successfully");
    return 0;
}

static int verify_received_data(ib_context_t *ctx, int size)
{
    char *data = (char *)ctx->buf;
    int i;
    int error_count = 0;
    const int MAX_ERRORS_TO_REPORT = 10;
    
    LOG_DEBUG("Verifying received data (size: %d bytes)", size);
    
    for (i = 0; i < size; i++) {
        if (data[i] != TEST_DATA_VALUE) {
            error_count++;
            if (error_count <= MAX_ERRORS_TO_REPORT) {
                LOG_ERROR("Data mismatch at position %d: expected %d, got %d", i, TEST_DATA_VALUE, (int)data[i]);
            }
        }
    }
    
    if (error_count > 0) {
        LOG_ERROR("Data verification failed: %d errors out of %d bytes", error_count, size);
        if (error_count > MAX_ERRORS_TO_REPORT) {
            LOG_ERROR("... and %d more errors (showing first %d only)", 
                   error_count - MAX_ERRORS_TO_REPORT, MAX_ERRORS_TO_REPORT);
        }
        return -1;
    }
    
    LOG_DEBUG("Data verification successful: all %d bytes are correct", size);
    return 0;
}

static int create_ah(ib_context_t *ctx, mcast_info_t *mcast_info)
{
    struct ibv_ah_attr ah_attr;
    
    memset(&ah_attr, 0, sizeof(ah_attr));
    ah_attr.is_global = 1;
    ah_attr.port_num = ctx->ib_port;
    ah_attr.grh.dgid = mcast_info->dgid;
    ah_attr.grh.sgid_index = 0;
    ah_attr.grh.flow_label = 0;
    ah_attr.grh.hop_limit = 1;
    ah_attr.grh.traffic_class = 0;
    ah_attr.dlid = mcast_info->mlid;
    ah_attr.sl = 0;
    
    ctx->ah = ibv_create_ah(ctx->pd, &ah_attr);
    if (!ctx->ah) {
        LOG_ERROR("Failed to create address handle: %s (errno=%d)", 
               strerror(errno), errno);
        return -1;
    }
    
    LOG_DEBUG("AH created successfully for multicast");
    return 0;
}

int main(int argc, char *argv[])
{
    char *dev_name = NULL;
    int opt;
    ib_context_t ctx = {0};
    struct sockaddr_storage ip_oib_addr;
    mcast_info_t mcast_info = {0};
    int ret;
    char *log_level_env;
    perf_params_t perf_params = {
        .warmup_iterations = 10,
        .test_iterations = 100,
        .size_step = 2,
        .min_size = 1024,
        .max_size = 32 * 1024 * 1024
    };

    // Initialize MPI
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);

    // Set log level
    log_level_env = getenv("LOG_LEVEL");
    if (log_level_env != NULL) {
        if (strcmp(log_level_env, "ERROR") == 0 || strcmp(log_level_env, "0") == 0) {
            g_log_level = LOG_LEVEL_ERROR;
        } else if (strcmp(log_level_env, "INFO") == 0 || strcmp(log_level_env, "1") == 0) {
            g_log_level = LOG_LEVEL_INFO;
        } else if (strcmp(log_level_env, "DEBUG") == 0 || strcmp(log_level_env, "2") == 0) {
            g_log_level = LOG_LEVEL_DEBUG;
        } else {
            LOG_ERROR("Invalid LOG_LEVEL value: %s. Using default (INFO)", log_level_env);
            LOG_ERROR("Valid values: ERROR(0), INFO(1), DEBUG(2)");
        }
    }

    LOG_INFO("Starting IB multicast performance test:");
    LOG_DEBUG("Log level: %d", g_log_level);

    // Parse command line arguments
    while ((opt = getopt(argc, argv, "d:l:u:w:i:s:h")) != -1) {
        switch (opt) {
        case 'd':
            dev_name = optarg;
            break;
        case 'l':
            perf_params.min_size = atoi(optarg);
            break;
        case 'u':
            perf_params.max_size = atoi(optarg);
            break;
        case 'w':
            perf_params.warmup_iterations = atoi(optarg);
            break;
        case 'i':
            perf_params.test_iterations = atoi(optarg);
            break;
        case 's':
            perf_params.size_step = atoi(optarg);
            break;
        case 'h':
            print_usage(argv[0]);
            MPI_Finalize();
            return 0;
        default:
            print_usage(argv[0]);
            MPI_Finalize();
            return 1;
        }
    }

    if (mpi_rank == 0) {
        LOG_INFO("Test sizes: %d to %d bytes (step: %dx)", perf_params.min_size, perf_params.max_size, perf_params.size_step);
        LOG_INFO("Warmup iterations: %d, Test iterations: %d", perf_params.warmup_iterations, perf_params.test_iterations);
    }

    // Initialize IB context
    ret = init_ib_context(&ctx, dev_name);
    if (ret < 0) {
        LOG_ERROR("Failed to initialize IB context");
        goto cleanup;
    }

    // Create QP
    ret = create_ud_qp(&ctx);
    if (ret < 0) {
        LOG_ERROR("Failed to create QP");
        goto cleanup;
    }

    // Set up QP
    ret = setup_ud_qp(&ctx);
    if (ret < 0) {
        LOG_ERROR("Failed to setup QP");
        goto cleanup;
    }

    // Get IPoIB address
    struct in_addr ip_addr = {0}; // Initialize to zero
    ret = get_ipv4_from_ib_device(dev_name ? dev_name : "mlx5_0", &ip_addr);
    if (ret < 0) {
        LOG_ERROR("Failed to get IPoIB address");
        goto cleanup;
    }
    
    // Convert to sockaddr_storage format
    memset(&ip_oib_addr, 0, sizeof(ip_oib_addr));
    struct sockaddr_in *addr_in = (struct sockaddr_in *)&ip_oib_addr;
    addr_in->sin_family = AF_INET;
    addr_in->sin_addr = ip_addr;
    addr_in->sin_port = 0; // Let the system choose a port

    // Root rank: Join multicast with fake address and get real address
    if (mpi_rank == 0) {
        ret = root_join_multicast(&ip_oib_addr, &mcast_info);
        if (ret < 0) {
            LOG_ERROR("Root rank: Failed to join multicast group");
            goto cleanup;
        }
    }

    // Broadcast multicast info from root to all ranks
    MPI_Bcast(&mcast_info, sizeof(mcast_info_t), MPI_BYTE, 0, MPI_COMM_WORLD);
    
    if (mpi_rank != 0) {
        LOG_DEBUG("Received multicast info from root");
        LOG_DEBUG("Real address: %s", mcast_info.addr_str);
        LOG_DEBUG("MLID: 0x%x", mcast_info.mlid);
    }

    // All ranks: Join multicast with real address
    ret = all_join_multicast_real(&ip_oib_addr, &mcast_info);
    if (ret < 0) {
        LOG_ERROR("Failed to join multicast group");
        goto cleanup;
    }

    // Attach QP to multicast group
    ret = attach_qp_to_multicast(&ctx, &mcast_info);
    if (ret < 0) {
        LOG_ERROR("Failed to attach QP to multicast");
        goto cleanup;
    }

    // Create Address Handle
    ret = create_ah(&ctx, &mcast_info);
    if (ret < 0) {
        LOG_ERROR("Failed to create AH");
        goto cleanup;
    }

    // Allocate work requests and sges
    ret = allocate_wrs_and_sges(&ctx, &perf_params);
    if (ret < 0) {
        LOG_ERROR("Failed to allocate work requests and sges");
        goto cleanup;
    }

    LOG_DEBUG("Successfully joined multicast group");

    // Synchronize before starting performance tests
    MPI_Barrier(MPI_COMM_WORLD);
    
    if (mpi_rank == 0) {
        LOG_INFO("Starting performance tests...");
    }

    // Run performance test suite
    run_performance_suite(&ctx, &perf_params);

    // Synchronize before cleanup
    MPI_Barrier(MPI_COMM_WORLD);
    LOG_DEBUG("Performance test completed");

    // Destroy AH here (safe timing after all tests)
    if (ctx.ah) {
        ibv_destroy_ah(ctx.ah);
        ctx.ah = NULL;
        LOG_DEBUG("AH destroyed");
    }

cleanup:
    // Detach QP from multicast group
    if (ctx.qp && mcast_info.addr_str[0] != '\0') {
        detach_qp_from_multicast(&ctx, &mcast_info);
    }
    
    if (mcast_cm_id) {
        rdma_destroy_ep(mcast_cm_id);
    }
    cleanup_ib_context(&ctx);
    MPI_Finalize();
    return ret < 0 ? 1 : 0;
} 
