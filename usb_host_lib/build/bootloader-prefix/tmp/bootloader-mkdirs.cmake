# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "E:/app/espidf/Espressif/frameworks/esp-idf-v5.0.1/components/bootloader/subproject"
  "E:/study/esp/DEMO/usb/usb_host_lib/build/bootloader"
  "E:/study/esp/DEMO/usb/usb_host_lib/build/bootloader-prefix"
  "E:/study/esp/DEMO/usb/usb_host_lib/build/bootloader-prefix/tmp"
  "E:/study/esp/DEMO/usb/usb_host_lib/build/bootloader-prefix/src/bootloader-stamp"
  "E:/study/esp/DEMO/usb/usb_host_lib/build/bootloader-prefix/src"
  "E:/study/esp/DEMO/usb/usb_host_lib/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "E:/study/esp/DEMO/usb/usb_host_lib/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "E:/study/esp/DEMO/usb/usb_host_lib/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
