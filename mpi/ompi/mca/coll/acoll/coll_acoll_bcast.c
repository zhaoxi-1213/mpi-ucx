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
#include "ompi/constants.h"
#include "ompi/datatype/ompi_datatype.h"
#include "ompi/mca/coll/base/coll_base_functions.h"
#include "ompi/mca/coll/base/coll_tags.h"
#include "ompi/mca/coll/coll.h"
#include "ompi/mca/pml/pml.h"
#include "opal/util/bit_ops.h"
#include "coll_acoll.h"
#include "coll_acoll_utils.h"

typedef int (*bcast_subc_func)(void *buff, size_t count, struct ompi_datatype_t *datatype, int root,
                               struct ompi_communicator_t *comm, ompi_request_t **preq, int *nreqs,
                               int world_rank);
int mca_coll_acoll_bcast_shm(void *buff, size_t count, struct ompi_datatype_t *dtype, int root,
                             struct ompi_communicator_t *comm, mca_coll_base_module_t *module);

/*
 * bcast_binomial
 *
 * Function:    Broadcast operation using balanced binomial tree
 *
 * Description: Core logic of implementation is derived from that in
 *              "basic" component.
 */
static int bcast_binomial(void *buff, size_t count, struct ompi_datatype_t *datatype, int root,
                          struct ompi_communicator_t *comm, ompi_request_t **preq, int *nreqs,
                          int world_rank)
{
    int msb_pos, sub_rank, peer, err = MPI_SUCCESS;
    int size, rank, dim;
    int i, mask;

    size = ompi_comm_size(comm);
    rank = ompi_comm_rank(comm);
    dim = comm->c_cube_dim;
    sub_rank = (rank - root + size) % size;

    msb_pos = opal_hibit(sub_rank, dim);
    --dim;

    /* Receive data from parent in the subgroup tree. */
    if (sub_rank > 0) {
        assert(msb_pos >= 0);
        peer = ((sub_rank & ~(1 << msb_pos)) + root) % size;

        err = MCA_PML_CALL(
            recv(buff, count, datatype, peer, MCA_COLL_BASE_TAG_BCAST, comm, MPI_STATUS_IGNORE));
        if (MPI_SUCCESS != err) {
            return err;
        }
    }

    for (i = msb_pos + 1, mask = 1 << i; i <= dim; ++i, mask <<= 1) {
        peer = sub_rank | mask;
        if (peer < size) {
            peer = (peer + root) % size;
            *nreqs = *nreqs + 1;

            err = MCA_PML_CALL(isend(buff, count, datatype, peer, MCA_COLL_BASE_TAG_BCAST,
                                     MCA_PML_BASE_SEND_STANDARD, comm, preq++));
            if (MPI_SUCCESS != err) {
                return err;
            }
        }
    }

    return err;
}

static int bcast_flat_tree(void *buff, size_t count, struct ompi_datatype_t *datatype, int root,
                           struct ompi_communicator_t *comm, ompi_request_t **preq, int *nreqs,
                           int world_rank)
{
    int peer;
    int err = MPI_SUCCESS;
    int rank = ompi_comm_rank(comm);
    int size = ompi_comm_size(comm);

    if (rank == root) {
        for (peer = 0; peer < size; peer++) {
            if (peer == root) {
                continue;
            }
            *nreqs = *nreqs + 1;
            err = MCA_PML_CALL(isend(buff, count, datatype, peer, MCA_COLL_BASE_TAG_BCAST,
                                     MCA_PML_BASE_SEND_STANDARD, comm, preq++));
            if (MPI_SUCCESS != err) {
                return err;
            }
        }
    } else {
        err = MCA_PML_CALL(
            recv(buff, count, datatype, root, MCA_COLL_BASE_TAG_BCAST, comm, MPI_STATUS_IGNORE));
        if (MPI_SUCCESS != err) {
            return err;
        }
    }

    return err;
}

