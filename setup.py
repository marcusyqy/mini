#!/usr/bin/env python3

# possible TODO is to make this also run the bat file? 
import os, sys
from extra import Vulkan_Installer

includes = [ "mini", "extra/imgui", "extra/glfw/include", "extra/adapter", "extra/volk", "extra/glm" ]

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
  for include in includes:
    compile_flags += "-I" + include + "\n"

  assert os.environ.__contains__("VULKAN_SDK"), "Vulkan SDK needs to be downloaded since this project relies on it."
  compile_flags += "-I" + process_win32_env(os.environ["VULKAN_SDK"]) + "/Include\n"

  compile_flags += "-std=c++17"
  f = open("compile_flags.txt", "w")
  f.write(compile_flags)


def need_setup():
  setup_py = "setup.py"
  compile_flags = "compile_flags.txt"
  return not os.path.exists(compile_flags) or os.path.getmtime(setup_py) > os.path.getmtime(compile_flags) 

if __name__ == "__main__":
  assert sys.platform == 'win32', "CURRENTLY MINI DOESN'T SUPPORT NON WINDOWS BUILD."
  if need_setup():
    print("--[REGENERATING COMPILE_FLAGS]--")
    download_vulkan_if_not_available()
    generate_compile_commands()

