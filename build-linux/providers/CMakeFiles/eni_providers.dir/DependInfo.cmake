# The set of languages for which implicit dependencies are needed:
set(CMAKE_DEPENDS_LANGUAGES
  "C"
  )
# The set of files for implicit dependencies of each language:
set(CMAKE_DEPENDS_CHECK_C
  "/home/spatchava/embeddedos-org/eNI/providers/eeg/eeg.c" "/home/spatchava/embeddedos-org/eNI/build-linux/providers/CMakeFiles/eni_providers.dir/eeg/eeg.c.o"
  "/home/spatchava/embeddedos-org/eNI/providers/generic/generic.c" "/home/spatchava/embeddedos-org/eNI/build-linux/providers/CMakeFiles/eni_providers.dir/generic/generic.c.o"
  "/home/spatchava/embeddedos-org/eNI/providers/neuralink/neuralink.c" "/home/spatchava/embeddedos-org/eNI/build-linux/providers/CMakeFiles/eni_providers.dir/neuralink/neuralink.c.o"
  "/home/spatchava/embeddedos-org/eNI/providers/simulator/simulator.c" "/home/spatchava/embeddedos-org/eNI/build-linux/providers/CMakeFiles/eni_providers.dir/simulator/simulator.c.o"
  "/home/spatchava/embeddedos-org/eNI/providers/stimulator_sim/stimulator_sim.c" "/home/spatchava/embeddedos-org/eNI/build-linux/providers/CMakeFiles/eni_providers.dir/stimulator_sim/stimulator_sim.c.o"
  )
set(CMAKE_C_COMPILER_ID "GNU")

# Preprocessor definitions for this target.
set(CMAKE_TARGET_DEFINITIONS_C
  "ENI_HAS_DATA_FORMATS"
  "ENI_HAS_DECODER"
  "ENI_HAS_DSP"
  "ENI_HAS_EEG"
  "ENI_HAS_NEURALINK"
  "ENI_HAS_NN"
  "ENI_HAS_SESSION"
  "ENI_HAS_STIMULATOR"
  "ENI_HAS_STIMULATOR_SIM"
  )

# The include file search paths:
set(CMAKE_C_TARGET_INCLUDE_PATH
  "../providers/simulator"
  "../providers/generic"
  "../providers/neuralink"
  "../providers/eeg"
  "../providers/stimulator_sim"
  "../common/include"
  "../platform/include"
  )

# Targets to which this target links.
set(CMAKE_TARGET_LINKED_INFO_FILES
  "/home/spatchava/embeddedos-org/eNI/build-linux/common/CMakeFiles/eni_common.dir/DependInfo.cmake"
  )

# Fortran module output directory.
set(CMAKE_Fortran_TARGET_MODULE_DIR "")