/*
 * coll_bcast_decision_fixed
 *
 * Function:    Choose optimal broadcast algorithm
 *
 * Description: Based on no. of processes and message size, chooses [log|lin]
 *              broadcast and subgroup size to be used.
 *
 */

#define SET_BCAST_PARAMS(l0, l1, l2) \
    *lin_0 = l0;                     \
    *lin_1 = l1;                     \
    *lin_2 = l2;

static inline void coll_bcast_decision_fixed(int size, size_t total_dsize, int node_size,
                                             int *sg_cnt, int *use_0, int *use_numa,
                                             int *use_socket, int *use_shm, int *lin_0,
                                             int *lin_1, int *lin_2, int num_nodes,
                                             mca_coll_acoll_module_t *acoll_module,
                                             coll_acoll_subcomms_t *subc)
{
    int sg_size = *sg_cnt;
    *use_0 = 0;
    *lin_0 = 0;
    *use_numa = 0;
    *use_socket = 0;
    *use_shm = 0;
    if (size <= node_size) {
        if (total_dsize <= 8192 && size >= 16 && !acoll_module->disable_shmbcast) {
            *use_shm = 1;
            return;
        }
        if (acoll_module->use_dyn_rules) {
            *sg_cnt = (acoll_module->mnode_sg_size == acoll_module->sg_cnt) ? acoll_module->sg_cnt : node_size;
            *use_0 = 0;
            SET_BCAST_PARAMS(acoll_module->use_lin0, acoll_module->use_lin1, acoll_module->use_lin2)
        } else if (size <= sg_size) {
            *sg_cnt = sg_size;
            if (total_dsize <= 8192) {
                SET_BCAST_PARAMS(0, 0, 0)
            } else {
                SET_BCAST_PARAMS(0, 1, 1)
            }
        } else if (size <= (sg_size << 1)) {
            if (total_dsize <= 1024) {
                *sg_cnt = size;
                SET_BCAST_PARAMS(0, 0, 0)
            } else if (total_dsize <= 8192) {
                *sg_cnt = sg_size;
                SET_BCAST_PARAMS(0, 0, 0)
            } else if (total_dsize <= 2097152) {
                *sg_cnt = size;
                SET_BCAST_PARAMS(0, 1, 1)
            } else {
                *sg_cnt = sg_size;
                SET_BCAST_PARAMS(0, 0, 0)
            }
        } else if (size <= (sg_size << 2)) {
            if (total_dsize <= 1024) {
                *sg_cnt = size;
                SET_BCAST_PARAMS(0, 0, 0)
            } else if (total_dsize <= 8192) {
                *sg_cnt = sg_size;
                SET_BCAST_PARAMS(0, 0, 0)
            } else if (total_dsize <= 32768) {
                *sg_cnt = sg_size;
                SET_BCAST_PARAMS(0, 1, 1)
            } else if (total_dsize <= 4194304) {
                *sg_cnt = size;
                SET_BCAST_PARAMS(0, 1, 1)
            } else {
                *sg_cnt = sg_size;
                SET_BCAST_PARAMS(0, 0, 0)
            }
        } else if (size <= (sg_size << 3)) {
            if (total_dsize <= 1024) {
                *sg_cnt = size;
                SET_BCAST_PARAMS(0, 0, 0)
            } else if (total_dsize <= 8192) {
                *sg_cnt = sg_size;
                SET_BCAST_PARAMS(0, 0, 0)
            } else if (total_dsize <= 262144) {
                *sg_cnt = sg_size;
                SET_BCAST_PARAMS(0, 1, 1)
            } else {
                *sg_cnt = size;
                SET_BCAST_PARAMS(0, 1, 1)
            }
        } else if (size <= (sg_size << 4)) {
            if (total_dsize <= 512) {
                *sg_cnt = size;
                SET_BCAST_PARAMS(0, 0, 0)
            } else if (total_dsize <= 8192) {
                *sg_cnt = sg_size;
                SET_BCAST_PARAMS(0, 0, 0)
            } else if (total_dsize <= 262144) {
                *sg_cnt = sg_size;
                SET_BCAST_PARAMS(0, 1, 1)
            } else {
                *sg_cnt = size;
                SET_BCAST_PARAMS(0, 1, 1)
            }
        } else {
            if (total_dsize <= 512) {
                *sg_cnt = size;
                SET_BCAST_PARAMS(0, 0, 0)
            } else if (total_dsize <= 8192) {
                *sg_cnt = sg_size;
                SET_BCAST_PARAMS(0, 0, 0)
            } else if (total_dsize <= 262144) {
                *sg_cnt = sg_size;
                SET_BCAST_PARAMS(0, 1, 1)
            } else if (total_dsize <= 16777216) {
                *sg_cnt = size;
                SET_BCAST_PARAMS(0, 1, 1)
            } else {
                *sg_cnt = sg_size;
                *use_numa = 1;
                SET_BCAST_PARAMS(0, 1, 1)
            }
        }
    } else {
        if (acoll_module->use_dyn_rules) {
            *sg_cnt = (acoll_module->mnode_sg_size == acoll_module->sg_cnt) ? acoll_module->sg_cnt : node_size;
            *use_0 = acoll_module->use_mnode;
            SET_BCAST_PARAMS(acoll_module->use_lin0, acoll_module->use_lin1, acoll_module->use_lin2)
        } else {
            *use_0 = 1;
            *sg_cnt = sg_size;
            if (2 == num_nodes) {
                SET_BCAST_PARAMS(1, 1, 1)
                if (total_dsize <= 8192) {
                    *use_shm = 1;
                } else {
                    *use_socket = 1;
                    *use_numa = (total_dsize <= 2097152) ? 0 : 1;
                }
            } else if (num_nodes <= 4) {
                if (total_dsize <= 64) {
                    *use_socket = 1;
                    SET_BCAST_PARAMS(1, 1, 0)
                } else if (total_dsize <= 512) {
                    *use_shm = 1;
                    SET_BCAST_PARAMS(1, 1, 0)
                } else if (total_dsize <= 2097152) {
                    *use_socket = 1;
                    SET_BCAST_PARAMS(1, 1, 1)
                } else {
                    *use_numa = 1;
                    *use_socket = (total_dsize <= 4194304) ? 0 : 1;
                    SET_BCAST_PARAMS(1, 1, 1)
                }
            } else if (num_nodes <= 6) {
                SET_BCAST_PARAMS(1, 1, 1)
                if (total_dsize <= 4096) {
                    *use_shm = 1;
                } else if (total_dsize <= 524288) {
                    *use_socket = 1;
                } else {
                    *use_numa = 1;
                }
            } else if (num_nodes <= 8) {
                SET_BCAST_PARAMS(1, 1, 1)
                if (total_dsize <= 8192) {
                    *use_shm = 1;
                } else {
                    *use_numa = 1;
                }
            } else if (num_nodes <= 10) {
                *use_numa = 1;
                if (total_dsize <= 32768) {
                    SET_BCAST_PARAMS(1, 1, 0)
                } else {
                    SET_BCAST_PARAMS(1, 1, 1)
                }
            } else {
                *use_numa = 1;
                if (total_dsize <= 64) {
                    SET_BCAST_PARAMS(1, 0, 1)
                } else if (total_dsize <= 2097152) {
                    SET_BCAST_PARAMS(1, 1, 1)
                } else {
                    *use_socket = 1;
                    SET_BCAST_PARAMS(0, 1, 1)
                }
            }
        }
    }
    if (-1 != acoll_module->force_numa) {
        *use_numa = acoll_module->force_numa;
        if (acoll_module->force_numa) {
            *sg_cnt = sg_size;
        }
    }
    if (-1 != acoll_module->use_socket) {
        *use_socket = acoll_module->use_socket;
    }
    if (1 == acoll_module->disable_shmbcast) {
        *use_shm = 0;
    }
}

