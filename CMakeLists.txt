cmake_minimum_required(VERSION 2.8.11)

project(ceph)
set(VERSION 12.2.2)

if(POLICY CMP0046)
  # Tweak policies (this one disables "missing" dependency warning)
  cmake_policy(SET CMP0046 OLD)
endif()
# we use LINK_PRIVATE keyword instead of PRIVATE, but do not specify the LINK_PUBLIC
# for target_link_libraries() command when PUBLIC should be used instead, it's just
# for backward compatibility with cmake 2.8.11.
if (POLICY CMP0022)
cmake_policy(SET CMP0022 OLD)
endif (POLICY CMP0022)
if (POLICY CMP0023)
cmake_policy(SET CMP0023 OLD)
endif (POLICY CMP0023)
if(POLICY CMP0056)
  cmake_policy(SET CMP0056 NEW)
endif()
if(POLICY CMP0065)
  cmake_policy(SET CMP0065 NEW)
endif()
if(POLICY CMP0051)
  # cmake 3.1 and higher include generator expressions in SOURCES property.
  # in BuildBoost.cmake, get_target_property(<var> <target> SOURCES) is used
  # to retrieve the source files of a target. in that case, we are only
  # interested in the *source* files. and i don't want to bother stripping off
  # the TARGET_OBJECTS elements from the returned SOURCES. so let's stick with
  # the old behavior now.
  cmake_policy(SET CMP0051 OLD)
endif()
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules/")


#find_library(KVADI_LIBRARIES
#  NAMES libkvadi_static.a
#  PATHS "${CMAKE_SOURCE_DIR}/src/os/kvsstore/lib/")

#LINK_DIRECTORIES(${KVADI_LIBRARIES})

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
  set(LINUX ON)
  FIND_PACKAGE(Threads)
elseif(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
  set(FREEBSD ON)
  FIND_PACKAGE(Threads)
elseif(APPLE)
  set(OperatingSystem "Mac OS X")
else()
  message(STATUS "No systemtype selected for building")
endif(CMAKE_SYSTEM_NAME MATCHES "Linux")

option(WITH_CCACHE "Build with ccache.")
if(WITH_CCACHE)
  find_program(CCACHE_FOUND ccache)
  if(CCACHE_FOUND)
    message(STATUS "Building with ccache: ${CCACHE_FOUND}, CCACHE_DIR=$ENV{CCACHE_DIR}")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
    # ccache does not accelerate link (ld), but let it handle it. by passing it
    # along with cc to python's distutils, we are able to workaround
    # https://bugs.python.org/issue8027.
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
  else(CCACHE_FOUND)
    message(FATAL_ERROR "Can't find ccache. Is it installed?")
  endif(CCACHE_FOUND)
endif(WITH_CCACHE)

option(WITH_MANPAGE "Build man pages." ON)
if(WITH_MANPAGE)
  find_program(SPHINX_BUILD
    sphinx-build)
  if(NOT SPHINX_BUILD)
    message(FATAL_ERROR "Can't find sphinx-build.")
  endif(NOT SPHINX_BUILD)
endif(WITH_MANPAGE)

include_directories(
  ${PROJECT_BINARY_DIR}/src/include
  ${PROJECT_SOURCE_DIR}/src)

if(OFED_PREFIX)
  include_directories(${OFED_PREFIX}/include)
  link_directories(${OFED_PREFIX}/lib)
endif()

if(FREEBSD)
  include_directories(/usr/local/include)
  link_directories(/usr/local/lib)
  list(APPEND CMAKE_REQUIRED_INCLUDES /usr/local/include)
endif(FREEBSD)

#Check Includes
include(CheckIncludeFiles)
include(CheckIncludeFileCXX)
include(CheckFunctionExists)

#put all the libs and binaries in one place
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)


