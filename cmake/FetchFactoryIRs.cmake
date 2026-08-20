if (NOT DEFINED GUITARDSP_FACTORY_IR_OUT OR NOT DEFINED GUITARDSP_FACTORY_IR_CACHE)
    message(FATAL_ERROR "Factory IR output/cache directories were not supplied")
endif()

# Jester Dyne's original host is occasionally unavailable via DNS. The same
# 48 kHz / 24-bit / mono Brutal Pack captures are bundled by Darwin's Cat
# OrbitCab under CC0. Pin an immutable Git commit so Factory IR builds do not
# depend on the mutable state of another project.
set(_orbitcab_commit "9081c0bdd84b325836d56aaebdb3955dbd9ccc0c")
set(_url "https://github.com/darwinscat/orbitcab/archive/${_orbitcab_commit}.zip")
set(_zip "${GUITARDSP_FACTORY_IR_CACHE}/orbitcab-${_orbitcab_commit}.zip")
set(_extract "${GUITARDSP_FACTORY_IR_CACHE}/orbitcab-${_orbitcab_commit}")
set(_source "${_extract}/orbitcab-${_orbitcab_commit}/resources/ir")

set(_pairs
    "01-cookie-monster.wav|1_Cookie_Monster.wav"
    "02-darth-genocider.wav|2_Darth_Genocider.wav"
    "03-kitten-slayer.wav|3_Kitten_Slayer.wav"
    "04-kaiju-tamer.wav|4_Kaiju_Tamer.wav"
    "05-iceburn-suicide.wav|5_Iceburn_Suicide.wav"
    "06-vertical-lip-stabber.wav|6_Vertical_Lip_Stabber.wav"
    "07-manslaughter-joe.wav|7_Manslaughter_Joe.wav"
    "08-big-bubba.wav|8_Big_Bubba.wav"
    "09-devils-cunnilingus.wav|9_Devils_Cunnilingus.wav"
    "10-october-32th.wav|10_October_32th.wav"
    "11-wumbo.wav|11_Wumbo.wav"
    "12-world-collider.wav|12_World_Collider.wav"
    "13-cannibal-choir.wav|13_Cannibal_Choir.wav"
    "14-cathode-ray-fleshburn.wav|14_Cathode_Ray_Fleshburn.wav"
    "15-impaler-jim.wav|15_Impaler_Jim.wav")

file(MAKE_DIRECTORY "${GUITARDSP_FACTORY_IR_CACHE}")

if (NOT EXISTS "${_zip}")
    message(STATUS "Downloading measured Factory IR source from pinned OrbitCab commit ${_orbitcab_commit}")
    file(DOWNLOAD "${_url}" "${_zip}"
         STATUS _download_status
         TLS_VERIFY ON
         TIMEOUT 90
         SHOW_PROGRESS)
    list(GET _download_status 0 _download_code)
    if (NOT _download_code EQUAL 0)
        list(GET _download_status 1 _download_message)
        file(REMOVE "${_zip}")
        message(FATAL_ERROR "Factory IR GitHub download failed: ${_download_message}. Reconfigure with -DGUITARDSP_FETCH_FACTORY_IRS=OFF for an offline build.")
    endif()
endif()

# Record the archive digest in the build log for reproducibility. The source URL
# itself is pinned to an immutable Git commit.
file(SHA256 "${_zip}" _archive_sha256)
message(STATUS "Factory IR pinned source archive SHA-256: ${_archive_sha256}")

set(_need_extract FALSE)
foreach(_pair IN LISTS _pairs)
    string(REPLACE "|" ";" _parts "${_pair}")
    list(GET _parts 0 _src_name)
    if (NOT EXISTS "${_source}/${_src_name}")
        set(_need_extract TRUE)
    endif()
endforeach()

if (_need_extract)
    file(REMOVE_RECURSE "${_extract}")
    file(MAKE_DIRECTORY "${_extract}")
    file(ARCHIVE_EXTRACT INPUT "${_zip}" DESTINATION "${_extract}")
endif()

file(MAKE_DIRECTORY "${GUITARDSP_FACTORY_IR_OUT}")
foreach(_pair IN LISTS _pairs)
    string(REPLACE "|" ";" _parts "${_pair}")
    list(GET _parts 0 _src_name)
    list(GET _parts 1 _dst_name)
    if (NOT EXISTS "${_source}/${_src_name}")
        message(FATAL_ERROR "Pinned Factory IR source is missing expected capture: ${_src_name}")
    endif()
    configure_file("${_source}/${_src_name}" "${GUITARDSP_FACTORY_IR_OUT}/${_dst_name}" COPYONLY)
endforeach()

set(_license "${CMAKE_CURRENT_LIST_DIR}/../assets/ir/FACTORY_IR_LICENSE.md")
if (EXISTS "${_license}")
    configure_file("${_license}" "${GUITARDSP_FACTORY_IR_OUT}/FACTORY_IR_LICENSE.md" COPYONLY)
endif()

message(STATUS "Factory IR pack ready: 15 measured 48 kHz captures -> ${GUITARDSP_FACTORY_IR_OUT}")