static inline void coll_acoll_bcast_subcomms(struct ompi_communicator_t *comm,
                                             coll_acoll_subcomms_t *subc,
                                             struct ompi_communicator_t **subcomms, int *subc_roots,
                                             int root, int num_nodes, int use_0, int no_sg,
                                             int use_numa, int use_socket)
{
    int lyr_id = use_socket ? MCA_COLL_ACOLL_LYR_SOCKET : MCA_COLL_ACOLL_LYR_NODE;
    /* Node leaders */
    if (use_0) {
        subcomms[MCA_COLL_ACOLL_NODE_L] = subc->leader_comm;
        subc_roots[MCA_COLL_ACOLL_NODE_L] = subc->outer_grp_root;
    }
    /* Socket leaders */
    if (use_socket) {
        subcomms[MCA_COLL_ACOLL_NODE_L] = subc->socket_ldr_comm;
        subc_roots[MCA_COLL_ACOLL_NODE_L] = subc->socket_ldr_root;
    }
    /* Intra comm */
    if (((num_nodes > 1) && use_0) || use_socket) {
        int is_root = use_socket ? subc->is_root_socket : subc->is_root_node;
        subc_roots[MCA_COLL_ACOLL_INTRA] = is_root ? subc->local_root[lyr_id] : 0;
        subcomms[MCA_COLL_ACOLL_INTRA] = use_socket ? subc->socket_comm : subc->local_comm;
    } else {
        subc_roots[MCA_COLL_ACOLL_INTRA] = root;
        subcomms[MCA_COLL_ACOLL_INTRA] = comm;
    }
    /* Base ranks comm */
    int parent = lyr_id;
    if (no_sg) {
        subcomms[MCA_COLL_ACOLL_L3_L] = subcomms[MCA_COLL_ACOLL_INTRA];
        subc_roots[MCA_COLL_ACOLL_L3_L] = subc_roots[MCA_COLL_ACOLL_INTRA];
    } else {
        subcomms[MCA_COLL_ACOLL_L3_L] = subc->base_comm[MCA_COLL_ACOLL_L3CACHE]
                                                       [parent];
        subc_roots[MCA_COLL_ACOLL_L3_L] = subc->base_root[MCA_COLL_ACOLL_L3CACHE]
                                                         [parent];
    }
    /* Subgroup comm */
    subcomms[MCA_COLL_ACOLL_LEAF] = subc->subgrp_comm;
    subc_roots[MCA_COLL_ACOLL_LEAF] = subc->subgrp_root;

    /* Override with numa when needed */
    if (use_numa) {
        subcomms[MCA_COLL_ACOLL_L3_L] = subc->base_comm[MCA_COLL_ACOLL_NUMA]
                                                       [parent];
        subc_roots[MCA_COLL_ACOLL_L3_L] = subc->base_root[MCA_COLL_ACOLL_NUMA]
                                                         [parent];
        subcomms[MCA_COLL_ACOLL_LEAF] = subc->numa_comm;
        subc_roots[MCA_COLL_ACOLL_LEAF] = subc->numa_root;
    }
}

