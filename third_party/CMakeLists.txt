
# this cmakefile replaces some of the cmake files that are originally
# supplied with the third party code such as the libsndfile which has
# a ton of ***** in it...

cmake_minimum_required(VERSION 3.0)

project(third_party)

if (MSVC)
    # suppress a bunch of shit that we're not fixing

    # conversion from double to float
    add_compile_options(/wd4244)
    # trunaction from double to float
    add_compile_options(/wd4305)
    # conversino from size_t to long
    add_compile_options(/wd4267)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

if (UNIX)
    add_compile_options(-Wno-implicit-function-declaration)
endif()

if (CMAKE_SYSTEM_NAME MATCHES "Emscripten")
    message(STATUS "Enable emscripten optimization build flags.")
    add_compile_options(-O3)
    # -msse flag is needed to enable SSEx extensions
    # -msimd128 is needed because of -msse flag
    # disabled for now since this seems to be broken somehow
    # and creates distortion in the sound.
    #add_compile_options(-msse2 -msimd128)
endif()
# Lua 5.4.3
add_library(Lua
        lua/src/lapi.c
        lua/src/lauxlib.c
        lua/src/lbaselib.c
        lua/src/lcode.c
        lua/src/lcorolib.c
        lua/src/lctype.c
        lua/src/ldblib.c
        lua/src/ldebug.c
        lua/src/ldo.c
        lua/src/ldump.c
        lua/src/lfunc.c
        lua/src/lgc.c
        lua/src/linit.c
        lua/src/liolib.c
        lua/src/llex.c
        lua/src/lmathlib.c
        lua/src/lmem.c
        lua/src/loadlib.c
        lua/src/lobject.c
        lua/src/lopcodes.c
        lua/src/loslib.c
        lua/src/lparser.c
        lua/src/lstate.c
        lua/src/lstring.c
        lua/src/lstrlib.c
        lua/src/ltable.c
        lua/src/ltablib.c
        lua/src/ltm.c
        lua/src/lua.c
        lua/src/luac.c
        lua/src/lundump.c
        lua/src/lutf8lib.c
        lua/src/lvm.c
        lua/src/lzio.c)
if (UNIX)
    target_compile_options(Lua PRIVATE -fPIC)
endif()
# box2d
add_library(box2d
        box2d/src/collision/b2_broad_phase.cpp
        box2d/src/collision/b2_chain_shape.cpp
        box2d/src/collision/b2_circle_shape.cpp
        box2d/src/collision/b2_collide_circle.cpp
        box2d/src/collision/b2_collide_edge.cpp
        box2d/src/collision/b2_collide_polygon.cpp
        box2d/src/collision/b2_collision.cpp
        box2d/src/collision/b2_distance.cpp
        box2d/src/collision/b2_dynamic_tree.cpp
        box2d/src/collision/b2_edge_shape.cpp
        box2d/src/collision/b2_polygon_shape.cpp
        box2d/src/collision/b2_time_of_impact.cpp
        box2d/src/common/b2_block_allocator.cpp
        box2d/src/common/b2_draw.cpp
        box2d/src/common/b2_math.cpp
        box2d/src/common/b2_settings.cpp
        box2d/src/common/b2_stack_allocator.cpp
        box2d/src/common/b2_timer.cpp
        box2d/src/dynamics/b2_body.cpp
        box2d/src/dynamics/b2_chain_circle_contact.cpp
        box2d/src/dynamics/b2_chain_polygon_contact.cpp
        box2d/src/dynamics/b2_circle_contact.cpp
        box2d/src/dynamics/b2_contact.cpp
        box2d/src/dynamics/b2_contact_manager.cpp
        box2d/src/dynamics/b2_contact_solver.cpp
        box2d/src/dynamics/b2_distance_joint.cpp
        box2d/src/dynamics/b2_edge_circle_contact.cpp
        box2d/src/dynamics/b2_edge_polygon_contact.cpp
        box2d/src/dynamics/b2_fixture.cpp
        box2d/src/dynamics/b2_friction_joint.cpp
        box2d/src/dynamics/b2_gear_joint.cpp
        box2d/src/dynamics/b2_island.cpp
        box2d/src/dynamics/b2_joint.cpp
        box2d/src/dynamics/b2_motor_joint.cpp
        box2d/src/dynamics/b2_mouse_joint.cpp
        box2d/src/dynamics/b2_polygon_circle_contact.cpp
        box2d/src/dynamics/b2_polygon_contact.cpp
        box2d/src/dynamics/b2_prismatic_joint.cpp
        box2d/src/dynamics/b2_pulley_joint.cpp
        box2d/src/dynamics/b2_revolute_joint.cpp
        box2d/src/dynamics/b2_weld_joint.cpp
        box2d/src/dynamics/b2_wheel_joint.cpp
        box2d/src/dynamics/b2_world.cpp
        box2d/src/dynamics/b2_world_callbacks.cpp
        box2d/src/rope/b2_rope.cpp)