CHECK_FUNCTION_EXISTS(fallocate CEPH_HAVE_FALLOCATE)
CHECK_FUNCTION_EXISTS(posix_fadvise HAVE_POSIX_FADVISE)
CHECK_FUNCTION_EXISTS(posix_fallocate HAVE_POSIX_FALLOCATE)
CHECK_FUNCTION_EXISTS(syncfs HAVE_SYS_SYNCFS)
CHECK_FUNCTION_EXISTS(sync_file_range HAVE_SYNC_FILE_RANGE)
CHECK_FUNCTION_EXISTS(pwritev HAVE_PWRITEV)
CHECK_FUNCTION_EXISTS(splice CEPH_HAVE_SPLICE)
CHECK_FUNCTION_EXISTS(getgrouplist HAVE_GETGROUPLIST)
CHECK_FUNCTION_EXISTS(fdatasync HAVE_FDATASYNC)
CHECK_FUNCTION_EXISTS(strerror_r HAVE_STRERROR_R)
CHECK_FUNCTION_EXISTS(name_to_handle_at HAVE_NAME_TO_HANDLE_AT)
CHECK_FUNCTION_EXISTS(pipe2 HAVE_PIPE2)
set(CMAKE_REQUIRED_LIBRARIES pthread)
CHECK_FUNCTION_EXISTS(pthread_spin_init HAVE_PTHREAD_SPINLOCK)
CHECK_FUNCTION_EXISTS(pthread_set_name_np HAVE_PTHREAD_SET_NAME_NP)
CHECK_FUNCTION_EXISTS(pthread_setname_np HAVE_PTHREAD_SETNAME_NP)
CHECK_FUNCTION_EXISTS(pthread_getname_np HAVE_PTHREAD_GETNAME_NP)
CHECK_FUNCTION_EXISTS(eventfd HAVE_EVENTFD)
CHECK_FUNCTION_EXISTS(getprogname HAVE_GETPROGNAME)

CHECK_INCLUDE_FILES("linux/types.h" HAVE_LINUX_TYPES_H)
CHECK_INCLUDE_FILES("linux/version.h" HAVE_LINUX_VERSION_H)
CHECK_INCLUDE_FILES("arpa/nameser_compat.h" HAVE_ARPA_NAMESER_COMPAT_H)
CHECK_INCLUDE_FILES("sys/mount.h" HAVE_SYS_MOUNT_H)
CHECK_INCLUDE_FILES("sys/param.h" HAVE_SYS_PARAM_H)
CHECK_INCLUDE_FILES("sys/types.h" HAVE_SYS_TYPES_H)
CHECK_INCLUDE_FILES("sys/vfs.h" HAVE_SYS_VFS_H)
CHECK_INCLUDE_FILES("sys/prctl.h" HAVE_SYS_PRCTL_H)
CHECK_INCLUDE_FILES("execinfo.h" HAVE_EXECINFO_H)
if(LINUX)
  CHECK_INCLUDE_FILES("sched.h" HAVE_SCHED)
endif(LINUX)
CHECK_INCLUDE_FILES("valgrind/helgrind.h" HAVE_VALGRIND_HELGRIND_H)

include(CheckTypeSize)
set(CMAKE_EXTRA_INCLUDE_FILES "linux/types.h")
CHECK_TYPE_SIZE(__be16 __BE16) 
CHECK_TYPE_SIZE(__be32 __BE32) 
CHECK_TYPE_SIZE(__be64 __BE64) 
CHECK_TYPE_SIZE(__le16 __LE16) 
CHECK_TYPE_SIZE(__le32 __LE32) 
CHECK_TYPE_SIZE(__le64 __LE64) 
CHECK_TYPE_SIZE(__u8 __U8) 
CHECK_TYPE_SIZE(__u16 __U16) 
CHECK_TYPE_SIZE(__u32 __U32) 
CHECK_TYPE_SIZE(__u64 __U64) 
CHECK_TYPE_SIZE(__s8 __S8) 
CHECK_TYPE_SIZE(__s16 __S16) 
CHECK_TYPE_SIZE(__s32 __S32) 
CHECK_TYPE_SIZE(__s64 __S64) 
unset(CMAKE_EXTRA_INCLUDE_FILES)