static int mca_coll_acoll_bcast_intra_node(void *buff, size_t count, struct ompi_datatype_t *datatype,
                                           mca_coll_base_module_t *module,
                                           coll_acoll_subcomms_t *subc,
                                           struct ompi_communicator_t **subcomms, int *subc_roots,
                                           int lin_1, int lin_2, int no_sg, int use_numa,
                                           int use_socket, int use_shm, int world_rank)
{
    int size;
    int rank;
    int err;
    int subgrp_size;
    int is_base = 0;
    int nreqs;
    ompi_request_t **preq, **reqs;
    struct ompi_communicator_t *comm = subcomms[MCA_COLL_ACOLL_INTRA];
    bcast_subc_func bcast_intra[2] = {&bcast_binomial, &bcast_flat_tree};

    rank = ompi_comm_rank(comm);
    size = ompi_comm_size(comm);

    if (use_shm && subc_roots[MCA_COLL_ACOLL_INTRA] == 0 && !use_socket) {
        return mca_coll_acoll_bcast_shm(buff, count, datatype, 0, comm, module);
    }
    reqs = ompi_coll_base_comm_get_reqs(module->base_data, size);
    if (NULL == reqs) {
        return OMPI_ERR_OUT_OF_RESOURCE;
    }
    nreqs = 0;
    preq = reqs;
    err = MPI_SUCCESS;
    if (no_sg) {
        is_base = 1;
    } else {
        int ind1 = use_numa ? MCA_COLL_ACOLL_NUMA : MCA_COLL_ACOLL_L3CACHE;
        int ind2 = use_socket ? MCA_COLL_ACOLL_LYR_SOCKET : MCA_COLL_ACOLL_LYR_NODE;
        is_base = rank == subc->base_rank[ind1][ind2] ? 1 : 0;
    }

    /* All base ranks receive from root */
    if (is_base) {
        err = bcast_intra[lin_1](buff, count, datatype, subc_roots[MCA_COLL_ACOLL_L3_L],
                                 subcomms[MCA_COLL_ACOLL_L3_L], preq, &nreqs, world_rank);
        if (MPI_SUCCESS != err) {
            ompi_coll_base_free_reqs(reqs, nreqs);
            return err;
        }
    }

    /* Start and wait on all requests. */
    if (nreqs > 0) {
        err = ompi_request_wait_all(nreqs, reqs, MPI_STATUSES_IGNORE);
        if (MPI_SUCCESS != err) {
            ompi_coll_base_free_reqs(reqs, nreqs);
        }
    }

    /* If single stage, return */
    if (no_sg) {
        ompi_coll_base_free_reqs(reqs, nreqs);
        return err;
    }

    subgrp_size = use_numa ? ompi_comm_size(subc->numa_comm) : subc->subgrp_size;
    /* All leaf ranks receive from the respective base rank */
    if ((subgrp_size > 1) && !no_sg) {
        err = bcast_intra[lin_2](buff, count, datatype, subc_roots[MCA_COLL_ACOLL_LEAF],
                                 subcomms[MCA_COLL_ACOLL_LEAF], preq, &nreqs, world_rank);
    }

    /* Start and wait on all requests. */
    if (nreqs > 0) {
        err = ompi_request_wait_all(nreqs, reqs, MPI_STATUSES_IGNORE);
        if (MPI_SUCCESS != err) {
            ompi_coll_base_free_reqs(reqs, nreqs);
        }
    }

    /* All done */
    ompi_coll_base_free_reqs(reqs, nreqs);
    return err;
}

