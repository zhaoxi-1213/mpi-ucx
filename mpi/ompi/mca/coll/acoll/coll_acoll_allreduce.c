/* -*- Mode: C; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2024 Advanced Micro Devices, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "ompi_config.h"


#include "mpi.h"
#include "ompi/communicator/communicator.h"
#include "ompi/constants.h"
#include "ompi/datatype/ompi_datatype.h"
#include "ompi/mca/coll/base/coll_tags.h"
#include "ompi/mca/coll/coll.h"
#include "ompi/mca/pml/pml.h"
#include "ompi/op/op.h"
#include "opal/util/bit_ops.h"
#include "coll_acoll.h"
#include "coll_acoll_utils.h"


void mca_coll_acoll_sync(coll_acoll_data_t *data, int offset, int *group, int gp_size, int rank, int up);
int mca_coll_acoll_allreduce_small_msgs_h(const void *sbuf, void *rbuf, size_t count,
                                          struct ompi_datatype_t *dtype, struct ompi_op_t *op,
                                          struct ompi_communicator_t *comm,
                                          mca_coll_base_module_t *module,
                                          coll_acoll_subcomms_t *subc, int intra);


static inline int coll_allreduce_decision_fixed(int comm_size, size_t msg_size)
{
    int alg = 3;
    if (msg_size <= 256) {
        alg = 1;
    } else if (msg_size <= 1045876) {
        alg = 2;
    } else if (msg_size <= 4194304) {
        alg = 3;
    } else if (msg_size <= 8388608) {
        alg = 0;
    } else {
        alg = 3;
    }
    return alg;
}

static inline int mca_coll_acoll_reduce_smsc_h(const void *sbuf, void *rbuf, size_t count,
                                                struct ompi_datatype_t *dtype, struct ompi_op_t *op,
                                                struct ompi_communicator_t *comm,
                                                mca_coll_base_module_t *module,
                                                coll_acoll_subcomms_t *subc)
{
    int size;
    size_t total_dsize, dsize;

    coll_acoll_init(module, comm, subc->data, subc, 0);
    coll_acoll_data_t *data = subc->data;
    if (NULL == data) {
        return -1;
    }

    size = ompi_comm_size(comm);
    int rank = ompi_comm_rank(comm);
    ompi_datatype_type_size(dtype, &dsize);
    total_dsize = dsize * count;

    int l1_gp_size = data->l1_gp_size;
    int *l1_gp = data->l1_gp;
    int *l2_gp = data->l2_gp;
    int l2_gp_size = data->l2_gp_size;

    int l1_local_rank = data->l1_local_rank;
    int l2_local_rank = data->l2_local_rank;
    char *tmp_sbuf = NULL;
    char *tmp_rbuf = NULL;
    if (!subc->smsc_use_sr_buf) {
        tmp_rbuf = (char *) data->scratch;
        tmp_sbuf = (char *) data->scratch + (subc->smsc_buf_size) / 2;
        if ((MPI_IN_PLACE == sbuf)) {
            memcpy(tmp_sbuf, rbuf, total_dsize);
        } else {
            memcpy(tmp_sbuf, sbuf, total_dsize);
        }
    } else {
        tmp_sbuf = (char *) sbuf;
        tmp_rbuf = (char *) rbuf;
        if (MPI_IN_PLACE == sbuf) {
            tmp_sbuf = (char *) rbuf;
        }
    }
    void *sbuf_vaddr[1] = {tmp_sbuf};
    void *rbuf_vaddr[1] = {tmp_rbuf};
    int err = MPI_SUCCESS;

    err = comm->c_coll->coll_allgather(sbuf_vaddr, sizeof(void *), MPI_BYTE, data->allshm_sbuf,
                                       sizeof(void *), MPI_BYTE, comm,
                                       comm->c_coll->coll_allgather_module);
    if (err != MPI_SUCCESS) {
        return err;
    }

    err = comm->c_coll->coll_allgather(rbuf_vaddr, sizeof(void *), MPI_BYTE, data->allshm_rbuf,
                                       sizeof(void *), MPI_BYTE, comm,
                                       comm->c_coll->coll_allgather_module);
    if (err != MPI_SUCCESS) {
        return err;
    }

    err = register_mem_with_smsc(rank, size, total_dsize, data, comm);
    if (err != MPI_SUCCESS) {
        return err;
    }

    /* reduce to the local group leader */
    size_t chunk = count / l1_gp_size;
    size_t my_count_size = (l1_local_rank == (l1_gp_size - 1)) ? chunk + count % l1_gp_size : chunk;

    if (rank == l1_gp[0]) {
        if (sbuf != MPI_IN_PLACE)
            memcpy(tmp_rbuf, sbuf, my_count_size * dsize);

        for (int i = 1; i < l1_gp_size; i++) {
            ompi_op_reduce(op, (char *) data->smsc_saddr[l1_gp[i]] + chunk * l1_local_rank * dsize,
                           (char *) tmp_rbuf + chunk * l1_local_rank * dsize, my_count_size, dtype);
        }
    } else {
        ompi_3buff_op_reduce(op,
                             (char *) data->smsc_saddr[l1_gp[0]] + chunk * l1_local_rank * dsize,
                             (char *) tmp_sbuf + chunk * l1_local_rank * dsize,
                             (char *) data->smsc_raddr[l1_gp[0]] + chunk * l1_local_rank * dsize,
                             my_count_size, dtype);
        for (int i = 1; i < l1_gp_size; i++) {
            if (i == l1_local_rank) {
                continue;
            }
            ompi_op_reduce(op, (char *) data->smsc_saddr[l1_gp[i]] + chunk * l1_local_rank * dsize,
                           (char *) data->smsc_raddr[l1_gp[0]] + chunk * l1_local_rank * dsize,
                           my_count_size, dtype);
        }
    }
    err = ompi_coll_base_barrier_intra_tree(comm, module);
    if (err != MPI_SUCCESS) {
        return err;
    }

    /* perform reduce to 0 */
    int local_size = l2_gp_size;
    if ((rank == l1_gp[0]) && (local_size > 1)) {
        chunk = count / local_size;

        my_count_size = (l2_local_rank == (local_size - 1)) ? chunk + (count % local_size) : chunk;

        if (0 == l2_local_rank) {
            for (int i = 1; i < local_size; i++) {
                ompi_op_reduce(op, (char *) data->smsc_raddr[l2_gp[i]], (char *) tmp_rbuf,
                               my_count_size, dtype);
            }
        } else {
            for (int i = 1; i < local_size; i++) {
                if (i == l2_local_rank) {
                    continue;
                }

                ompi_op_reduce(op,
                               (char *) data->smsc_raddr[l2_gp[i]] + chunk * l2_local_rank * dsize,
                               (char *) data->smsc_raddr[0] + chunk * l2_local_rank * dsize,
                               my_count_size, dtype);
            }
            ompi_op_reduce(op, (char *) tmp_rbuf + chunk * l2_local_rank * dsize,
                           (char *) data->smsc_raddr[0] + chunk * l2_local_rank * dsize,
                           my_count_size, dtype);
        }
    }

    err = ompi_coll_base_barrier_intra_tree(comm, module);
    if (!subc->smsc_use_sr_buf) {
        memcpy(rbuf, tmp_rbuf, total_dsize);
    }
    // Note: neither unmap nor deregister will have any effect here, just having it for consistency
    unmap_mem_with_smsc(rank, size, data);
    return err;
}

