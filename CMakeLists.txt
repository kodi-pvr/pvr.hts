cmake_minimum_required(VERSION 3.5)
project(pvr.hts)

find_package(Kodi REQUIRED)
find_package(p8-platform REQUIRED)

include_directories(${p8-platform_INCLUDE_DIRS}
                    ${KODI_INCLUDE_DIR}/.. # Hack way with "/..", need bigger Kodi cmake rework to match right include ways
                    ${PROJECT_SOURCE_DIR}/lib)

# Sources and headers
set(HTS_SOURCES src/addon.h
                src/addon.cpp
                src/Tvheadend.cpp
                src/Tvheadend.h)

set(HTS_SOURCES_TVHEADEND
                src/tvheadend/AutoRecordings.cpp
                src/tvheadend/AutoRecordings.h
                src/tvheadend/ChannelTuningPredictor.h
                src/tvheadend/ChannelTuningPredictor.cpp
                src/tvheadend/HTSPConnection.h
                src/tvheadend/HTSPConnection.cpp
                src/tvheadend/HTSPDemuxer.h
                src/tvheadend/HTSPDemuxer.cpp
                src/tvheadend/HTSPMessage.h
                src/tvheadend/HTSPTypes.h
                src/tvheadend/HTSPVFS.h
                src/tvheadend/HTSPVFS.cpp
                src/tvheadend/IHTSPConnectionListener.h
                src/tvheadend/IHTSPDemuxPacketHandler.h
                src/tvheadend/Profile.h
                src/tvheadend/Settings.cpp
                src/tvheadend/Settings.h
                src/tvheadend/Subscription.cpp
                src/tvheadend/Subscription.h
                src/tvheadend/TimeRecordings.cpp
                src/tvheadend/TimeRecordings.h)

set(HTS_SOURCES_TVHEADEND_ENTITY
                src/tvheadend/entity/AutoRecording.h
                src/tvheadend/entity/AutoRecording.cpp
                src/tvheadend/entity/Channel.h
                src/tvheadend/entity/Entity.h
                src/tvheadend/entity/Event.h
                src/tvheadend/entity/Event.cpp
                src/tvheadend/entity/Recording.h
                src/tvheadend/entity/RecordingBase.h
                src/tvheadend/entity/RecordingBase.cpp
                src/tvheadend/entity/Schedule.h
                src/tvheadend/entity/Schedule.cpp
                src/tvheadend/entity/Tag.h
                src/tvheadend/entity/Tag.cpp
                src/tvheadend/entity/TimeRecording.h
                src/tvheadend/entity/TimeRecording.cpp)

set(HTS_SOURCES_TVHEADEND_STATUS
                src/tvheadend/status/DescrambleInfo.h
                src/tvheadend/status/DescrambleInfo.cpp
                src/tvheadend/status/Quality.h
                src/tvheadend/status/QueueStatus.h
                src/tvheadend/status/SourceInfo.h
                src/tvheadend/status/TimeshiftStatus.h)

set(HTS_SOURCES_TVHEADEND_UTILITIES
                src/tvheadend/utilities/Utilities.h
                src/tvheadend/utilities/Logger.h
                src/tvheadend/utilities/Logger.cpp
                src/tvheadend/utilities/LifetimeMapper.h
                src/tvheadend/utilities/AsyncState.cpp
                src/tvheadend/utilities/AsyncState.h)

source_group("Source Files" FILES ${HTS_SOURCES})
source_group("Source Files\\tvheadend" FILES ${HTS_SOURCES_TVHEADEND})
source_group("Source Files\\tvheadend\\entity" FILES ${HTS_SOURCES_TVHEADEND_ENTITY})
source_group("Source Files\\tvheadend\\status" FILES ${HTS_SOURCES_TVHEADEND_STATUS})
source_group("Source Files\\tvheadend\\utilities" FILES ${HTS_SOURCES_TVHEADEND_UTILITIES})

# Resource files
set(HTS_RESOURCES
                README.md
                pvr.hts/addon.xml
                pvr.hts/changelog.txt
                pvr.hts/icon.png
                pvr.hts/resources/settings.xml
                pvr.hts/resources/language/resource.language.en_gb/strings.po)

source_group("Resource Files" FILES ${HTS_RESOURCES})

# Combine the file lists
list(APPEND HTS_SOURCES
            ${HTS_SOURCES_TVHEADEND}
            ${HTS_SOURCES_TVHEADEND_ENTITY}
            ${HTS_SOURCES_TVHEADEND_STATUS}
            ${HTS_SOURCES_TVHEADEND_UTILITIES})

add_subdirectory(lib/libhts)

set(DEPLIBS ${p8-platform_LIBRARIES} hts)
if(WIN32)
  list(APPEND DEPLIBS ws2_32)
endif()

build_addon(pvr.hts HTS DEPLIBS)

include(CPack)