include(CheckSymbolExists)
CHECK_SYMBOL_EXISTS(res_nquery "resolv.h" HAVE_RES_NQUERY)
CHECK_SYMBOL_EXISTS(F_SETPIPE_SZ "linux/fcntl.h" CEPH_HAVE_SETPIPE_SZ)
CHECK_SYMBOL_EXISTS(__func__ "" HAVE_FUNC)
CHECK_SYMBOL_EXISTS(__PRETTY_FUNCTION__ "" HAVE_PRETTY_FUNC)

include(CheckCXXSourceCompiles)
CHECK_CXX_SOURCE_COMPILES("
  #include <string.h>
  int main() { char x = *strerror_r(0, &x, sizeof(x)); return 0; }
  " STRERROR_R_CHAR_P)

include(CheckStructHasMember)
CHECK_STRUCT_HAS_MEMBER("struct stat" st_mtim.tv_nsec sys/stat.h
  HAVE_STAT_ST_MTIM_TV_NSEC LANGUAGE C)
CHECK_STRUCT_HAS_MEMBER("struct stat" st_mtimespec.tv_nsec sys/stat.h
  HAVE_STAT_ST_MTIMESPEC_TV_NSEC LANGUAGE C)

set(CEPH_MAN_DIR "share/man" CACHE STRING "Install location for man pages (relative to prefix).")

option(ENABLE_SHARED "build shared libraries" ON)
if(ENABLE_SHARED)
  set(CEPH_SHARED SHARED)
else(ENABLE_SHARED)
  set(CEPH_SHARED STATIC)
endif(ENABLE_SHARED)
set(CMAKE_POSITION_INDEPENDENT_CODE ${ENABLE_SHARED})

option(WITH_RDMA "Enable RDMA in async messenger" ON)
if(WITH_RDMA)
  find_package(rdma REQUIRED)
  set(HAVE_RDMA ${RDMA_FOUND})
endif(WITH_RDMA)

find_package(Backtrace)

if(LINUX)
  find_package(udev REQUIRED)
  set(HAVE_UDEV ${UDEV_FOUND})

  find_package(aio REQUIRED)
  set(HAVE_LIBAIO ${AIO_FOUND})

  find_package(blkid REQUIRED)
  set(HAVE_BLKID ${BLKID_FOUND})
else()
  set(HAVE_UDEV OFF)
  message(STATUS "Not using udev")
  set(HAVE_LIBAIO OFF)
  message(STATUS "Not using AIO")
  set(HAVE_BLKID OFF)
  message(STATUS "Not using BLKID")
endif(LINUX)

option(WITH_OPENLDAP "OPENLDAP is here" ON)
if(${WITH_OPENLDAP})
find_package(OpenLdap REQUIRED)
set(HAVE_OPENLDAP ${OPENLDAP_FOUND})
endif(${WITH_OPENLDAP})

option(WITH_FUSE "Fuse is here" ON)
if(${WITH_FUSE})
find_package(fuse)
set(HAVE_LIBFUSE ${FUSE_FOUND})
endif(${WITH_FUSE})

option(WITH_XFS "XFS is here" ON)
if(WITH_XFS)
  find_package(xfs)
  set(HAVE_LIBXFS ${XFS_FOUND})
endif()

option(WITH_ZFS "enable LibZFS if found" OFF)
if(WITH_ZFS)
  find_package(zfs)
  set(HAVE_LIBZFS ${ZFS_FOUND})
endif()

option(WITH_SPDK "Enable SPDK" OFF)
if(WITH_SPDK)
  find_package(dpdk REQUIRED)
  set(HAVE_SPDK TRUE)
endif(WITH_SPDK)

option(WITH_PMEM "Enable PMEM" OFF)
if(WITH_PMEM)
  find_package(pmem REQUIRED)
  set(HAVE_PMEM ${PMEM_FOUND})
endif(WITH_PMEM)

# needs mds and? XXX
option(WITH_LIBCEPHFS "libcephfs client library" ON)

# key-value store
option(WITH_KVS "Key value store is here" ON)

# remote block storage
option(WITH_RBD "Remote block storage is here" ON)

# KERNEL remote block storage
option(WITH_KRBD "Kernel Remote block storage is here" ON)

if(WITH_KRBD AND WITHOUT_RBD)
  message(FATAL_ERROR "Cannot have WITH_KRBD with WITH_RBD.")
endif()

# embedded ceph daemon static library
# NOTE: Ceph is mostly LGPL (see COPYING), which means that
# static linking brings with it restrictions. Please be sure
# to look at the LGPL license carefully before linking this library to
# your code. See http://www.gnu.org/licenses/gpl-faq.html#LGPLStaticVsDynamic.
option(WITH_EMBEDDED "build the embedded ceph daemon library" ON)

option(WITH_LEVELDB "LevelDB is here" ON)
if(WITH_LEVELDB)
  if(LEVELDB_PREFIX)
    include_directories(${LEVELDB_PREFIX}/include)
    link_directories(${LEVELDB_PREFIX}/lib)
  endif()
  find_package(leveldb REQUIRED)
  find_file(HAVE_LEVELDB_FILTER_POLICY leveldb/filter_policy.h PATHS ${LEVELDB_INCLUDE_DIR})
endif(WITH_LEVELDB)

find_package(snappy REQUIRED)

option(WITH_LZ4 "LZ4 compression support" OFF)
if(WITH_LZ4)
  find_package(LZ4 1.7 REQUIRED)
  set(HAVE_LZ4 ${LZ4_FOUND})
endif(WITH_LZ4)

#if allocator is set on command line make sure it matches below strings
if(ALLOCATOR)
  if(${ALLOCATOR} MATCHES "tcmalloc(_minimal)?")
    find_package(gperftools REQUIRED)
    set(HAVE_LIBTCMALLOC ON)
  elseif(${ALLOCATOR} STREQUAL "jemalloc")
    find_package(JeMalloc REQUIRED)
    set(HAVE_LIBJEMALLOC ${JEMALLOC_FOUND})
    set(HAVE_JEMALLOC 1)
  endif()
else(ALLOCATOR)
  find_package(gperftools)
  set(HAVE_LIBTCMALLOC ${GPERFTOOLS_FOUND})
  if(NOT GPERFTOOLS_FOUND)
    find_package(JeMalloc)
    set(HAVE_LIBJEMALLOC ${JEMALLOC_FOUND})
  endif(NOT GPERFTOOLS_FOUND)
  if(GPERFTOOLS_FOUND)
    set(ALLOCATOR tcmalloc)
  elseif(JEMALLOC_FOUND)
    set(ALLOCATOR jemalloc)
  else()
    if(NOT FREEBSD)
      # FreeBSD already has jemalloc as its default allocator
      message(WARNING "tcmalloc and jemalloc not found, falling back to libc")
    endif()
    set(ALLOCATOR "libc")
  endif(GPERFTOOLS_FOUND)
endif(ALLOCATOR)


if(WITH_LIBCEPHFS OR WITH_KRBD)
  find_package(keyutils REQUIRED)
endif()

find_package(CURL REQUIRED)
set(CMAKE_REQUIRED_INCLUDES ${CURL_INCLUDE_DIRS})
set(CMAKE_REQUIRED_LIBRARIES ${CURL_LIBRARIES})
CHECK_SYMBOL_EXISTS(curl_multi_wait curl/curl.h HAVE_CURL_MULTI_WAIT)

# nss or cryptopp?
option(WITH_NSS "Use NSS crypto and SSL implementations" ON)
if (${WITH_NSS})
  find_package(NSS REQUIRED)
    set(USE_NSS 1)
    find_package(NSPR REQUIRED)
    set(CRYPTO_LIBS ${NSS_LIBRARIES} ${NSPR_LIBRARIES})
else ()
  find_package(cryptopp REQUIRED)
    set(CRYPTO_LIBS ${CRYPTOPP_LIBRARIES})
    set(USE_CRYPTOPP 1)
endif (${WITH_NSS})

option(WITH_SSL "SSL build selected" ON)
if(USE_NSS)
  #nss
  set(SSL_LIBRARIES ${NSS_LIBRARIES})
  message(STATUS "SSL with NSS selected (Libs: ${SSL_LIBRARIES})")
else(USE_NSS)
  #openssl
  find_package(OpenSSL REQUIRED)
  set(USE_OPENSSL ON)
  set(SSL_LIBRARIES ${OPENSSL_LIBRARIES})
  message(STATUS "SSL with OpenSSL selected (Libs: ${SSL_LIBRARIES})")
endif(USE_NSS)

option(WITH_XIO "Enable XIO messaging" OFF)
if(WITH_XIO)
  find_package(xio REQUIRED)
set(HAVE_XIO ${XIO_FOUND})
endif(WITH_XIO)

option(WITH_DPDK "Enable DPDK messaging" OFF)
if(WITH_DPDK)
find_package(dpdk)
set(HAVE_DPDK ${DPDK_FOUND})
endif(WITH_DPDK)

option(WITH_BLKIN "Use blkin to emit LTTng tracepoints for Zipkin" OFF)
if(WITH_BLKIN)
  set(BLKIN_LIBRARIES blkin lttng-ust)
  include_directories(src/blkin/blkin-lib)
endif(WITH_BLKIN)

#option for RGW
option(WITH_RADOSGW "Rados Gateway is enabled" ON)
option(WITH_RADOSGW_FCGI_FRONTEND "Rados Gateway's FCGI frontend is enabled" OFF)
option(WITH_RADOSGW_BEAST_FRONTEND "Rados Gateway's Beast frontend is enabled" ON)
if(WITH_RADOSGW)
  find_package(EXPAT REQUIRED)
  if(WITH_RADOSGW_FCGI_FRONTEND)
    find_package(fcgi REQUIRED)
  endif()
endif(WITH_RADOSGW)


if (WITH_RADOSGW)
  if (NOT DEFINED OPENSSL_FOUND)
    message(STATUS "Looking for openssl anyways, because radosgw selected")
    find_package(OpenSSL)
  endif()
  if (OPENSSL_FOUND)
    execute_process(
      COMMAND
	"sh" "-c"
	"objdump -p ${OPENSSL_SSL_LIBRARY} | sed -n 's/^  SONAME  *//p'"
      OUTPUT_VARIABLE LIBSSL_SONAME
      ERROR_VARIABLE OBJDUMP_ERRORS
      RESULT_VARIABLE OBJDUMP_RESULTS
      OUTPUT_STRIP_TRAILING_WHITESPACE)
    if (OBJDUMP_RESULTS)
      message(FATAL_ERROR "can't run objdump: ${OBJDUMP_RESULTS}")
    endif()
    if (NOT OBJDUMP_ERRORS STREQUAL "")
      message(WARNING "message from objdump: ${OBJDUMP_ERRORS}")
    endif()
    execute_process(
      COMMAND
	"sh" "-c"
	"objdump -p ${OPENSSL_CRYPTO_LIBRARY} | sed -n 's/^  SONAME  *//p'"
      OUTPUT_VARIABLE LIBCRYPTO_SONAME
      ERROR_VARIABLE OBJDUMP_ERRORS
      RESULT_VARIABLE OBJDUMP_RESULTS
      OUTPUT_STRIP_TRAILING_WHITESPACE)
    if (OBJDUMP_RESULTS)
      message(FATAL_ERROR "can't run objdump: ${OBJDUMP_RESULTS}")
    endif()
    if (NOT OBJDUMP_ERRORS STREQUAL "")
      message(WARNING "message from objdump: ${OBJDUMP_ERRORS}")
    endif()
    message(STATUS "ssl soname: ${LIBSSL_SONAME}")
    message(STATUS "crypto soname: ${LIBCRYPTO_SONAME}")
  else()
    message(WARNING "ssl not found: rgw civetweb may fail to dlopen libssl libcrypto")
  endif()
endif (WITH_RADOSGW)

#option for CephFS
option(WITH_CEPHFS "CephFS is enabled" ON)

#option for Mgr
option(WITH_MGR "ceph-mgr is enabled" ON)
if(WITH_MGR)
    set(Python_ADDITIONAL_VERSIONS 2.7)
    find_package(PythonInterp 2.7 REQUIRED)
    find_package(PythonLibs 2.7 REQUIRED)
    # Boost dependency check deferred to Boost section
endif(WITH_MGR)

option(WITH_THREAD_SAFE_RES_QUERY "res_query is thread safe" OFF)
if(WITH_THREAD_SAFE_RES_QUERY)
    set(HAVE_THREAD_SAFE_RES_QUERY 1 CACHE INTERNAL "Thread safe res_query supported.")
endif(WITH_THREAD_SAFE_RES_QUERY)

option(WITH_REENTRANT_STRSIGNAL "strsignal is reentrant" OFF)
if(WITH_REENTRANT_STRSIGNAL)
    set(HAVE_REENTRANT_STRSIGNAL 1 CACHE INTERNAL "Reentrant strsignal is supported.")
endif(WITH_REENTRANT_STRSIGNAL)

set(HAVE_LIBROCKSDB 1)

# -lz link into kv
find_package(ZLIB REQUIRED)

#option for EventTrace
option(WITH_EVENTTRACE "Event tracing support" OFF)

#option for LTTng
option(WITH_LTTNG "LTTng tracing is enabled" ON)
if(${WITH_LTTNG})
  find_package(LTTngUST REQUIRED)
  find_program(LTTNG_GEN_TP
    lttng-gen-tp)
  if(NOT LTTNG_GEN_TP)
    message(FATAL_ERROR "Can't find lttng-gen-tp.")
  endif()
endif(${WITH_LTTNG})

option(WITH_OSD_INSTRUMENT_FUNCTIONS OFF)

#option for Babeltrace
option(WITH_BABELTRACE "Babeltrace libraries are enabled" ON)
if(WITH_BABELTRACE)
  set(HAVE_BABELTRACE ON)
  find_package(babeltrace REQUIRED)
  set(HAVE_BABELTRACE_BABELTRACE_H ${BABELTRACE_FOUND})
  set(HAVE_BABELTRACE_CTF_EVENTS_H ${BABELTRACE_FOUND})
  set(HAVE_BABELTRACE_CTF_ITERATOR_H ${BABELTRACE_FOUND})
endif(WITH_BABELTRACE)

option(DEBUG_GATHER "C_Gather debugging is enabled" ON)
option(ENABLE_COVERAGE "Coverage is enabled" OFF)
option(PG_DEBUG_REFS "PG Ref debugging is enabled" OFF)

option(WITH_TESTS "enable the build of ceph-test package scripts/binaries" ON)

option(WITH_FIO "Support for fio engines" OFF)
if(WITH_FIO)
  find_package(fio REQUIRED)
endif(WITH_FIO)

if(LINUX)
  add_definitions(-D__linux__)
endif(LINUX)

# ASAN and friends
option(WITH_ASAN "build with ASAN" OFF)
option(WITH_ASAN_LEAK "explicitly enable ASAN leak detection" OFF)

if(WITH_ASAN)
  set(ASAN_CFLAGS "-fsanitize=address -fno-omit-frame-pointer")
  if(WITH_ASAN_LEAK)
    set(ASAN_CFLAGS "${ASAN_CFLAGS} -fsanitize=leak")
  endif()
  set(ASAN_LFLAGS "${CMAKE_EXE_LINKER_FLAGS} ${ASAN_CFLAGS} -lasan")
  set(CMAKE_EXE_LINKER_FLAGS "${ASAN_LFLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ASAN_CFLAGS}")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ASAN_CFLAGS}")
  if(HAVE_JEMALLOC)
    message(FATAL "ASAN does not work well with JeMalloc")
  endif()
