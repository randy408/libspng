.. _version:

Versioning
==========

Releases follow the semantic versioning scheme with a few exceptions:

* Releases from 0.4.0 to 0.8.x are stable
* 0.8.x will be maintained as a separate release branch from 1.0.0

Note that 1.0.0 will introduce little to no breaking changes.

API/ABI changes can be tracked `here <https://abi-laboratory.pro/index.php?view=timeline&l=libspng>`_.

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
