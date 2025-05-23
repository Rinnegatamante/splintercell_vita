cmake_minimum_required(VERSION 2.8)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
  endif()
endif()

project(splintercell C CXX)
include("${VITASDK}/share/vita.cmake" REQUIRED)
set(VITA_APP_NAME "Splinter Cell: Conviction")
set(VITA_TITLEID  "SPLINTERC")
set(VITA_VERSION  "01.00")
set(VITA_MKSFOEX_FLAGS "-d ATTRIBUTE2=12")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Wl,-q -D_GNU_SOURCE -Wall -O3 -mfloat-abi=softfp")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -std=c++11 -Wno-write-strings")

add_executable(splintercell
  loader/main.c
  loader/dialog.c
  loader/so_util.c
  loader/bionic_ctype.c
  loader/video_player.c
  loader/audio_player.cpp
)

target_link_libraries(splintercell
  -Wl,--whole-archive pthread -Wl,--no-whole-archive
  soloud_static
  SDL2_mixer_ext
  SDL2
  vorbisfile
  vorbis
  ogg
  mpg123
  opusfile
  opus
  modplug
  mikmod
  FLAC
  stdc++
  openal
  vitaGL
  vitashark
  SceShaccCgExt
  m
  mathneon
  taihen_stub
  kubridge_stub
  SceHid_stub
  SceMotion_stub
  SceIme_stub
  SceAvPlayer_stub
  SceAppMgr_stub
  SceAudio_stub
  SceAudioIn_stub
  SceCtrl_stub
  SceCommonDialog_stub
  SceDisplay_stub
  SceKernelDmacMgr_stub
  SceGxm_stub
  SceShaccCg_stub
  SceSysmodule_stub
  ScePower_stub
  SceTouch_stub
  SceVshBridge_stub
  SceAppUtil_stub
)

add_executable(videoplayer
  loader/main.c
  loader/dialog.c
  loader/so_util.c
  loader/bionic_ctype.c
  loader/video_player.c
  loader/audio_player.cpp
)

target_link_libraries(videoplayer
  -Wl,--whole-archive pthread -Wl,--no-whole-archive
  soloud_static
  SDL2_mixer_ext
  SDL2
  vorbisfile
  vorbis
  ogg
  mpg123
  opusfile
  opus
  modplug
  mikmod
  FLAC
  stdc++
  openal
  vitaGL
  vitashark
  SceShaccCgExt
  m
  mathneon
  taihen_stub
  kubridge_stub
  SceHid_stub
  SceMotion_stub
  SceIme_stub
  SceAvPlayer_stub
  SceAppMgr_stub
  SceAudio_stub
  SceAudioIn_stub
  SceCtrl_stub
  SceCommonDialog_stub
  SceDisplay_stub
  SceKernelDmacMgr_stub
  SceGxm_stub
  SceShaccCg_stub
  SceSysmodule_stub
  ScePower_stub
  SceTouch_stub
  SceVshBridge_stub
  SceAppUtil_stub
)

target_compile_options(videoplayer PRIVATE "-DEXCLUSIVE_VIDEO_PLAYER") 

add_executable(companion
  companion/main.cpp
)

target_link_libraries(companion
  imgui
  vitaGL
  vitashark
  SceAppUtil_stub
  SceShaccCgExt
  taihen_stub
  mathneon
  SceAppMgr_stub
  SceAudio_stub
  SceCtrl_stub
  SceCommonDialog_stub
  SceDisplay_stub
  SceGxm_stub
  SceShaccCg_stub
  SceSysmodule_stub
  SceTouch_stub
  SceKernelDmacMgr_stub
)

vita_create_self(companion.bin companion UNSAFE)
vita_create_self(eboot.bin videoplayer UNSAFE)
vita_create_self(main.bin splintercell UNSAFE)
vita_create_vpk(splintercell.vpk ${VITA_TITLEID} eboot.bin
  VERSION ${VITA_VERSION}
  NAME ${VITA_APP_NAME}
  FILE ${CMAKE_SOURCE_DIR}/livearea/icon0.png sce_sys/icon0.png
       ${CMAKE_SOURCE_DIR}/livearea/pic0.png sce_sys/pic0.png
       ${CMAKE_SOURCE_DIR}/livearea/bg0.png sce_sys/livearea/contents/bg0.png
       ${CMAKE_SOURCE_DIR}/livearea/startup.png sce_sys/livearea/contents/startup.png
       ${CMAKE_SOURCE_DIR}/livearea/template.xml sce_sys/livearea/contents/template.xml
       ${CMAKE_SOURCE_DIR}/livearea/config.png sce_sys/livearea/contents/config.png
       ${CMAKE_BINARY_DIR}/companion.bin companion.bin
       ${CMAKE_BINARY_DIR}/main.bin main.bin
       ${CMAKE_SOURCE_DIR}/presets presets
       ${CMAKE_SOURCE_DIR}/fog fog
       ${CMAKE_SOURCE_DIR}/antialiasing antialiasing
)
