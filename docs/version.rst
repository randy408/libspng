.. _version:

Versioning
==========

libspng uses `semantic versioning <https://semver.org/>`_. All releases until
1.0.0 may introduce breaking changes.


Macros
------

.. c:macro:: SPNG_VERSION_MAJOR

    libspng version's major number

.. c:macro:: SPNG_VERSION_MINOR

    libspng version's minor number

.. c:macro:: SPNG_VERSION_PATCH

    libspng version's patch number


Functions
---------

.. c:function:: const char *spng_version_string(void)

    Returns the library version as a string.
