/*
 * Copyright (c) 2004-2007 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2020 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart,
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2006-2012 Cisco Systems, Inc.  All rights reserved.
 * Copyright (c) 2015-2017 Research Organization for Information Science
 *                         and Technology (RIST). All rights reserved.
 * Copyright (c) 2025      UT-Battelle, LLC.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "ompi_config.h"

#include "ompi/mpi/fortran/mpif-h/bindings.h"

#if OMPI_BUILD_MPI_PROFILING
#if OPAL_HAVE_WEAK_SYMBOLS
#pragma weak PMPI_REMOVE_ERROR_STRING = ompi_remove_error_string_f
#pragma weak pmpi_remove_error_string = ompi_remove_error_string_f
#pragma weak pmpi_remove_error_string_ = ompi_remove_error_string_f
#pragma weak pmpi_remove_error_string__ = ompi_remove_error_string_f

#pragma weak PMPI_Remove_error_string_f = ompi_remove_error_string_f
#pragma weak PMPI_Remove_error_string_f08 = ompi_remove_error_string_f
#else
OMPI_GENERATE_F77_BINDINGS (PMPI_REMOVE_ERROR_STRING,
                           pmpi_remove_error_string,
                           pmpi_remove_error_string_,
                           pmpi_remove_error_string__,
                           pompi_remove_error_string_f,
                           (MPI_Fint *errorcode, MPI_Fint *ierr),
                           (errorcode, ierr) )
#endif
#endif

#if OPAL_HAVE_WEAK_SYMBOLS
#pragma weak MPI_REMOVE_ERROR_STRING = ompi_remove_error_string_f
#pragma weak mpi_remove_error_string = ompi_remove_error_string_f
#pragma weak mpi_remove_error_string_ = ompi_remove_error_string_f
#pragma weak mpi_remove_error_string__ = ompi_remove_error_string_f

#pragma weak MPI_Remove_error_string_f = ompi_remove_error_string_f
#pragma weak MPI_Remove_error_string_f08 = ompi_remove_error_string_f
#else
#if ! OMPI_BUILD_MPI_PROFILING
OMPI_GENERATE_F77_BINDINGS (MPI_REMOVE_ERROR_STRING,
                           mpi_remove_error_string,
                           mpi_remove_error_string_,
                           mpi_remove_error_string__,
                           ompi_remove_error_string_f,
                           (MPI_Fint *errorcode, MPI_Fint *ierr),
                           (errorcode, ierr) )
#else
#define ompi_remove_error_string_f pompi_remove_error_string_f
#endif
#endif


void ompi_remove_error_string_f(MPI_Fint *errorcode, MPI_Fint *ierr)
{
    int ierr_c;

    ierr_c = PMPI_Remove_error_string(OMPI_FINT_2_INT(*errorcode));
    if (NULL != ierr) *ierr = OMPI_INT_2_FINT(ierr_c);
}
