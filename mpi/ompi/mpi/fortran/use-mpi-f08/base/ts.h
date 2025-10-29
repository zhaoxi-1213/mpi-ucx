/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * Copyright (c) 2014      Argonne National Laboratory.
 * Copyright (c) 2019      Research Organization for Information Science
 *                         and Technology (RIST).  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "ompi_config.h"

#include "ompi/datatype/ompi_datatype.h"
#include "ompi/mpi/fortran/base/fint_2_int.h"

#if OMPI_FORTRAN_HAVE_TS

#include <ISO_Fortran_binding.h>

#define OMPI_CFI_BUFFER CFI_cdesc_t

extern int ompi_ts_create_datatype(CFI_cdesc_t *cdesc, int oldcount, MPI_Datatype oldtype, MPI_Datatype *newtype);

extern size_t ompi_ts_size(CFI_cdesc_t *cdesc);

extern int ompi_ts_copy_back(char *buffer, CFI_cdesc_t *cdesc);

extern int ompi_ts_copy(CFI_cdesc_t *cdesc, char *buffer);

#define OMPI_CFI_BASE_ADDR(x) (x)->base_addr

#define OMPI_CFI_2_C(x, count, type, datatype, rc)                      \
    do {                                                                \
        datatype = type;                                                \
        if (x->rank != 0 && !CFI_is_contiguous(x)) {                    \
            rc = ompi_ts_create_datatype(x, count, type, &datatype);    \
            if (OPAL_LIKELY(MPI_SUCCESS == rc)) {                       \
                count = 1;                                              \
            }                                                           \
        } else {                                                        \
            rc = MPI_SUCCESS;                                           \
        }                                                               \
    } while (0)

#define OMPI_CFI_2_C_ALLOC(x, buffer, count, type, datatype, rc)        \
    do {                                                                \
        datatype = type;                                                \
        if (x->rank != 0 && !CFI_is_contiguous(x)) {                    \
            size_t size = ompi_ts_size(x);                              \
            buffer = malloc(size);                                      \
            if (OPAL_UNLIKELY(NULL == buffer)) {                        \
                rc = MPI_ERR_NO_MEM;                                    \
            } else {                                                    \
                rc = MPI_SUCCESS;                                       \
            }                                                           \
        } else {                                                        \
            buffer = x->base_addr;                                      \
            rc = MPI_SUCCESS;                                           \
        }                                                               \
    } while (0)

#define OMPI_CFI_2_C_COPY(x, buffer, count, type, datatype, rc)         \
    do {                                                                \
        datatype = type;                                                \
        if (x->rank != 0 && !CFI_is_contiguous(x)) {                    \
            size_t size = ompi_ts_size(x);                              \
            buffer = malloc(size);                                      \
            if (OPAL_UNLIKELY(NULL == buffer)) {                        \
                rc = MPI_ERR_NO_MEM;                                    \
            } else {                                                    \
                rc = ompi_ts_copy(x, buffer);                           \
            }                                                           \
        } else {                                                        \
            buffer = x->base_addr;                                      \
            rc = MPI_SUCCESS;                                           \
        }                                                               \
    } while (0)

#define OMPI_C_2_CFI_FREE(x, buffer, count, type, datatype, rc)         \
    do {                                                                \
        if (buffer != x->base_addr) {                                   \
            free(buffer);                                               \
        }                                                               \
        if (type != datatype) {                                         \
            rc = PMPI_Type_free(&datatype);                             \
        }                                                               \
    } while (0)

#define OMPI_C_2_CFI_COPY(x, buffer, count, type, datatype, rc)         \
    do {                                                                \
        if (buffer != x->base_addr) {                                   \
            rc = ompi_ts_copy_back(buffer, x);                          \
            free(buffer);                                               \
        }                                                               \
        if (type != datatype) {                                         \
            rc = PMPI_Type_free(&datatype);                             \
        }                                                               \
    } while (0)

#define OMPI_CFI_IS_CONTIGUOUS(x)                                       \
    (0 == x->rank || CFI_is_contiguous(x))

#define OMPI_CFI_CHECK_CONTIGUOUS(x, rc)                                \
    do {                                                                \
        if (OMPI_CFI_IS_CONTIGUOUS(x)) {                                \
            rc = MPI_SUCCESS;                                           \
        } else {                                                        \
            rc = MPI_ERR_INTERN;                                        \
        }                                                               \
    } while (0)

#else

/*
 * Macros for compilers not supporting TS 29113.
 */

#define OMPI_CFI_BUFFER char

#define OMPI_CFI_BASE_ADDR(x) (x)

#define OMPI_CFI_2_C(x, count, type, datatype, rc)                      \
    do {                                                                \
        datatype = type;                                                \
        rc = MPI_SUCCESS;                                               \
    } while (0)

#define OMPI_CFI_2_C_ALLOC(x, buffer, count, type, datatype, rc)        \
    do {                                                                \
        datatype = type;                                                \
        buffer = x;                                                     \
        rc = MPI_SUCCESS;                                               \
    } while (0)

#define OMPI_CFI_2_C_COPY(x, buffer, count, type, datatype, rc)         \
    do {                                                                \
        datatype = type;                                                \
        buffer = x;                                                     \
        rc = MPI_SUCCESS;                                               \
    } while (0)

#define OMPI_C_2_CFI_FREE(x, buffer, count, type, datatype, rc)         \
    do {} while (0)

#define OMPI_C_2_CFI_COPY(x, buffer, count, type, datatype, rc)         \
    do {} while (0)

#define OMPI_CFI_IS_CONTIGUOUS(x) 1

#define OMPI_CFI_CHECK_CONTIGUOUS(x, rc)                                \
    do {                                                                \
        rc = MPI_SUCCESS;                                               \
    } while (0)
#endif /* OMPI_FORTRAN_HAVE_TS */

#define OMPI_COUNT_CONVERT(fcount)