static inline int mca_coll_acoll_allreduce_smsc_f(const void *sbuf, void *rbuf, size_t count,
                                                   struct ompi_datatype_t *dtype,
                                                   struct ompi_op_t *op,
                                                   struct ompi_communicator_t *comm,
                                                   mca_coll_base_module_t *module,
                                                   coll_acoll_subcomms_t *subc)
{
    int size;
    size_t total_dsize, dsize;

    coll_acoll_init(module, comm, subc->data, subc, 0);
    coll_acoll_data_t *data = subc->data;
    if (NULL == data) {
        return -1;
    }

    size = ompi_comm_size(comm);
    ompi_datatype_type_size(dtype, &dsize);
    total_dsize = dsize * count;

    char *tmp_sbuf = NULL;
    char *tmp_rbuf = NULL;
    if (!subc->smsc_use_sr_buf) {
        tmp_rbuf = (char *) data->scratch;
        tmp_sbuf = (char *) data->scratch + (subc->smsc_buf_size) / 2;
        if ((MPI_IN_PLACE == sbuf)) {
            memcpy(tmp_sbuf, rbuf, total_dsize);
        } else {
            memcpy(tmp_sbuf, sbuf, total_dsize);
        }
    } else {
        tmp_sbuf = (char *) sbuf;
        tmp_rbuf = (char *) rbuf;
        if (MPI_IN_PLACE == sbuf) {
            tmp_sbuf = (char *) rbuf;
        }
    }
    void *sbuf_vaddr[1] = {tmp_sbuf};
    void *rbuf_vaddr[1] = {tmp_rbuf};
    int err = MPI_SUCCESS;
    int rank = ompi_comm_rank(comm);

    err = comm->c_coll->coll_allgather(sbuf_vaddr, sizeof(void *), MPI_BYTE, data->allshm_sbuf,
                                       sizeof(void *), MPI_BYTE, comm,
                                       comm->c_coll->coll_allgather_module);
    if (err != MPI_SUCCESS) {
        return err;
    }
    err = comm->c_coll->coll_allgather(rbuf_vaddr, sizeof(void *), MPI_BYTE, data->allshm_rbuf,
                                       sizeof(void *), MPI_BYTE, comm,
                                       comm->c_coll->coll_allgather_module);

    if (err != MPI_SUCCESS) {
        return err;
    }

    err = register_mem_with_smsc(rank, size, total_dsize, data, comm);
    if (err != MPI_SUCCESS) {
        return err;
    }

    size_t chunk = count / size;
    size_t my_count_size = (rank == (size - 1)) ? (count / size) + count % size : count / size;
    if (0 == rank) {
        if (sbuf != MPI_IN_PLACE)
            memcpy(tmp_rbuf, sbuf, my_count_size * dsize);
    } else {
        ompi_3buff_op_reduce(op, (char *) data->smsc_saddr[0] + chunk * rank * dsize,
                             (char *) tmp_sbuf + chunk * rank * dsize,
                             (char *) tmp_rbuf + chunk * rank * dsize, my_count_size, dtype);
    }

    err = ompi_coll_base_barrier_intra_tree(comm, module);
    if (err != MPI_SUCCESS) {
        return err;
    }

    for (int i = 1; i < size; i++) {
        if (rank == i) {
            continue;
        }
        ompi_op_reduce(op, (char *) data->smsc_saddr[i] + chunk * rank * dsize,
                       (char *) tmp_rbuf + chunk * rank * dsize, my_count_size, dtype);
    }
    err = ompi_coll_base_barrier_intra_tree(comm, module);
    if (err != MPI_SUCCESS) {
        return err;
    }

    size_t tmp = chunk * dsize;
    for (int i = 0; i < size; i++) {
        if (subc->smsc_use_sr_buf && (rank == i)) {
            continue;
        }
        my_count_size = (i == (size - 1)) ? (count / size) + count % size : count / size;
        size_t tmp1 = i * tmp;
        char *dst = (char *) rbuf + tmp1;
        char *src = (char *) data->smsc_raddr[i] + tmp1;
        memcpy(dst, src, my_count_size * dsize);
    }

    err = ompi_coll_base_barrier_intra_tree(comm, module);

    // Note: neither unmap nor deregister will have any effect here, just having it for consistency
    unmap_mem_with_smsc(rank, size, data);

    return err;
}