/*
 * mca_coll_acoll_bcast_shm
 *
 * Function:    Broadcast operation for small messages ( <= 8K) using shared memory
 * Accepts:     Same arguments as MPI_Bcast()
 * Returns:     MPI_SUCCESS or error code
 *
 * Description: Broadcast is performed across and within subgroups.
 *
 * Memory:      Additional memory is allocated for group leaders
 *              (around 2MB for comm size of 256).
 */
// 0) all flags are initialized to 0 and increment with each bcast call
// 1) root sets the ready flag and waits for
//  - all "done" from l2 members
//  - all "done" from its l1 members
// 2) l2 members wait on root's ready flag
// 3) l1 members wait on l1 leader's ready flag

int mca_coll_acoll_bcast_shm(void *buff, size_t count, struct ompi_datatype_t *dtype, int root,
                             struct ompi_communicator_t *comm, mca_coll_base_module_t *module)
{
    size_t dsize;
    int err = MPI_SUCCESS;
    int rank = ompi_comm_rank(comm);
    int size = ompi_comm_size(comm);
    mca_coll_acoll_module_t *acoll_module = (mca_coll_acoll_module_t *) module;
    coll_acoll_subcomms_t *subc = NULL;

    err = check_and_create_subc(comm, acoll_module, &subc);
    if (!subc->initialized) {
       err = mca_coll_acoll_comm_split_init(comm, acoll_module, subc, root);
        if (MPI_SUCCESS != err) {
            return err;
        }
    }
    coll_acoll_init(module, comm, subc->data, subc, root);
    coll_acoll_data_t *data = subc->data;