target_include_directories(box2d PRIVATE "${CMAKE_CURRENT_LIST_DIR}/box2d/src")
target_include_directories(box2d PRIVATE "${CMAKE_CURRENT_LIST_DIR}/box2d/include")
if (UNIX)
    target_compile_options(box2d PRIVATE -fPIC)
endif()

# libogg 1.3.5
# libvorbis 1.3.7
# flac 1.3.2
# sndfile 1.0.31

# todo: check the SSE2/3 etc extensions

add_library(ogg
        libogg/src/bitwise.c
        libogg/src/framing.c
        )
target_include_directories(ogg PRIVATE "${CMAKE_CURRENT_LIST_DIR}/libogg/include")

add_library(vorbis
        libvorbis/lib/os.cpp
        libvorbis/lib/mdct.c
        libvorbis/lib/smallft.c
        libvorbis/lib/block.c
        libvorbis/lib/envelope.c
        libvorbis/lib/window.c
        libvorbis/lib/lsp.c
        libvorbis/lib/lpc.c
        libvorbis/lib/analysis.c
        libvorbis/lib/synthesis.c
        libvorbis/lib/psy.c
        libvorbis/lib/info.c
        libvorbis/lib/floor1.c
        libvorbis/lib/floor0.c
        libvorbis/lib/res0.c
        libvorbis/lib/mapping0.c
        libvorbis/lib/registry.c
        libvorbis/lib/codebook.c
        libvorbis/lib/sharedbook.c
        libvorbis/lib/lookup.c
        libvorbis/lib/bitrate.c
        libvorbis/lib/vorbisenc.c)
target_include_directories(vorbis PRIVATE "${CMAKE_CURRENT_LIST_DIR}/libvorbis/include")
target_include_directories(vorbis PRIVATE "${CMAKE_CURRENT_LIST_DIR}/libvorbis/lib")
target_include_directories(vorbis PRIVATE "${CMAKE_CURRENT_LIST_DIR}/libogg/include")
if (UNIX)
    target_compile_options(vorbis PRIVATE -fPIC)
endif()

add_library(flac
        libflac/src/libFLAC/bitmath.c
        libflac/src/libFLAC/bitreader.c
        libflac/src/libFLAC/bitwriter.c
        libflac/src/libFLAC/cpu.c
        libflac/src/libFLAC/crc.c
        libflac/src/libFLAC/fixed.c
        libflac/src/libFLAC/fixed_intrin_sse2.c
        libflac/src/libFLAC/fixed_intrin_ssse3.c
        libflac/src/libFLAC/float.c
        libflac/src/libFLAC/format.c
        libflac/src/libFLAC/lpc.c
        libflac/src/libFLAC/lpc_intrin_avx2.c
        libflac/src/libFLAC/lpc_intrin_sse.c
        libflac/src/libFLAC/lpc_intrin_sse2.c
        libflac/src/libFLAC/lpc_intrin_sse41.c
        libflac/src/libFLAC/md5.c
        libflac/src/libFLAC/memory.c
        libflac/src/libFLAC/metadata_iterators.c
        libflac/src/libFLAC/metadata_object.c
        libflac/src/libFLAC/ogg_decoder_aspect.c
        libflac/src/libFLAC/ogg_encoder_aspect.c
        libflac/src/libFLAC/ogg_helper.c
        libflac/src/libFLAC/ogg_mapping.c
        libflac/src/libFLAC/stream_decoder.c
        libflac/src/libFLAC/stream_encoder.c
        libflac/src/libFLAC/stream_encoder_framing.c
        libflac/src/libFLAC/stream_encoder_intrin_avx2.c
        libflac/src/libFLAC/stream_encoder_intrin_sse2.c
        libflac/src/libFLAC/stream_encoder_intrin_ssse3.c
        libflac/src/libFLAC/window.c
        libflac/src/libFLAC/windows_unicode_filenames.c)