void mca_coll_acoll_sync(coll_acoll_data_t *data, int offset, int *group, int gp_size, int rank,
                         int up)
{
    volatile int *tmp, tmp0;
    tmp = (int *) ((char *) data->allshmmmap_sbuf[group[0]] + offset
                   + CACHE_LINE_SIZE * rank);
    tmp0 = __atomic_load_n((int *) ((char *) data->allshmmmap_sbuf[group[0]] + offset
                                    + CACHE_LINE_SIZE * group[0]),
                           __ATOMIC_RELAXED);

    opal_atomic_wmb();

    int val;
    if (1 == up) {
        val = data->sync[0];
    } else {
        val = data->sync[1];
    }

    if (rank == group[0]) {
        __atomic_store_n((int *) ((char *) data->allshmmmap_sbuf[group[0]] + offset
                                  + CACHE_LINE_SIZE * group[0]),
                         val, __ATOMIC_RELAXED);
    }

    while (tmp0 != val) {
        tmp0 = __atomic_load_n((int *) ((char *) data->allshmmmap_sbuf[group[0]] + offset
                                        + CACHE_LINE_SIZE * group[0]),
                               __ATOMIC_RELAXED);
    }

    if (rank != group[0]) {
        val++;
        __atomic_store_n(tmp, val, __ATOMIC_RELAXED);
    }
    opal_atomic_wmb();
    if (rank == group[0]) {
        for (int i = 1; i < gp_size; i++) {
            volatile int tmp1 = __atomic_load_n(
                (int *) ((char *) data->allshmmmap_sbuf[group[0]] + offset + CACHE_LINE_SIZE * group[i]),
                __ATOMIC_RELAXED);
            while (tmp1 == val) {
                tmp1 = __atomic_load_n((int *) ((char *) data->allshmmmap_sbuf[group[0]] + offset
                                                + CACHE_LINE_SIZE * group[i]),
                                       __ATOMIC_RELAXED);
            }
            opal_atomic_wmb();
        }
        ++val;
        __atomic_store_n(tmp, val, __ATOMIC_RELAXED);
    } else {
        volatile int tmp1 = __atomic_load_n(
            (int *) ((char *) data->allshmmmap_sbuf[group[0]] + offset + CACHE_LINE_SIZE * group[0]),
            __ATOMIC_RELAXED);
        while (tmp1 != val) {
            tmp1 = __atomic_load_n((int *) ((char *) data->allshmmmap_sbuf[group[0]] + offset
                                            + CACHE_LINE_SIZE * group[0]),
                                   __ATOMIC_RELAXED);
        }
    }
    if (1 == up) {
        data->sync[0] = val;
    } else {
        data->sync[1] = val;
    }
}

