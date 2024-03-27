import os
import sys

includes = [ "mini", "deps/imgui/imgui", "deps/glfw/include", "extra/adapter" ]

def download_vulkan_if_not_available():
  # TODO: Download vulkan.
  assert os.environ.__contains__("VULKAN_SDK"), "Does not contain Vulkan SDK & don't have logic to download it yet"
  pass

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