target_include_directories(flac PRIVATE "${CMAKE_CURRENT_LIST_DIR}/libflac/include")
target_include_directories(flac PRIVATE "${CMAKE_CURRENT_LIST_DIR}/libflac/src")
target_include_directories(flac PRIVATE "${CMAKE_CURRENT_LIST_DIR}/libflac/src/libFLAC")
target_include_directories(flac PRIVATE "${CMAKE_CURRENT_LIST_DIR}/libflac/src/libFLAC/include")
target_include_directories(flac PRIVATE "${CMAKE_CURRENT_LIST_DIR}/libogg/include")
if (UNIX)
    target_compile_options(flac PRIVATE -fPIC)
endif()

target_compile_definitions(flac PRIVATE FLAC__NO_DLL)

# we want ogg and flac at least for sndfile.
# opus has been commented out manually.
# originally there's a config.h file that is generated by the
# convoluted build process that defines #define HAVE_EXTERNAL_XIPH_LIBS 1
# which is used by the build to build in the ogg and flac support
# sse2 should be turned on automatically

# todo: drop the gsm610, g72 and ALAC stuff

add_library(sndfile
        libsndfile/src/common.c
        libsndfile/src/file_io.c
        libsndfile/src/command.c
        libsndfile/src/pcm.c
        libsndfile/src/ulaw.c
        libsndfile/src/alaw.c
        libsndfile/src/float32.c
        libsndfile/src/double64.c
        libsndfile/src/ima_adpcm.c
        libsndfile/src/ms_adpcm.c
        libsndfile/src/gsm610.c
        libsndfile/src/dwvw.c
        libsndfile/src/vox_adpcm.c
        libsndfile/src/interleave.c
        libsndfile/src/strings.c
        libsndfile/src/dither.c
        libsndfile/src/cart.c
        libsndfile/src/broadcast.c
        libsndfile/src/audio_detect.c
        libsndfile/src/ima_oki_adpcm.c
        libsndfile/src/alac.c
        libsndfile/src/chunk.c
        libsndfile/src/ogg.c
        libsndfile/src/chanmap.c
        libsndfile/src/id3.c
        libsndfile/src/windows.c
        libsndfile/src/sndfile.c
        libsndfile/src/aiff.c
        libsndfile/src/au.c
        libsndfile/src/avr.c
        libsndfile/src/caf.c
        libsndfile/src/dwd.c
        libsndfile/src/flac.c
        libsndfile/src/g72x.c
        libsndfile/src/htk.c
        libsndfile/src/ircam.c
        libsndfile/src/macos.c
        libsndfile/src/mat4.c
        libsndfile/src/mat5.c
        libsndfile/src/nist.c
        libsndfile/src/paf.c
        libsndfile/src/pvf.c
        libsndfile/src/raw.c
        libsndfile/src/rx2.c
        libsndfile/src/sd2.c
        libsndfile/src/sds.c
        libsndfile/src/svx.c
        libsndfile/src/txw.c
        libsndfile/src/voc.c
        libsndfile/src/wve.c
        libsndfile/src/w64.c
        libsndfile/src/wavlike.c
        libsndfile/src/wav.c
        libsndfile/src/xi.c
        libsndfile/src/mpc2k.c
        libsndfile/src/rf64.c
        libsndfile/src/ogg_vorbis.c
        #libsndfile/src/ogg_speex.c
        libsndfile/src/ogg_pcm.c
        #libsndfile/src/ogg_opus.c
        libsndfile/src/ogg_vcomment.c
        libsndfile/src/nms_adpcm.c
        libsndfile/src/GSM610/add.c
        libsndfile/src/GSM610/code.c
        libsndfile/src/GSM610/decode.c
        libsndfile/src/GSM610/gsm_create.c
        libsndfile/src/GSM610/gsm_decode.c
        libsndfile/src/GSM610/gsm_destroy.c
        libsndfile/src/GSM610/gsm_encode.c
        libsndfile/src/GSM610/gsm_option.c
        libsndfile/src/GSM610/long_term.c
        libsndfile/src/GSM610/lpc.c
        libsndfile/src/GSM610/preprocess.c
        libsndfile/src/GSM610/rpe.c
        libsndfile/src/GSM610/short_term.c
        libsndfile/src/GSM610/table.c
        libsndfile/src/G72x/g721.c
        libsndfile/src/G72x/g723_16.c
        libsndfile/src/G72x/g723_24.c
        libsndfile/src/G72x/g723_40.c
        libsndfile/src/G72x/g72x.c
        libsndfile/src/ALAC/ALACBitUtilities.c
        libsndfile/src/ALAC/ag_dec.c
        libsndfile/src/ALAC/ag_enc.c
        libsndfile/src/ALAC/dp_dec.c
        libsndfile/src/ALAC/dp_enc.c
        libsndfile/src/ALAC/matrix_dec.c
        libsndfile/src/ALAC/matrix_enc.c
        libsndfile/src/ALAC/alac_decoder.c
        libsndfile/src/ALAC/alac_encoder.c
        libsndfile/src/ALAC/alac_decoder.c
        libsndfile/src/ALAC/alac_encoder.c
        )