int mca_coll_acoll_allreduce_small_msgs_h(const void *sbuf, void *rbuf, size_t count,
                                          struct ompi_datatype_t *dtype, struct ompi_op_t *op,
                                          struct ompi_communicator_t *comm,
                                          mca_coll_base_module_t *module,
                                          coll_acoll_subcomms_t *subc, int intra)
{
    size_t dsize;
    int err = MPI_SUCCESS;
    coll_acoll_init(module, comm, subc->data, subc, 0);
    coll_acoll_data_t *data = subc->data;
    if (NULL == data) {
        return -1;
    }

    int rank = ompi_comm_rank(comm);
    ompi_datatype_type_size(dtype, &dsize);

    int l1_gp_size = data->l1_gp_size;
    int *l1_gp = data->l1_gp;
    int *l2_gp = data->l2_gp;
    int l2_gp_size = data->l2_gp_size;

    int l1_local_rank = data->l1_local_rank;
    int l2_local_rank = data->l2_local_rank;

    int offset1 = data->offset[0];
    int offset2 = data->offset[1];
    int tshm_offset = data->offset[2];
    int shm_offset = data->offset[3];
    const int per_rank_shm_size = 8 * 1024;

    int local_size;

    if (rank == l1_gp[0]) {
        if (l2_gp_size > 1) {
            mca_coll_acoll_sync(data, offset2, l2_gp, l2_gp_size, rank, 3);
        }
    }

    if (MPI_IN_PLACE == sbuf) {
        memcpy((char *) data->allshmmmap_sbuf[l1_gp[0]] + shm_offset, rbuf, count * dsize);
    } else {
        memcpy((char *) data->allshmmmap_sbuf[l1_gp[0]] + shm_offset, sbuf, count * dsize);
    }

    mca_coll_acoll_sync(data, offset1, l1_gp, l1_gp_size, rank, 1);

    if (rank == l1_gp[0]) {
        memcpy((char *) data->allshmmmap_sbuf[l1_gp[l1_local_rank]],
               (char *) data->allshmmmap_sbuf[l1_gp[0]] + shm_offset, count * dsize);
        for (int i = 1; i < l1_gp_size; i++) {
            ompi_op_reduce(op,
                           (char *) data->allshmmmap_sbuf[l1_gp[0]] + tshm_offset
                               + l1_gp[i] * per_rank_shm_size,
                           (char *) data->allshmmmap_sbuf[l1_gp[l1_local_rank]], count, dtype);
        }
        memcpy(rbuf, data->allshmmmap_sbuf[l1_gp[l1_local_rank]], count * dsize);
    }

    if (rank == l1_gp[0]) {
        if (l2_gp_size > 1) {
            mca_coll_acoll_sync(data, offset2, l2_gp, l2_gp_size, rank, 3);
        }
    }

    /* perform allreduce across leaders */
    local_size = l2_gp_size;
    if (local_size > 1) {
        if (rank == l1_gp[0]) {
            for (int i = 0; i < local_size; i++) {
                if (i == l2_local_rank) {
                    continue;
                }
                ompi_op_reduce(op, (char *) data->allshmmmap_sbuf[l2_gp[i]], (char *) rbuf, count,
                               dtype);
            }
        }
    }

    if (intra && (ompi_comm_size(subc->numa_comm) > 1)) {
        err = ompi_coll_base_bcast_intra_basic_linear(rbuf, count, dtype, 0, subc->numa_comm, module);
    }
    return err;
}