endif()

option(WITH_TSAN "build with TSAN" OFF)
if(WITH_TSAN)
  if (WITH_ASAN AND WITH_ASAN_LEAK)
    message(FATAL_ERROR "Cannot combine -fsanitize-leak w/-fsanitize-thread")
  endif()
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=thread -fno-omit-frame-pointer -pie -ltsan ")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread -fno-omit-frame-pointer -fPIC")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=thread -fno-omit-frame-pointer -fPIC")
  if(HAVE_JEMALLOC)
    message(FATAL "TSAN does not work well with JeMalloc")
  endif()
endif()

option(WITH_UBSAN "build with UBSAN" OFF)
if(WITH_UBSAN)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=undefined")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=undefined")
  if(HAVE_JEMALLOC)
    message(FATAL "UBSAN does not work well with JeMalloc")
  endif()
endif()

# Rocksdb
option(WITH_SYSTEM_ROCKSDB "require and build with system rocksdb" OFF)
if (WITH_SYSTEM_ROCKSDB)
  find_package(rocksdb REQUIRED)
endif()

# Boost
option(WITH_SYSTEM_BOOST "require and build with system Boost" OFF)

# Boost::thread depends on Boost::atomic, so list it explicitly.
set(BOOST_COMPONENTS
  atomic thread system regex random program_options date_time iostreams
  filesystem)
