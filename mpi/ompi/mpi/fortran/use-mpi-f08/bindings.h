/*
 * Copyright (c) 2019      Research Organization for Information Science
 *                         and Technology (RIST).  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef OMPI_CDESC_BINDINGS_H
#define OMPI_CDESC_BINDINGS_H

#include "ompi_config.h"

#include "ts.h"

#include "mpi.h"

void ompi_bsend_ts(CFI_cdesc_t* x, MPI_Fint *count, MPI_Fint *datatype,
                   MPI_Fint *dest, MPI_Fint *tag, MPI_Fint *comm,
                   MPI_Fint *ierr);

void ompi_bsend_init_ts(CFI_cdesc_t* x, MPI_Fint *count, MPI_Fint *datatype,
                        MPI_Fint *dest, MPI_Fint *tag, MPI_Fint *comm,
                        MPI_Fint *request, MPI_Fint *ierr);

void ompi_buffer_attach_ts(CFI_cdesc_t *x, MPI_Fint *size, MPI_Fint *ierr);

void ompi_ibsend_ts(CFI_cdesc_t* x, MPI_Fint *count, MPI_Fint *datatype,
                    MPI_Fint *dest, MPI_Fint *tag, MPI_Fint *comm,
                    MPI_Fint *request, MPI_Fint *ierr);

void ompi_irecv_ts(CFI_cdesc_t *x, MPI_Fint *count, MPI_Fint *datatype,
                   MPI_Fint *source, MPI_Fint *tag, MPI_Fint *comm,
                   MPI_Fint *request, MPI_Fint *ierr);

void ompi_irsend_ts(CFI_cdesc_t* x, MPI_Fint *count, MPI_Fint *datatype,
                    MPI_Fint *dest, MPI_Fint *tag, MPI_Fint *comm,
                    MPI_Fint *request, MPI_Fint *ierr);

void ompi_isend_ts(CFI_cdesc_t *x, MPI_Fint *count, MPI_Fint *datatype,
                   MPI_Fint *dest, MPI_Fint *tag, MPI_Fint *comm,
                   MPI_Fint *request, MPI_Fint *ierr);

void ompi_issend_ts(CFI_cdesc_t* x, MPI_Fint *count, MPI_Fint *datatype,
                    MPI_Fint *dest, MPI_Fint *tag, MPI_Fint *comm,
                    MPI_Fint *request, MPI_Fint *ierr);

void ompi_recv_ts(CFI_cdesc_t *x, MPI_Fint *count, MPI_Fint *datatype,
                  MPI_Fint *source, MPI_Fint *tag, MPI_Fint *comm,
                  MPI_Fint *status, MPI_Fint *ierr);

void ompi_recv_init_ts(CFI_cdesc_t* x, MPI_Fint *count, MPI_Fint *datatype,
                       MPI_Fint *source, MPI_Fint *tag, MPI_Fint *comm,
                       MPI_Fint *request, MPI_Fint *ierr);

void ompi_rsend_ts(CFI_cdesc_t* x, MPI_Fint *count, MPI_Fint *datatype,
                   MPI_Fint *dest, MPI_Fint *tag, MPI_Fint *comm,
                   MPI_Fint *ierr);

void ompi_rsend_init_ts(CFI_cdesc_t* x, MPI_Fint *count,
                        MPI_Fint *datatype, MPI_Fint *dest,
                        MPI_Fint *tag, MPI_Fint *comm,
                        MPI_Fint *request, MPI_Fint *ierr);

void ompi_send_ts(CFI_cdesc_t* x, MPI_Fint *count, MPI_Fint *datatype,
                  MPI_Fint *dest, MPI_Fint *tag, MPI_Fint *comm, MPI_Fint *ierr);

void ompi_sendrecv_ts(CFI_cdesc_t* x1, MPI_Fint *sendcount, MPI_Fint *sendtype,
                      MPI_Fint *dest, MPI_Fint *sendtag, CFI_cdesc_t* x2,
                      MPI_Fint *recvcount, MPI_Fint *recvtype,
                      MPI_Fint *source, MPI_Fint *recvtag, MPI_Fint *comm,
                      MPI_Fint *status, MPI_Fint *ierr);

void ompi_sendrecv_replace_ts(CFI_cdesc_t* x, MPI_Fint *count, MPI_Fint *datatype,
                              MPI_Fint *dest, MPI_Fint *sendtag,
                              MPI_Fint *source, MPI_Fint *recvtag,
                              MPI_Fint *comm, MPI_Fint *status, MPI_Fint *ierr);

void ompi_send_init_ts(CFI_cdesc_t *x, MPI_Fint *count, MPI_Fint *datatype,
                       MPI_Fint *dest, MPI_Fint *tag, MPI_Fint *comm,
                       MPI_Fint *request, MPI_Fint *ierr);

void ompi_ssend_ts(CFI_cdesc_t* x, MPI_Fint *count, MPI_Fint *datatype,
                   MPI_Fint *dest, MPI_Fint *tag,
                   MPI_Fint *comm, MPI_Fint *ierr);

void ompi_ssend_init_ts(CFI_cdesc_t* x, MPI_Fint *count, MPI_Fint *datatype,
                        MPI_Fint *dest, MPI_Fint *tag,
                        MPI_Fint *comm, MPI_Fint *request, MPI_Fint *ierr);

void ompi_get_address_ts(CFI_cdesc_t *x, MPI_Aint *address, MPI_Fint *ierr);

void ompi_pack_ts(CFI_cdesc_t* x1, MPI_Fint *incount, MPI_Fint *datatype,
                  CFI_cdesc_t* x2, MPI_Fint *outsize, MPI_Fint *position,
                  MPI_Fint *comm, MPI_Fint *ierr);

void ompi_pack_external_ts(char *datarep, CFI_cdesc_t* x1, MPI_Fint *incount,
                           MPI_Fint *datatype, CFI_cdesc_t* x2,
                           MPI_Aint *outsize, MPI_Aint *position,
                           MPI_Fint *ierr, int datarep_len);

void ompi_unpack_ts(CFI_cdesc_t* x1, MPI_Fint *insize, MPI_Fint *position,
                           CFI_cdesc_t* x2, MPI_Fint *outcount, MPI_Fint *datatype,
                           MPI_Fint *comm, MPI_Fint *ierr);

void ompi_unpack_external_ts(char *datarep, CFI_cdesc_t* x1, MPI_Aint *insize,
                             MPI_Aint *position, CFI_cdesc_t* x2,
                             MPI_Fint *outcount, MPI_Fint *datatype,
                             MPI_Fint *ierr, int datarep_len);

void ompi_allgather_ts(CFI_cdesc_t* x1, MPI_Fint *sendcount, MPI_Fint *sendtype,
                       CFI_cdesc_t* x2, MPI_Fint *recvcount, MPI_Fint *recvtype,
                       MPI_Fint *comm, MPI_Fint *ierr);

void ompi_allgatherv_ts(CFI_cdesc_t *x1, MPI_Fint *sendcount, MPI_Fint *sendtype,
                        CFI_cdesc_t *x2, MPI_Fint *recvcounts, MPI_Fint *displs,
                        MPI_Fint *recvtype, MPI_Fint *comm, MPI_Fint *ierr);

void ompi_allreduce_ts(CFI_cdesc_t *x1, CFI_cdesc_t *x2, MPI_Fint *count,
                       MPI_Fint *datatype, MPI_Fint *op, MPI_Fint *comm,
                       MPI_Fint *ierr);

void ompi_alltoall_ts(CFI_cdesc_t* x1, MPI_Fint *sendcount, MPI_Fint *sendtype,
                      CFI_cdesc_t* x2, MPI_Fint *recvcount, MPI_Fint *recvtype,
                      MPI_Fint *comm, MPI_Fint *ierr);

void ompi_alltoallv_ts(CFI_cdesc_t *x1, MPI_Fint *sendcounts, MPI_Fint *sdispls,
                       MPI_Fint *sendtype, CFI_cdesc_t *x2, MPI_Fint *recvcounts,
                       MPI_Fint *rdispls, MPI_Fint *recvtype,
                       MPI_Fint *comm, MPI_Fint *ierr);

void ompi_alltoallw_ts(CFI_cdesc_t *x1, MPI_Fint *sendcounts,
                       MPI_Fint *sdispls, MPI_Fint *sendtypes,
                       CFI_cdesc_t *x2, MPI_Fint *recvcounts,
                       MPI_Fint *rdispls, MPI_Fint *recvtypes,
                       MPI_Fint *comm, MPI_Fint *ierr);

void ompi_bcast_ts(CFI_cdesc_t* x, MPI_Fint *count, MPI_Fint *datatype,
                   MPI_Fint *root, MPI_Fint *comm, MPI_Fint *ierr);

void ompi_exscan_ts(CFI_cdesc_t *x1, CFI_cdesc_t *x2, MPI_Fint *count,
                    MPI_Fint *datatype, MPI_Fint *op, MPI_Fint *comm,
                    MPI_Fint *ierr);

void ompi_gather_ts(CFI_cdesc_t *x1, MPI_Fint *sendcount, MPI_Fint *sendtype,
                    CFI_cdesc_t *x2, MPI_Fint *recvcount, MPI_Fint *recvtype,
                    MPI_Fint *root, MPI_Fint *comm, MPI_Fint *ierr);

void ompi_gatherv_ts(CFI_cdesc_t *x1, MPI_Fint *sendcount, MPI_Fint *sendtype,
                     CFI_cdesc_t *x2, MPI_Fint *recvcounts, MPI_Fint *displs,
                     MPI_Fint *recvtype, MPI_Fint *root, MPI_Fint *comm,
                     MPI_Fint *ierr);

void ompi_reduce_ts(CFI_cdesc_t *x1, CFI_cdesc_t *x2, MPI_Fint *count,
                    MPI_Fint *datatype, MPI_Fint *op,
                    MPI_Fint *root, MPI_Fint *comm, MPI_Fint *ierr);

void ompi_reduce_local_ts(CFI_cdesc_t *x1, CFI_cdesc_t *x2, MPI_Fint *count,
                          MPI_Fint *datatype, MPI_Fint *op, MPI_Fint *ierr);

void ompi_reduce_scatter_ts(CFI_cdesc_t *x1, CFI_cdesc_t *x2,
                            MPI_Fint *recvcounts, MPI_Fint *datatype,
                            MPI_Fint *op, MPI_Fint *comm, MPI_Fint *ierr);

void ompi_reduce_scatter_block_ts(CFI_cdesc_t *x1, CFI_cdesc_t *x2,
                                  MPI_Fint *recvcount, MPI_Fint *datatype,
                                  MPI_Fint *op, MPI_Fint *comm, MPI_Fint *ierr);

void ompi_scan_ts(CFI_cdesc_t *x1, CFI_cdesc_t *x2, MPI_Fint *count,
                  MPI_Fint *datatype, MPI_Fint *op, MPI_Fint *comm,
                  MPI_Fint *ierr);

void ompi_scatter_ts(CFI_cdesc_t *x1, MPI_Fint *sendcount,
                     MPI_Fint *sendtype, CFI_cdesc_t *x2,
                     MPI_Fint *recvcount, MPI_Fint *recvtype,
                     MPI_Fint *root, MPI_Fint *comm, MPI_Fint *ierr);

void ompi_scatterv_ts(CFI_cdesc_t *x1, MPI_Fint *sendcounts,
                      MPI_Fint *displs, MPI_Fint *sendtype,
                      CFI_cdesc_t *x2, MPI_Fint *recvcount,
                      MPI_Fint *recvtype, MPI_Fint *root,
                      MPI_Fint *comm, MPI_Fint *ierr);

void ompi_free_mem_ts(CFI_cdesc_t *x, MPI_Fint *ierr);

void ompi_f_sync_reg_ts(CFI_cdesc_t *x);

void ompi_imrecv_ts(CFI_cdesc_t* x, MPI_Fint *count, MPI_Fint *datatype,
                    MPI_Fint *message, MPI_Fint *request, MPI_Fint *ierr);

void ompi_mrecv_ts(CFI_cdesc_t* x, MPI_Fint *count, MPI_Fint *datatype,
                   MPI_Fint *message, MPI_Fint *status, MPI_Fint *ierr);

void ompi_neighbor_allgather_ts(CFI_cdesc_t *x1, MPI_Fint *sendcount, MPI_Fint *sendtype,
                                CFI_cdesc_t *x2, MPI_Fint *recvcount, MPI_Fint *recvtype,
                                MPI_Fint *comm, MPI_Fint *ierr);

void ompi_neighbor_allgatherv_ts(CFI_cdesc_t *x1, MPI_Fint *sendcount, MPI_Fint *sendtype,
                                 CFI_cdesc_t *x2, MPI_Fint *recvcounts, MPI_Fint *displs,
                                 MPI_Fint *recvtype, MPI_Fint *comm, MPI_Fint *ierr);

void ompi_neighbor_alltoall_ts(CFI_cdesc_t *x1, MPI_Fint *sendcount, MPI_Fint *sendtype,
                               CFI_cdesc_t *x2, MPI_Fint *recvcount, MPI_Fint *recvtype,
                               MPI_Fint *comm, MPI_Fint *ierr);

void ompi_neighbor_alltoallv_ts(CFI_cdesc_t *x1, MPI_Fint *sendcounts, MPI_Fint *sdispls,
                                MPI_Fint *sendtype, CFI_cdesc_t *x2, MPI_Fint *recvcounts,
                                MPI_Fint *rdispls, MPI_Fint *recvtype,
                                MPI_Fint *comm, MPI_Fint *ierr);

void ompi_neighbor_alltoallw_ts(CFI_cdesc_t *x1, MPI_Fint *sendcounts,
                                MPI_Aint *sdispls, MPI_Fint *sendtypes,
                                CFI_cdesc_t *x2, MPI_Fint *recvcounts,
                                MPI_Aint *rdispls, MPI_Fint *recvtypes,
                                MPI_Fint *comm, MPI_Fint *ierr);

#endif /* OMPI_CDESC_BINDINGS_H */