int mca_coll_acoll_allreduce_intra(const void *sbuf, void *rbuf, size_t count,
                                   struct ompi_datatype_t *dtype, struct ompi_op_t *op,
                                   struct ompi_communicator_t *comm, mca_coll_base_module_t *module)
{
    int size, alg, err;
    int num_nodes;
    size_t total_dsize, dsize;
    mca_coll_acoll_module_t *acoll_module = (mca_coll_acoll_module_t *) module;
    size = ompi_comm_size(comm);
    ompi_datatype_type_size(dtype, &dsize);
    total_dsize = dsize * count;

    /* Disable smsc/shm/xpmem based optimizations if: */
    /* - datatype is not a predefined type */
    /* - it's a gpu buffer */
    uint64_t flags = 0;
    int dev_id;
    bool is_opt = true;
    if (!OMPI_COMM_CHECK_ASSERT_NO_ACCEL_BUF(comm)) {
        if (!ompi_datatype_is_predefined(dtype)
            || (0 < opal_accelerator.check_addr(sbuf, &dev_id, &flags))
            || (0 < opal_accelerator.check_addr(rbuf, &dev_id, &flags))) {
            is_opt = false;
        }
    }

    if ((1 == size) && is_opt) {
        if (MPI_IN_PLACE != sbuf) {
            memcpy((char *) rbuf, sbuf, total_dsize);
        }
        return MPI_SUCCESS;
    }