    if (NULL == data) {
        return -1;
    }
    ompi_datatype_type_size(dtype, &dsize);

    int l1_gp_size = data->l1_gp_size;
    int *l1_gp = data->l1_gp;
    int *l2_gp = data->l2_gp;
    int l2_gp_size = data->l2_gp_size;
    /* 16 * 1024 + 2 * 64 * size + 8 * 1024 * size */
    int offset_bcast = LEADER_SHM_SIZE + 2*CACHE_LINE_SIZE*size + PER_RANK_SHM_SIZE*size; 

    volatile int *leader_shm;
    if (rank == l1_gp[0]) {
        leader_shm = (int *) ((char *) data->allshmmmap_sbuf[root] + offset_bcast + CACHE_LINE_SIZE * root);
    } else {
        leader_shm = (int *) ((char *) data->allshmmmap_sbuf[l1_gp[0]] + offset_bcast
                              + CACHE_LINE_SIZE * l1_gp[0]);
    }

    /*
     * 0) all flags are initialized to 0 and increment with each bcast call
     * 1) root sets the ready flag and waits for
     *  - all "done" from l2 members
     *  - all "done" from its l1 members
     * 2) l2 members wait on root's ready flag
     *  - copy data from root to its buffer
     *  - increment its ready flag
     *  - wait for all l1 members to finish
     * 3) l1 members wait on l1 leader's ready flag
     *  - copy data from l1 leader's buffer to its buffer
     *  - increment its ready flag
     */
    int ready;
    if (rank == root) {
        memcpy((char *) data->allshmmmap_sbuf[root], buff, count * dsize);
        ready = __atomic_load_n(leader_shm, __ATOMIC_RELAXED); // we don't need atomic hear!
        ready++;
        __atomic_store_n(leader_shm, ready, __ATOMIC_RELAXED);
        for (int i = 0; i < l2_gp_size; i++) {
            if (l2_gp[i] == root)
                continue;
            volatile int *val = (int *) ((char *) data->allshmmmap_sbuf[root] + offset_bcast
                                         + CACHE_LINE_SIZE * l2_gp[i]);
            while (*val != ready) {
                ;
            }
        }
        for (int i = 0; i < l1_gp_size; i++) {
            if (l1_gp[i] == root)
                continue;
            volatile int *val = (int *) ((char *) data->allshmmmap_sbuf[root] + offset_bcast
                                         + CACHE_LINE_SIZE * l1_gp[i]);
            while (*val != ready) {
                ;
            }
        }
    } else if (rank == l1_gp[0]) {
        volatile int leader_ready = __atomic_load_n(leader_shm, __ATOMIC_RELAXED);
        int done = __atomic_load_n((int *) ((char *) data->allshmmmap_sbuf[root] + offset_bcast
                                            + CACHE_LINE_SIZE * rank),
                                   __ATOMIC_RELAXED);
        while (done == leader_ready) {
            leader_ready = __atomic_load_n(leader_shm, __ATOMIC_RELAXED);
        }
        memcpy(buff, (char *) data->allshmmmap_sbuf[root], count * dsize);
        memcpy((char *) data->allshmmmap_sbuf[rank], (char *) data->allshmmmap_sbuf[root],
               count * dsize);
        int val = __atomic_load_n((int *) ((char *) data->allshmmmap_sbuf[rank] + offset_bcast
                                           + CACHE_LINE_SIZE * rank),
                                  __ATOMIC_RELAXED); // do we need atomic load?
        val++;
        int local_val = val;
        __atomic_store_n((int *) ((char *) data->allshmmmap_sbuf[root] + offset_bcast + CACHE_LINE_SIZE * rank),
                         val, __ATOMIC_RELAXED); // do we need atomic store?
        __atomic_store_n((int *) ((char *) data->allshmmmap_sbuf[rank] + offset_bcast + CACHE_LINE_SIZE * rank),
                         val, __ATOMIC_RELAXED); // do we need atomic store?
        // do we need wmb() here?
        for (int i = 0; i < l1_gp_size; i++) {
            if (l1_gp[i] == l1_gp[0])
                continue;
            volatile int *vali = (int *) ((char *) data->allshmmmap_sbuf[l1_gp[0]] + offset_bcast
                                          + CACHE_LINE_SIZE * l1_gp[i]); // do we need atomic_load here?
            while (*vali != local_val) {
                ; // can we use a more specific condition than "!=" ?
            }
        }
    } else {
        int done = __atomic_load_n((int *) ((char *) data->allshmmmap_sbuf[l1_gp[0]] + offset_bcast
                                            + CACHE_LINE_SIZE * rank),
                                   __ATOMIC_RELAXED);
        while (done == *leader_shm) {
            ;
        }
        memcpy(buff, (char *) data->allshmmmap_sbuf[l1_gp[0]], count * dsize);
        int val = __atomic_load_n((int *) ((char *) data->allshmmmap_sbuf[l1_gp[0]] + offset_bcast
                                           + CACHE_LINE_SIZE * rank),
                                  __ATOMIC_RELAXED); // do we need atomic load?
        val++;
        __atomic_store_n((int *) ((char *) data->allshmmmap_sbuf[l1_gp[0]] + offset_bcast
                                  + CACHE_LINE_SIZE * rank),
                         val, __ATOMIC_RELAXED); // do we need atomic store?
        // do we need wmb() here?
    }
    return err;
}

