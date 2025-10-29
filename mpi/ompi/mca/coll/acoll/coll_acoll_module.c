/*
 * Copyright (c) 2024 Advanced Micro Devices, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "ompi_config.h"

#include <stdio.h>

#include "mpi.h"
#include "ompi/mca/coll/base/base.h"
#include "ompi/mca/coll/coll.h"
#include "coll_acoll.h"


static int acoll_module_enable(mca_coll_base_module_t *module,  struct ompi_communicator_t *comm);
static int acoll_module_disable(mca_coll_base_module_t *module, struct ompi_communicator_t *comm);

/*
 * Initial query function that is invoked during MPI_INIT, allowing
 * this component to disqualify itself if it doesn't support the
 * required level of thread support.
 */
int mca_coll_acoll_init_query(bool enable_progress_threads, bool enable_mpi_threads)
{
    /* Nothing to do */
    return OMPI_SUCCESS;
}



#define ACOLL_INSTALL_COLL_API(__comm, __module, __api)                                                     \
    do                                                                                                      \
    {                                                                                                       \
        if (__module->super.coll_##__api)                                                                   \
        {                                                                                                   \
            MCA_COLL_INSTALL_API(__comm, __api, __module->super.coll_##__api, &__module->super, "acoll");  \
        }                                                                                                   \
    } while (0)

#define ACOLL_UNINSTALL_COLL_API(__comm, __module, __api)               \
    do                                                                  \
    {                                                                   \
        if (__comm->c_coll->coll_##__api##_module == &__module->super)  \
        {                                                               \
            MCA_COLL_INSTALL_API(__comm, __api, NULL, NULL, "acoll");   \
        }                                                               \
    } while (0)

/*
 * Invoked when there's a new communicator that has been created.
 * Look at the communicator and decide which set of functions and
 * priority we want to return.
 */
mca_coll_base_module_t *mca_coll_acoll_comm_query(struct ompi_communicator_t *comm, int *priority)
{
    mca_coll_acoll_module_t *acoll_module;

    if (OMPI_COMM_IS_INTER(comm)) {
        *priority = 0;
        return NULL;
    }
    if (ompi_comm_size(comm) < mca_coll_acoll_comm_size_thresh) {
        *priority = 0;
        return NULL;
    }

    acoll_module = OBJ_NEW(mca_coll_acoll_module_t);
    if (NULL == acoll_module) {
        return NULL;
    }

    *priority = mca_coll_acoll_priority;

    /* Set topology params */
    acoll_module->max_comms = mca_coll_acoll_max_comms;
    acoll_module->sg_scale = mca_coll_acoll_sg_scale;
    acoll_module->sg_size = mca_coll_acoll_sg_size;
    acoll_module->sg_cnt = mca_coll_acoll_sg_size / mca_coll_acoll_sg_scale;
    acoll_module->node_cnt = mca_coll_acoll_node_size;
    if (mca_coll_acoll_sg_size == MCA_COLL_ACOLL_SG_SIZE_1) {
        assert((acoll_module->sg_cnt == 1) || (acoll_module->sg_cnt == 2)
               || (acoll_module->sg_cnt == 4) || (acoll_module->sg_cnt == 8));
    }
    if (mca_coll_acoll_sg_size == MCA_COLL_ACOLL_SG_SIZE_2) {
        assert((acoll_module->sg_cnt == 1) || (acoll_module->sg_cnt == 2)
               || (acoll_module->sg_cnt == 4) || (acoll_module->sg_cnt == 8)
               || (acoll_module->sg_cnt == 16));
    }

    switch (acoll_module->sg_cnt) {
    case 1:
        acoll_module->log2_sg_cnt = 0;
        break;
    case 2:
        acoll_module->log2_sg_cnt = 1;
        break;
    case 4:
        acoll_module->log2_sg_cnt = 2;
        break;
    case 8:
        acoll_module->log2_sg_cnt = 3;
        break;
    case 16:
        acoll_module->log2_sg_cnt = 4;
        break;
    default:
        assert(0);
        break;
    }

    switch (acoll_module->node_cnt) {
    case 96:
    case 128:
        acoll_module->log2_node_cnt = 7;
        break;
    case 192:
        acoll_module->log2_node_cnt = 8;
        break;
    case 64:
        acoll_module->log2_node_cnt = 6;
        break;
    case 32:
        acoll_module->log2_node_cnt = 5;
        break;
    default:
        assert(0);
        break;
    }

    // Check SMSC availability (currently only for XPMEM)
    if (!mca_smsc_base_has_feature(MCA_SMSC_FEATURE_CAN_MAP)) {
        opal_output_verbose(MCA_BASE_VERBOSE_ERROR, ompi_coll_base_framework.framework_output,
                            "coll:acoll: Error: SMSC's MAP feature is not available. "
                            "SMSC will be disabled for this communicator irrespective of "
                            "the mca parameters.");
        acoll_module->has_smsc = 0;
    } else {
        acoll_module->has_smsc = 1;
    }

    acoll_module->force_numa = mca_coll_acoll_force_numa;
    acoll_module->use_dyn_rules = mca_coll_acoll_use_dynamic_rules;
    acoll_module->disable_shmbcast = mca_coll_acoll_disable_shmbcast;
    acoll_module->use_mnode = mca_coll_acoll_mnode_enable;
    /* Value of 0 is currently unsupported for mnode_enable */
    acoll_module->use_mnode = 1;
    acoll_module->use_lin0 = mca_coll_acoll_bcast_lin0;
    acoll_module->use_lin1 = mca_coll_acoll_bcast_lin1;
    acoll_module->use_lin2 = mca_coll_acoll_bcast_lin2;
    acoll_module->use_socket = mca_coll_acoll_bcast_socket;
    if (mca_coll_acoll_bcast_nonsg) {
        acoll_module->mnode_sg_size = acoll_module->node_cnt;
        acoll_module->mnode_log2_sg_size = acoll_module->log2_node_cnt;
    } else {
        acoll_module->mnode_sg_size = acoll_module->sg_cnt;
        acoll_module->mnode_log2_sg_size = acoll_module->log2_sg_cnt;
    }
    acoll_module->allg_lin = mca_coll_acoll_allgather_lin;
    acoll_module->allg_ring = mca_coll_acoll_allgather_ring_1;

    /* Choose whether to use [intra|inter], and [subgroup|normal]-based
     * algorithms. */
    acoll_module->super.coll_module_enable = acoll_module_enable;
    acoll_module->super.coll_module_disable = acoll_module_disable;

    acoll_module->super.coll_allgather = mca_coll_acoll_allgather;
    acoll_module->super.coll_allreduce = mca_coll_acoll_allreduce_intra;
    acoll_module->super.coll_alltoall = mca_coll_acoll_alltoall;
    acoll_module->super.coll_barrier = mca_coll_acoll_barrier_intra;
    acoll_module->super.coll_bcast = mca_coll_acoll_bcast;
    acoll_module->super.coll_gather = mca_coll_acoll_gather_intra;
    acoll_module->super.coll_reduce = mca_coll_acoll_reduce_intra;

    return &(acoll_module->super);
}

/*
 * Init module on the communicator
 */
static int acoll_module_enable(mca_coll_base_module_t *module, struct ompi_communicator_t *comm)
{
    mca_coll_acoll_module_t *acoll_module = (mca_coll_acoll_module_t *) module;

    /* prepare the placeholder for the array of request* */
    module->base_data = OBJ_NEW(mca_coll_base_comm_t);
    if (NULL == module->base_data) {
        return OMPI_ERROR;
    }

   ACOLL_INSTALL_COLL_API(comm, acoll_module, allgather);
   ACOLL_INSTALL_COLL_API(comm, acoll_module, allreduce);
   ACOLL_INSTALL_COLL_API(comm, acoll_module, alltoall);
   ACOLL_INSTALL_COLL_API(comm, acoll_module, barrier);
   ACOLL_INSTALL_COLL_API(comm, acoll_module, bcast);
   ACOLL_INSTALL_COLL_API(comm, acoll_module, gather);
   ACOLL_INSTALL_COLL_API(comm, acoll_module, reduce);

   /* Initialize k-nomial tree */
    module->base_data->cached_kmtree = NULL;
    module->base_data->cached_kmtree_root = -1;
    module->base_data->cached_kmtree_radix = 4;

    /* All done */
    return OMPI_SUCCESS;
}

static int acoll_module_disable(mca_coll_base_module_t *module, struct ompi_communicator_t *comm)
{
    mca_coll_acoll_module_t *acoll_module = (mca_coll_acoll_module_t *) module;

    ACOLL_UNINSTALL_COLL_API(comm, acoll_module, allgather);
    ACOLL_UNINSTALL_COLL_API(comm, acoll_module, allreduce);
    ACOLL_UNINSTALL_COLL_API(comm, acoll_module, alltoall);
    ACOLL_UNINSTALL_COLL_API(comm, acoll_module, barrier);
    ACOLL_UNINSTALL_COLL_API(comm, acoll_module, bcast);
    ACOLL_UNINSTALL_COLL_API(comm, acoll_module, gather);
    ACOLL_UNINSTALL_COLL_API(comm, acoll_module, reduce);

    return OMPI_SUCCESS;
}
