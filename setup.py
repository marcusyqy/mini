#!/usr/bin/env python3
import os
from extra import Vulkan_Installer

includes = [ "mini", "extra/imgui/imgui", "extra/glfw/include", "extra/adapter", "extra/volk" ]

def download_vulkan_if_not_available():
  # TODO: Download vulkan.
  vulkan = Vulkan_Installer()
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
  print("Generating compile_flags.txt...")
  compile_flags=""
  for i in range(0, len(includes)):
    compile_flags += "-I" + includes[i] + "\n"

  assert os.environ.__contains__("VULKAN_SDK"), "Vulkan SDK needs to be downloaded since this project relies on it."
  compile_flags += "-I" + process_win32_env(os.environ["VULKAN_SDK"]) + "/Include\n"

  compile_flags += "-std=c++17"
  f = open("compile_flags.txt", "w")
  f.write(compile_flags)

if __name__ == "__main__":
  download_vulkan_if_not_available()
  generate_compile_commands()
