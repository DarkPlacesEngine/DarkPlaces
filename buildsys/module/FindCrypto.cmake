#
# Try to find d0_blind_id and d0_blind_id_rijndael libraries and include paths
# Once done this will define
#
# CRYPTO_FOUND
# CRYPTO_INCLUDE_DIRS
# CRYPTO_LIBRARIES
#

find_path(CRYPTO_INCLUDE_DIR d0_blind_id/d0_blind_id.h d0_blind_id/d0_rijndael.h d0_blind_id/d0.h)

find_library(d0_blind_id_LIBRARY NAMES d0_blind_id)
find_library(d0_rijndael_LIBRARY NAMES d0_rijndael)

set(CRYPTO_LIBRARIES ${d0_blind_id_LIBRARY} ${d0_rijndael_LIBRARY})

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(Crypto DEFAULT_MSG CRYPTO_LIBRARIES CRYPTO_INCLUDE_DIR)

mark_as_advanced(CRYPTO_INCLUDE_DIR CRYPTO_LIBRARIES)
