find_package(EXPAT)
if(NOT EXPAT_FOUND)
  message(FATAL_ERROR "EXPAT not found,  make sure you have also installed the expat and it's dev package")
endif(NOT EXPAT_FOUND)
include_directories(${EXPAT_INCLUDE_DIR})
set (EXPAT_CFLAGS_PKG "-I${EXPAT_INCLUDE_DIRS}")
string(REGEX REPLACE "^(.*)/lib([^/]*)\\.[^.]*$" "-L\\1 -l\\2" EXPAT_LIBS_PKG ${EXPAT_LIBRARY})

if(NOT MKL_FOUND AND NOT GSL_FOUND)
  #explicitly disabled
  file(GLOB VOTCA_LINALG_SOURCES linalg/dummy/*.cc)
elseif (MKL_FOUND)
  include_directories(${MKL_INCLUDE_DIRS})
  file(GLOB VOTCA_LINALG_SOURCES linalg/mkl/*.cc)
  set(LINALG_LIBRARIES ${MKL_LIBRARIES})
elseif(GSL_FOUND)
  include_directories(${GSL_INCLUDE_DIRS})
  file(GLOB VOTCA_LINALG_SOURCES linalg/gsl/*.cc)
  set(LINALG_LIBRARIES ${GSL_LIBRARIES})
endif()

file(GLOB VOTCA_SOURCES *.cc)
file(GLOB NOT_VOTCA_SOURCES version_nb.cc)
file(GLOB_RECURSE VOTCA_SQL_SOURCES database.cc statement.cc)
list(REMOVE_ITEM VOTCA_SOURCES ${NOT_VOTCA_SOURCES} ${VOTCA_SQL_SOURCES})

option(WITH_SQLITE3 "Use SQLite3, at this point only needed for charge transport parts!" ON)
if (WITH_SQLITE3)
  find_package(SQLITE3)
  if(NOT SQLITE3_FOUND)
    message(FATAL_ERROR "SQLITE3 not found,  make sure you have also installed the sqlite3 and it's dev package, at this point sqlite3 is only needed for charge transport parts of VOTCA, so you can just switch it off using -DWITH_SQLITE3=OFF in case you do not need it.")
  endif(NOT SQLITE3_FOUND)
  include_directories(${SQLITE3_INCLUDE_DIRS})
  set(SQLITE3_PKG "sqlite3")
else(WITH_SQLITE3)
  set(SQLITE3_PKG)
  set(VOTCA_SQL_SOURCES)
endif(WITH_SQLITE3)


#for gitversion.h
include_directories(${CMAKE_CURRENT_BINARY_DIR})
add_custom_target(gitversion COMMAND ${CMAKE_COMMAND}
  -DTOP_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
  -DGIT_EXECUTABLE="${GIT_EXECUTABLE}"
  -DMERCURIAL_EXECUTABLE="${MERCURIAL_EXECUTABLE}"
  -P ${CMAKE_MODULE_PATH}/gitversion.cmake)
set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES gitversion.h)

add_library(votca_tools ${VOTCA_SOURCES} ${VOTCA_SQL_SOURCES} ${VOTCA_LINALG_SOURCES})
add_dependencies(votca_tools gitversion)
if(MKL_FOUND)
  add_dependencies(votca_tools intel)
endif(MKL_FOUND)
set_target_properties(votca_tools PROPERTIES SOVERSION ${SOVERSION})
target_link_libraries(votca_tools ${Boost_LIBRARIES} ${LINALG_LIBRARIES} ${SQLITE3_LIBRARIES}
  ${FFTW3_LIBRARIES} ${EXPAT_LIBRARIES} ${THREAD_LIBRARIES} ${MATH_LIBRARIES})
install(TARGETS votca_tools LIBRARY DESTINATION ${LIB} ARCHIVE DESTINATION ${LIB})

configure_file(libvotca_tools.pc.in ${CMAKE_CURRENT_BINARY_DIR}/libvotca_tools.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libvotca_tools.pc DESTINATION ${LIB}/pkgconfig)
