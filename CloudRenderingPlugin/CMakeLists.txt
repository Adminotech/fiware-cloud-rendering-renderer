
#webrtc

file (TO_CMAKE_PATH "$ENV{WEBRTC_ROOT}" WEBRTC_ROOT)
if ("${WEBRTC_ROOT}" STREQUAL "")
    message(FATAL_ERROR "WEBRTC_ROOT environment variable missing! Point it to the webrtc repo root.")
endif ()

set (WEBRTC_INCLUDE_DIRS ${WEBRTC_ROOT} ${WEBRTC_ROOT}/third_party/protobuf/src)
set (WEBRTC_LIBRARY_DIRS ${WEBRTC_ROOT}/build/Release/lib)
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

set (WEBRTC_SOURCES ${WEBRTC_ROOT}/build/Release/obj/global_intermediate/protoc_out/webrtc/audio_processing/debug.pb.h
                    ${WEBRTC_ROOT}/build/Release/obj/global_intermediate/protoc_out/webrtc/audio_processing/debug.pb.cc)

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

include_directories (${WEBRTC_INCLUDE_DIRS} ${ENV_TUNDRA_DEP_PATH}/websocketpp)
link_directories (${WEBRTC_LIBRARY_DIRS})

build_library (${TARGET_NAME} SHARED ${CPP_FILES} ${H_FILES} ${MOC_SRCS} ${WEBRTC_SOURCES})

target_link_libraries (${TARGET_NAME} ${WEBRTC_LIBRARIES})

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
