.. This file is included by building-open-mpi.rst

MPI functionality
^^^^^^^^^^^^^^^^^

The following are command line options to set the default for various
MPI API behaviors that can be used with ``configure``:

* ``--with-mpi-param-check[=VALUE]``:
  Whether or not to check MPI function parameters for errors at
  runtime.  The following ``VALUE``\s are permitted:

  * ``always``: MPI function parameters are always checked for errors
  * ``never``: MPI function parameters are never checked for errors
  * ``runtime``: Whether MPI function parameters are checked depends on
    the value of the MCA parameter ``mpi_param_check`` (default: yes).
  * ``yes``: Synonym for "always" (same as ``--with-mpi-param-check``).
  * ``no``: Synonym for "never" (same as ``--without-mpi-param-check``).

  If ``--with-mpi-param`` is not specified, ``runtime`` is the default.

* ``--disable-mpi-thread-multiple``:
  Disable the MPI thread level ``MPI_THREAD_MULTIPLE`` (it is enabled by
  default).

* ``--disable-ft``:
  Disable the User-Level Fault Mitigation (ULFM) support in Open MPI
  (it is enabled by default).

  :ref:`See the ULFM section <ulfm-label>` for more information.

* ``--enable-mpi-java``:
  Enable building of an **EXPERIMENTAL** Java MPI interface (disabled
  by default).  You may also need to specify ``--with-jdk-dir``,
  ``--with-jdk-bindir``, and/or ``--with-jdk-headers``.

  .. warning:: Note that this Java interface is **INCOMPLETE**
     (meaning: it does not support all MPI functionality) and **LIKELY
     TO CHANGE**.  The Open MPI developers would very much like to
     hear your feedback about this interface.

  :ref:`See the Java section <open-mpi-java-label>` for many more
  details.

* ``--enable-mpi-fortran[=VALUE]``:
  By default, Open MPI will attempt to build all 3 Fortran bindings:
  ``mpif.h``, the ``mpi`` module, and the ``mpi_f08`` module.  The following
  ``VALUE``\s are permitted:

  * ``all``: Synonym for ``yes``.
  * ``yes``: Attempt to build all 3 Fortran bindings; skip
    any binding that cannot be built (same as
    ``--enable-mpi-fortran``).
  * ``mpifh``: Only build ``mpif.h`` support.
  * ``usempi``: Only build ``mpif.h`` and ``mpi`` module support.
  * ``usempif08``:  Build ``mpif.h``, ``mpi`` module, and ``mpi_f08``
    module support.
  * ``none``: Synonym for ``no``.
  * ``no``: Do not build any MPI Fortran support (same as
    ``--disable-mpi-fortran``).  This is mutually exclusive
    with building the OpenSHMEM Fortran interface.

* ``--with-mpi-moduledir=DIR``:
  Specify a specific ``DIR`` directory where to install the MPI
  Fortran bindings modulefiles.  By default, Open MPI will install
  Fortran modulefiles into ``$libdir``.

* ``--enable-mpi-ext[=LIST]``:
  Enable Open MPI's non-portable API extensions.  ``LIST`` is a
  comma-delmited list of extensions.  If no ``LIST`` is specified, all
  of the extensions are enabled.

  See the "Open MPI API Extensions" section for more details.

* ``--disable-mpi-io``:
  Disable built-in support for MPI-2 I/O, likely because an
  externally-provided MPI I/O package will be used.

* ``--disable-io-ompio``:
  Disable the ompio MPI-IO component

* ``--enable-sparse-groups``:
  Enable the usage of sparse groups. This would save memory
  significantly especially if you are creating large
  communicators. (Disabled by default)
