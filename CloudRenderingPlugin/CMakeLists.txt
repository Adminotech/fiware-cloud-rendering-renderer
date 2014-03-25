
# webrtc

file (TO_CMAKE_PATH "$ENV{WEBRTC_ROOT}" WEBRTC_ROOT)
if ("${WEBRTC_ROOT}" STREQUAL "")
    message(FATAL_ERROR "WEBRTC_ROOT environment variable missing! Point it to the webrtc repo root.")
endif ()

set (WEBRTC_INCLUDE_DIRS ${WEBRTC_ROOT} ${WEBRTC_ROOT}/third_party/protobuf/src)
if (NOT APPLE)
    set (WEBRTC_LIBRARY_DIRS ${WEBRTC_ROOT}/build/Release/lib)
else()
    set (WEBRTC_LIBRARY_DIRS ${WEBRTC_ROOT}/xcodebuild/Release)
endif()


# This library listing might seem overly complicated, but there is no sensible auto linking done in webrtc.
# We need to know all of the cross dependencies any module might have and link to everything.
# TODO: Verify this list is comprehensive enough on linux/mac builds.

if (NOT APPLE)
    set (WEBRTC_SOURCES ${WEBRTC_ROOT}/build/Release/obj/global_intermediate/protoc_out/webrtc/audio_processing/debug.pb.h
                        ${WEBRTC_ROOT}/build/Release/obj/global_intermediate/protoc_out/webrtc/audio_processing/debug.pb.cc)
    set (WEBRTC_LIBRARIES
        libjingle
        libjingle_p2p 
        libjingle_media
        libjingle_sound
        libjingle_peerconnection

        common_video
        video_render_module
        video_capture_module
        video_engine_core
        video_processing
        video_coding_utility

        common_audio
        audio_coding_module
        audio_conference_mixer
        audio_device
        audio_processing   
        voice_engine

        ${WEBRTC_LIBRARY_DIRS}/../libyuv.lib
        libsrtp
        libjpeg
        expat
        jsoncpp
        
        acm2
        opus
        CNG
        G711
        G722
        iLBC
        iSAC
        iSACFix
        PCM16B
        NetEq
        NetEq4

        system_wrappers
        directshow_baseclasses
        media_file
        paced_sender
        rtp_rtcp
        
        bitrate_controller
        remote_bitrate_estimator
        rbe_components

        webrtc_utility
        webrtc_video_coding
        webrtc_i420
        webrtc_opus
        webrtc_vp8

        libvpx
        libvpx_asm_offsets_vp8
        libvpx_intrinsics_mmx
        libvpx_intrinsics_sse2
        libvpx_intrinsics_ssse3

        common_audio_sse2
        audio_processing_sse2
        video_processing_sse2
        desktop_capture_differ_sse2

        nss_static
        crypt32
        iphlpapi
        secur32
        dmoguids
        wmcodecdspuuid
        amstrmid
        msdmo

        protobuf_full_do_not_use
    )
else()
    set (WEBRTC_SOURCES ${WEBRTC_ROOT}/xcodebuild/DerivedSources/Release/protoc_out/webrtc/audio_processing/debug.pb.h
                        ${WEBRTC_ROOT}/xcodebuild/DerivedSources/Release/protoc_out/webrtc/audio_processing/debug.pb.cc)
    set (WEBRTC_LIBRARIES
        CNG
        G711
        G722
        NetEq
        NetEq4
        NetEq4TestTools
        NetEqTestTools
        PCM16B
        acm2
        audio_coding_module
        audio_conference_mixer
        audio_device
        audio_processing
        audio_processing_sse2
        audioproc_debug_proto
        audioproc_unittest_proto
        bitrate_controller
        bwe_tools_util
        channel_transport
        command_line_parser
        common_audio
        common_audio_sse2
        common_video
        crnspr
        crnss
        crssl
        desktop_capture
        desktop_capture_differ_sse2
        expat
        frame_editing_lib
        frame_generator
        genperf_libs
        gflags
        gmock
        gtest
        gunit
        iLBC
        iSAC
        iSACFix
        icudata
        icui18n
        icuuc
        jingle
        jingle_media
        jingle_p2p
        jingle_peerconnection
        jingle_peerconnection_objc
        jingle_sound
        jingle_unittest_main
        jingle_xmpphelp
        jpeg_turbo
        jsoncpp
        media_file
        metrics
        neteq_unittest_tools
        nss_static
        opus
        paced_sender
        protobuf_full_do_not_use
        protobuf_lite
        rbe_components
        remote_bitrate_estimator
        rtcp_packet_parser
        rtp_rtcp
        sqlite3
        srtp
        system_wrappers
        test_framework
        test_support
        test_support_main
        test_support_main_threaded_mac
        usrsctplib
        video_capture_module
        video_codecs_test_framework
        video_coding_utility
        video_engine_core
        video_processing
        video_processing_sse2
        video_quality_analysis
        video_render_module
        vietest
        voice_engine
        vpx
        vpx_asm_offsets_vp8
        vpx_intrinsics_mmx
        vpx_intrinsics_sse2
        vpx_intrinsics_ssse3
        webrtc
        webrtc_i420
        webrtc_opus
        webrtc_test_common
        webrtc_utility
        webrtc_video_coding
        webrtc_vp8
        yuv
    )
endif()

# boost

if (TUNDRA_NO_BOOST)
    configure_boost()
endif ()

#################################################################

# Define target name and output directory
init_target (CloudRenderingPlugin OUTPUT plugins)

# Define source files
file (GLOB CPP_FILES *.cpp)
file (GLOB H_FILES *.h)
file (GLOB MOC_FILES CloudRenderingPlugin.h CloudRenderingProtocol.h WebRTCRenderer.h
                     WebRTCClient.h WebRTCWebSocketClient.h WebRTCPeerConnection.h 
                     WebRTCVideoRenderer.h WebRTCTundraCapturer.h)

QT4_WRAP_CPP (MOC_SRCS ${MOC_FILES})

MocFolder ()

# Includes
UseTundraCore ()
use_core_modules (TundraCore OgreRenderingModule Math)

add_definitions (-DCloudRenderingPlugin_EXPORTS)
if (APPLE)
    add_definitions (-DPOSIX)
endif()

include_directories (${WEBRTC_INCLUDE_DIRS} ${ENV_TUNDRA_DEP_PATH}/websocketpp)
link_directories (${WEBRTC_LIBRARY_DIRS})

build_library (${TARGET_NAME} SHARED ${CPP_FILES} ${H_FILES} ${MOC_SRCS} ${WEBRTC_SOURCES})

target_link_libraries (${TARGET_NAME} ${WEBRTC_LIBRARIES})

if (APPLE)
    target_link_libraries(${TARGET_NAME} ${Boost_LIBRARY_DIRS}/libboost_random.a)
    target_link_libraries(${TARGET_NAME} ${ENV_TUNDRA_DEP_PATH}/protobuf/lib/libprotobuf.7.dylib)
endif()

if (WIN32)
    target_link_libraries (${TARGET_NAME} winmm.lib ws2_32.lib)
    if (DirectX_FOUND)
        target_link_libraries (${TARGET_NAME} optimized d3dx9.lib debug d3dx9d.lib)
    endif ()
endif ()

# Linking
link_ogre ()
link_modules (TundraCore OgreRenderingModule Math)

SetupCompileFlags ()
final_target ()