/*
 * mca_coll_acoll_bcast
 *
 * Function:    Broadcast operation using subgroup based algorithm
 * Accepts:     Same arguments as MPI_Bcast()
 * Returns:     MPI_SUCCESS or error code
 *
 * Description: Broadcast is performed across and within subgroups.
 *              O(N) or O(log(N)) algorithm within sunbgroup based on count.
 *              Subgroups can be 1 or more based on size and count.
 *
 * Limitations: None
 *
 * Memory:      No additional memory requirements beyond user-supplied buffers.
 *
 */
int mca_coll_acoll_bcast(void *buff, size_t count, struct ompi_datatype_t *datatype, int root,
                         struct ompi_communicator_t *comm, mca_coll_base_module_t *module)
{
    int size;
    int rank;
    int err;
    int nreqs;
    ompi_request_t **preq, **reqs;
    int sg_cnt, node_size;
    int num_nodes;
    int use_0 = 0;
    int lin_0 = 0, lin_1 = 0, lin_2 = 0;
    int use_numa = 0, use_socket = 0, use_shm = 0;
    int no_sg;
    size_t total_dsize, dsize;
    mca_coll_acoll_module_t *acoll_module = (mca_coll_acoll_module_t *) module;
    bcast_subc_func bcast_func[2] = {&bcast_binomial, &bcast_flat_tree};
    coll_acoll_subcomms_t *subc = NULL;
    struct ompi_communicator_t *subcomms[MCA_COLL_ACOLL_NUM_SC] = {NULL};
    int subc_roots[MCA_COLL_ACOLL_NUM_SC] = {-1};

    /* For small communicators, use linear bcast */
    size = ompi_comm_size(comm);
    if (size < 8) {
        return ompi_coll_base_bcast_intra_basic_linear(buff, count, datatype, root, comm, module);
    }

    /* Obtain the subcomms structure */
    err = check_and_create_subc(comm, acoll_module, &subc);
    /* Fallback to knomial if subcomms is not obtained */
    if (NULL == subc) {
        return ompi_coll_base_bcast_intra_knomial(buff, count, datatype, root, comm, module, 0, 4);
    }

