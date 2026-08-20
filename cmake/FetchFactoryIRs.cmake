if (NOT DEFINED GUITARDSP_FACTORY_IR_OUT OR NOT DEFINED GUITARDSP_FACTORY_IR_CACHE)
    message(FATAL_ERROR "Factory IR output/cache directories were not supplied")
endif()

set(_url "https://www.jester-dyne-productions.com/content/files/2023/04/JestersBrutalPack_1.0.zip")
set(_sha "299dc053f01ebd1e980459adc48f9c6b8a8c7af91917b4f946512eefdbb311ea")
set(_zip "${GUITARDSP_FACTORY_IR_CACHE}/JestersBrutalPack_1.0.zip")
set(_extract "${GUITARDSP_FACTORY_IR_CACHE}/extracted")
set(_source "${_extract}/Jesters_Brutal_Pack_1.0/Impulses/48kHz")

set(_files
    "1_Cookie_Monster.wav"
    "2_Darth_Genocider.wav"
    "3_Kitten_Slayer.wav"
    "4_Kaiju_Tamer.wav"
    "5_Iceburn_Suicide.wav"
    "6_Vertical_Lip_Stabber.wav"
    "7_Manslaughter_Joe.wav"
    "8_Big_Bubba.wav"
    "9_Devils_Cunnilingus.wav"
    "10_October_32th.wav"
    "11_Wumbo.wav"
    "12_World_Collider.wav"
    "13_Cannibal_Choir.wav"
    "14_Cathode_Ray_Fleshburn.wav"
    "15_Impaler_Jim.wav")

file(MAKE_DIRECTORY "${GUITARDSP_FACTORY_IR_CACHE}")

set(_need_download TRUE)
if (EXISTS "${_zip}")
    file(SHA256 "${_zip}" _existing_sha)
    if ("${_existing_sha}" STREQUAL "${_sha}")
        set(_need_download FALSE)
    else()
        file(REMOVE "${_zip}")
    endif()
endif()

if (_need_download)
    message(STATUS "Downloading measured Factory IR pack")
    file(DOWNLOAD "${_url}" "${_zip}"
         EXPECTED_HASH "SHA256=${_sha}"
         STATUS _download_status
         TLS_VERIFY ON
         TIMEOUT 90
         SHOW_PROGRESS)
    list(GET _download_status 0 _download_code)
    if (NOT _download_code EQUAL 0)
        list(GET _download_status 1 _download_message)
        message(FATAL_ERROR "Factory IR download failed: ${_download_message}. Reconfigure with -DGUITARDSP_FETCH_FACTORY_IRS=OFF for an offline build.")
    endif()
endif()

set(_need_extract FALSE)
foreach(_name IN LISTS _files)
    if (NOT EXISTS "${_source}/${_name}")
        set(_need_extract TRUE)
    endif()
endforeach()

if (_need_extract)
    file(REMOVE_RECURSE "${_extract}")
    file(MAKE_DIRECTORY "${_extract}")
    file(ARCHIVE_EXTRACT INPUT "${_zip}" DESTINATION "${_extract}")
endif()

file(MAKE_DIRECTORY "${GUITARDSP_FACTORY_IR_OUT}")
foreach(_name IN LISTS _files)
    if (NOT EXISTS "${_source}/${_name}")
        message(FATAL_ERROR "Factory IR pack is missing expected capture: ${_name}")
    endif()
    configure_file("${_source}/${_name}" "${GUITARDSP_FACTORY_IR_OUT}/${_name}" COPYONLY)
endforeach()

set(_license "${CMAKE_CURRENT_LIST_DIR}/../assets/ir/FACTORY_IR_LICENSE.md")
if (EXISTS "${_license}")
    configure_file("${_license}" "${GUITARDSP_FACTORY_IR_OUT}/FACTORY_IR_LICENSE.md" COPYONLY)
endif()

message(STATUS "Factory IR pack ready: 15 measured 48 kHz captures -> ${GUITARDSP_FACTORY_IR_OUT}")
