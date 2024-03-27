#!/usr/bin/env python3
import os
import sys
from extra.install.vulkan import Vulkan

includes = [ "mini", "deps/imgui/imgui", "deps/glfw/include", "extra/adapter" ]

def download_vulkan_if_not_available():
  # TODO: Download vulkan.
  vulkan = Vulkan()
  if not vulkan.check_installed():
    vulkan.install()

def process_win32_env(str):
  arr = str.split("\\")
  ret=""
  for i in range(0, len(arr)):
    ret += arr[i]
    if(i+1 != len(arr)):
      ret += "\\\\"

  return ret

def generate_compile_commands():
  compile_flags=""
  for i in range(0, len(includes)):
    compile_flags += "-I" + includes[i] + "\n"

  download_vulkan_if_not_available()
  compile_flags += "-I" + process_win32_env(os.environ["VULKAN_SDK"]) + "/Include\n"

  compile_flags += "-std=c++17"
  f = open("compile_flags.txt", "w")
  f.write(compile_flags)

if __name__ == "__main__":
  generate_compile_commands()