    /* Fallback to knomial if no. of root changes is beyond a threshold */
    if ((subc->num_root_change > MCA_COLL_ACOLL_ROOT_CHANGE_THRESH)
        && (root != subc->prev_init_root)) {
        return ompi_coll_base_bcast_intra_knomial(buff, count, datatype, root, comm, module, 0, 4);
    }
    if ((!subc->initialized || (root != subc->prev_init_root)) && size > 2) {
        err = mca_coll_acoll_comm_split_init(comm, acoll_module, subc, root);
        if (MPI_SUCCESS != err) {
            return err;
        }
    }

    ompi_datatype_type_size(datatype, &dsize);
    total_dsize = dsize * count;
    rank = ompi_comm_rank(comm);
    sg_cnt = acoll_module->sg_cnt;
    if (size > 2) {
        num_nodes = subc->num_nodes;
        node_size = ompi_comm_size(subc->local_comm);
    } else {
        num_nodes = 1;
        node_size = size;
    }

    /* Use knomial for nodes 8 and above and non-large messages */
    if ((num_nodes >= 8 && total_dsize <= 65536)
        || (1 == num_nodes && size >= 256 && total_dsize < 16384)) {
        return ompi_coll_base_bcast_intra_knomial(buff, count, datatype, root, comm, module, 0, 4);
    }

    /* Determine the algorithm to be used based on size and count */
    /* sg_cnt determines subgroup based communication */
    /* lin_1 and lin_2 indicate whether to use linear or log based
     sends/receives across and within subgroups respectively. */
    coll_bcast_decision_fixed(size, total_dsize, node_size, &sg_cnt, &use_0,
                              &use_numa, &use_socket, &use_shm, &lin_0,
                              &lin_1, &lin_2, num_nodes, acoll_module, subc);
    no_sg = (sg_cnt == node_size) ? 1 : 0;
    if (size <= 2)
        no_sg = 1;

    /* Disable shm based bcast if: */
    /* - datatype is not a predefined type */
    /* - it's a gpu buffer */
    uint64_t flags = 0;
    int dev_id;
    if (!OMPI_COMM_CHECK_ASSERT_NO_ACCEL_BUF(comm)) {
        if (!ompi_datatype_is_predefined(datatype)
            || (0 < opal_accelerator.check_addr(buff, &dev_id, &flags))) {
            use_shm = 0;
        }
    }

    coll_acoll_bcast_subcomms(comm, subc, subcomms, subc_roots, root, num_nodes, use_0, no_sg,
                              use_numa, use_socket);

    reqs = ompi_coll_base_comm_get_reqs(module->base_data, size);
    if (NULL == reqs) {
        return OMPI_ERR_OUT_OF_RESOURCE;
    }
    nreqs = 0;
    preq = reqs;
    err = MPI_SUCCESS;

    if (use_0 || use_socket) {
        if (subc_roots[MCA_COLL_ACOLL_NODE_L] != -1) {
            err = bcast_func[lin_0](buff, count, datatype, subc_roots[MCA_COLL_ACOLL_NODE_L],
                                    subcomms[MCA_COLL_ACOLL_NODE_L], preq, &nreqs, rank);
            if (MPI_SUCCESS != err) {
                ompi_coll_base_free_reqs(reqs, nreqs);
                return err;
            }
        }
    }

    /* Start and wait on all requests. */
    if (nreqs > 0) {
        err = ompi_request_wait_all(nreqs, reqs, MPI_STATUSES_IGNORE);
        if (MPI_SUCCESS != err) {
            ompi_coll_base_free_reqs(reqs, nreqs);
            return err;
        }
    }

    err = mca_coll_acoll_bcast_intra_node(buff, count, datatype, module, subc, subcomms, subc_roots,
                                          lin_1, lin_2, no_sg, use_numa, use_socket, use_shm, rank);

    if (MPI_SUCCESS != err) {
        ompi_coll_base_free_reqs(reqs, nreqs);
        return err;
    }

    /* All done */
    ompi_coll_base_free_reqs(reqs, nreqs);
    return err;
}