set(BOOST_HEADER_COMPONENTS container)

if(WITH_MGR)
	list(APPEND BOOST_COMPONENTS python)
endif()

if(WITH_RADOSGW_BEAST_FRONTEND)
	list(APPEND BOOST_COMPONENTS coroutine context)
endif()

set(Boost_USE_MULTITHREADED ON)
# require minimally the bundled version
if(WITH_SYSTEM_BOOST)
  if(ENABLE_SHARED)
    set(Boost_USE_STATIC_LIBS OFF)
  else()
    set(Boost_USE_STATIC_LIBS ON)
  endif()
  find_package(Boost 1.61 COMPONENTS ${BOOST_COMPONENTS} REQUIRED)
else()
  set(BOOST_J 1 CACHE STRING
    "max jobs for Boost build") # override w/-DBOOST_J=<n>
  set(Boost_USE_STATIC_LIBS ON)
  include(BuildBoost)
  build_boost(1.63
    COMPONENTS ${BOOST_COMPONENTS} ${BOOST_HEADER_COMPONENTS})
  include_directories(BEFORE SYSTEM ${Boost_INCLUDE_DIRS})
endif()
set(Boost_USE_MULTITHREADED ON)

include_directories(SYSTEM ${PROJECT_BINARY_DIR}/include)

