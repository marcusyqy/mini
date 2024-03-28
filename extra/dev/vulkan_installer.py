#!/usr/bin/env python3
import os

class Vulkan_Installer:
  def check_installed(self):
    return os.environ.__contains__("VULKAN_SDK")
  def install(self):
    assert self.check_installed(), "Does not contain Vulkan SDK & don't have logic to download it yet"
    pass

if __name__ == "__main__":
  vulkan = Vulkan_Installer()
  if not vulkan.check_installed():
    vulkan.install()
