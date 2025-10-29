/*
 * Copyright (c) 2004-2005 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2005 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart,
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2011-2012 Cisco Systems, Inc.  All rights reserved.
 * Copyright (c) 2015      Research Organization for Information Science
 *                         and Technology (RIST). All rights reserved.
 * Copyright (c) 2024      Triad National Security, LLC. All rights
 *                         reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */


#include "ompi_config.h"

#include "ompi/datatype/ompi_datatype.h"
#include "ompi/mpi/fortran/base/fint_2_int.h"

void ompi_type_get_envelope_f_c(MPI_Fint *type, MPI_Count *num_integers,
			     MPI_Count *num_addresses,
			     MPI_Count *num_large_counts,
			     MPI_Count *num_datatypes, MPI_Fint *combiner,
			     MPI_Fint *ierr);
void ompi_type_get_envelope_f_c(MPI_Fint *type, MPI_Count *num_integers,
			     MPI_Count *num_addresses,
			     MPI_Count *num_large_counts,
			     MPI_Count *num_datatypes, MPI_Fint *combiner,
			     MPI_Fint *ierr)
{
    int c_ierr;
    MPI_Datatype c_type = PMPI_Type_f2c(*type);
    int c_combiner;

    c_ierr = PMPI_Type_get_envelope_c(c_type,
                                     num_integers,
                                     num_addresses,
                                     num_large_counts,
                                     num_datatypes,
                                     &c_combiner);
    if (NULL != ierr) *ierr = OMPI_INT_2_FINT(c_ierr);

    if (MPI_SUCCESS == c_ierr) {
        *combiner = (MPI_Fint)(c_combiner);
    }
}