    /* Falling back to recursivedoubling for non-commutative operators to be safe */
    if (!ompi_op_is_commute(op)) {
        return ompi_coll_base_allreduce_intra_recursivedoubling(sbuf, rbuf, count, dtype, op, comm,
                                                                module);
    }

    /* Obtain the subcomms structure */
    coll_acoll_subcomms_t *subc = NULL;
    err = check_and_create_subc(comm, acoll_module, &subc);

    if (MPI_SUCCESS != err) {
        return err;
    }

    /* Fallback to knomial if subc is not obtained */
    if (NULL == subc) {
        return ompi_coll_base_allreduce_intra_redscat_allgather(sbuf, rbuf, count, dtype, op, comm,
                                                                module);
    }
    if (!subc->initialized) {
        err = mca_coll_acoll_comm_split_init(comm, acoll_module, subc, 0);
        if (MPI_SUCCESS != err)
            return err;
    }

    num_nodes = subc->num_nodes;

    alg = coll_allreduce_decision_fixed(size, total_dsize);

    if (1 == num_nodes) {
        if (total_dsize < 32) {
            return ompi_coll_base_allreduce_intra_recursivedoubling(sbuf, rbuf, count, dtype, op,
                                                                    comm, module);
        } else if ((total_dsize < 512) && is_opt) {
            return mca_coll_acoll_allreduce_small_msgs_h(sbuf, rbuf, count, dtype, op, comm, module,
                                                         subc, 1);
        } else if (total_dsize <= 2048) {
            return ompi_coll_base_allreduce_intra_recursivedoubling(sbuf, rbuf, count, dtype, op,
                                                                    comm, module);
        } else if (total_dsize < 65536) {
            if (1 == alg) {
                return ompi_coll_base_allreduce_intra_recursivedoubling(sbuf, rbuf, count, dtype,
                                                                        op, comm, module);
            } else if (2 == alg) {
                return ompi_coll_base_allreduce_intra_redscat_allgather(sbuf, rbuf, count, dtype,
                                                                        op, comm, module);
            } else { /*alg == 3 */
                return ompi_coll_base_allreduce_intra_ring_segmented(sbuf, rbuf, count, dtype, op,
                                                                     comm, module, 0);
            }
        } else if (total_dsize < 4194304) {
            if (((subc->smsc_use_sr_buf != 0) || (subc->smsc_buf_size > 2 * total_dsize)) && (subc->without_smsc != 1) && is_opt) {
                return mca_coll_acoll_allreduce_smsc_f(sbuf, rbuf, count, dtype, op, comm, module, subc);
            } else {
                return ompi_coll_base_allreduce_intra_redscat_allgather(sbuf, rbuf, count, dtype,
                                                                        op, comm, module);
            }
        } else if (total_dsize <= 16777216) {
            if (((subc->smsc_use_sr_buf != 0) || (subc->smsc_buf_size > 2 * total_dsize)) && (subc->without_smsc != 1) && is_opt) {
                mca_coll_acoll_reduce_smsc_h(sbuf, rbuf, count, dtype, op, comm, module, subc);
                return mca_coll_acoll_bcast(rbuf, count, dtype, 0, comm, module);
            } else {
                return ompi_coll_base_allreduce_intra_redscat_allgather(sbuf, rbuf, count, dtype,
                                                                        op, comm, module);
            }
        } else {
            if (((subc->smsc_use_sr_buf != 0) || (subc->smsc_buf_size > 2 * total_dsize)) && (subc->without_smsc != 1) && is_opt) {
                return mca_coll_acoll_allreduce_smsc_f(sbuf, rbuf, count, dtype, op, comm, module, subc);
            } else {
                return ompi_coll_base_allreduce_intra_redscat_allgather(sbuf, rbuf, count, dtype,
                                                                        op, comm, module);
            }
        }

    } else {
        return ompi_coll_base_allreduce_intra_redscat_allgather(sbuf, rbuf, count, dtype, op, comm,
                                                                module);
    }
    return MPI_SUCCESS;
}