find_package(Threads REQUIRED)

option(WITH_SELINUX "build SELinux policy" OFF)
if(WITH_SELINUX)
  find_file(SELINUX_MAKEFILE selinux/devel/Makefile
    PATH /usr/share)
  if(NOT SELINUX_MAKEFILE)
    message(FATAL_ERROR "Can't find selinux's Makefile")
  endif()
  add_subdirectory(selinux)
endif(WITH_SELINUX)

# enables testing and creates Make check command
add_custom_target(tests
  COMMENT "Building tests")
enable_testing()
set(CMAKE_CTEST_COMMAND ctest)
add_custom_target(check
  COMMAND ${CMAKE_CTEST_COMMAND}
  DEPENDS tests)

add_subdirectory(src)

add_subdirectory(doc)
if(WITH_MANPAGE)
  add_subdirectory(man)
endif(WITH_MANPAGE)

option(WITH_SYSTEMD "install systemd target and service files" OFF)
if(WITH_SYSTEMD)
  add_subdirectory(systemd)
endif()

include(CTags)
option(CTAG_EXCLUDES "Exclude files/directories when running ctag.")
add_tags(ctags
  SRC_DIR src
  TAG_FILE tags
  EXCLUDE_OPTS ${CTAG_EXCLUDES}
  EXCLUDES "*.js" "*.css")
add_custom_target(tags DEPENDS ctags)