target_include_directories(sndfile PRIVATE "${CMAKE_CURRENT_LIST_DIR}/libsndfile/include")
target_include_directories(sndfile PRIVATE "${CMAKE_CURRENT_LIST_DIR}/libsndfile/src")
target_include_directories(sndfile PRIVATE "${CMAKE_CURRENT_LIST_DIR}/libvorbis/include")
target_include_directories(sndfile PRIVATE "${CMAKE_CURRENT_LIST_DIR}/libflac/include")
target_include_directories(sndfile PRIVATE "${CMAKE_CURRENT_LIST_DIR}/libogg/include")
target_link_libraries(sndfile vorbis)
target_link_libraries(sndfile flac)
target_link_libraries(sndfile ogg)
target_compile_definitions(sndfile PRIVATE FLAC__NO_DLL)
if (UNIX)
    target_compile_options(sndfile PRIVATE -fPIC)
endif()

# libsamplerate
set(CPU_IS_LITTLE_ENDIAN 1)
add_library(samplerate
        libsamplerate/src/common.h
        libsamplerate/src/fastest_coeffs.h
        libsamplerate/src/high_qual_coeffs.h
        libsamplerate/src/mid_qual_coeffs.h
        libsamplerate/src/samplerate.c
        libsamplerate/src/src_linear.c
        libsamplerate/src/src_sinc.c
        libsamplerate/src/src_zoh.c)
target_include_directories(samplerate PRIVATE "${CMAKE_CURRENT_LIST_DIR}/libsamplerate/src")
target_include_directories(samplerate PRIVATE "${CMAKE_CURRENT_LIST_DIR}/libsamplerate/include")
target_compile_definitions(samplerate PRIVATE ENABLE_SINC_BEST_CONVERTER)
target_compile_definitions(samplerate PRIVATE ENABLE_SINC_FAST_CONVERTER)
target_compile_definitions(samplerate PRIVATE ENABLE_SINC_MEDIUM_CONVERTER)
