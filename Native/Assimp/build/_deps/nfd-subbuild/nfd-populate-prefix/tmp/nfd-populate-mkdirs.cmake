# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/Users/seangandre/Documents/GitHub/Atlys/Native/Assimp/build/_deps/nfd-src")
  file(MAKE_DIRECTORY "/Users/seangandre/Documents/GitHub/Atlys/Native/Assimp/build/_deps/nfd-src")
endif()
file(MAKE_DIRECTORY
  "/Users/seangandre/Documents/GitHub/Atlys/Native/Assimp/build/_deps/nfd-build"
  "/Users/seangandre/Documents/GitHub/Atlys/Native/Assimp/build/_deps/nfd-subbuild/nfd-populate-prefix"
  "/Users/seangandre/Documents/GitHub/Atlys/Native/Assimp/build/_deps/nfd-subbuild/nfd-populate-prefix/tmp"
  "/Users/seangandre/Documents/GitHub/Atlys/Native/Assimp/build/_deps/nfd-subbuild/nfd-populate-prefix/src/nfd-populate-stamp"
  "/Users/seangandre/Documents/GitHub/Atlys/Native/Assimp/build/_deps/nfd-subbuild/nfd-populate-prefix/src"
  "/Users/seangandre/Documents/GitHub/Atlys/Native/Assimp/build/_deps/nfd-subbuild/nfd-populate-prefix/src/nfd-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/Users/seangandre/Documents/GitHub/Atlys/Native/Assimp/build/_deps/nfd-subbuild/nfd-populate-prefix/src/nfd-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/Users/seangandre/Documents/GitHub/Atlys/Native/Assimp/build/_deps/nfd-subbuild/nfd-populate-prefix/src/nfd-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
