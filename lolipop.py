import sys
import os, os.path
import urllib.request
import json
import shutil
import platform
import hashlib

CONFIG = {
  "install_path": "./pkg",
  "catalog_url": "http://txlyre.ml/files/loli/pkg/catalog.json",
}
CONFIG["catalog_path"] = os.path.join(CONFIG["install_path"], "catalog_local.json")

if not os.path.isdir(CONFIG["install_path"]):
  os.mkdir(CONFIG["install_path"])

def _error(msg):
  print("[!] {}".format(msg))
  sys.exit(1)
  
def _msg(msg):
  print("[i] {}".format(msg))

def _format_file_size(num, suffix='B'):
  for unit in ['','Ki','Mi','Gi','Ti','Pi','Ei','Zi']:
    if abs(num) < 1024.0:
      return "%3.1f%s%s" % (num, unit, suffix)
    num /= 1024.0
  return "%.1f%s%s" % (num, 'Yi', suffix)
  
def _get_hash(path):
  if not os.path.isfile(path):
    _error("{}: not a file".format(path))
  sha1 = hashlib.sha1()
  with open(path, 'rb') as f:
    while True:
      data = f.read(65536)
      if not data:
        break
      sha1.update(data)
  return sha1.hexdigest()
  
def _check_file(path, hash):
  if not os.path.isfile(path):
    _error("{}: not a file".format(path))
    
  if _get_hash(path) != hash:
    _error("{}: checking fail: cheksum does not match expected\n\tGot: {}\n\tExpected: {}".format(path,_get_hash(path),hash))
    
def _download_file(path, url):
  file_name = path
  u = urllib.request.urlopen(url)
  f = open(file_name, 'wb')
  meta = u.info()
  file_size = int(u.getheader('Content-Length'))
  _msg("Downloading {} to {} from {}".format(_format_file_size(file_size), file_name,url))

  file_size_dl = 0
  block_sz = 8192
  while True:
    buffer = u.read(block_sz)
    if not buffer:
      break

    file_size_dl += len(buffer)
    f.write(buffer)
    status = r"%10s  [%3.2f%%]" % (_format_file_size(file_size_dl), file_size_dl * 100. / file_size)
    status = status + chr(8)*(len(status)+1)
    sys.stdout.write("\r"+status)
    sys.stdout.flush()

  f.close()
  
def _download_safe(local_name, url):
  if os.path.isfile(local_name):
    os.remove(local_name)
  try:
    _download_file(local_name, url)
  except Exception as e:
    _error("{}: download failed: {}".format(local_name,e))
  
  if not os.path.isfile(local_name):
    _error("{}: download failed".format(local_name))
  

def _check_catalog():
  if not os.path.isfile(CONFIG["catalog_path"]):
    return False
  try:
    json.load(open(CONFIG["catalog_path"], "r"))
  except Exception as e:
    return False
  return True
  
def _is_package_installed(package_name):
  package_name = package_name.strip().lower()
  bin_suffix = ".so"
  plat = platform.system().lower()  
  if plat == "windows":
    bin_suffix = ".dll"
  return os.path.isfile(os.path.join(CONFIG["install_path"], package_name, package_name+bin_suffix))
  
def action_update():
  _download_safe(CONFIG["catalog_path"], CONFIG["catalog_url"])
  
def action_install(package_name):
  package_name = package_name.strip().lower()
  if _is_package_installed(package_name):
    _error("package '{}' already installed".format(package_name))
  if not _check_catalog():
    action_update()
  
  try:
    catalog = json.load(open(CONFIG["catalog_path"], "r"))
  except Exception as e:
    _error("catalog parsing error: {}".format(e))
    
  if type(catalog) != list:
    _error("invalid catalog")
  
  package_info = None
  for package in catalog:
    if type(package) != dict or "name" not in package:
      _error("invalid catalog")
    
    if package["name"] == package_name:
      package_info = package
  
  if package_info == None:
    _error("could not find a package named '{}'".format(package_name))
  
  bin_suffix = ".so"
  plat = platform.system().lower()  
  if plat == "windows":
    bin_suffix = ".dll"
  
  if "binaries" not in package_info:
    _error("invalid catalog")
    
  if "requires" in package_info:
    for require in package_info["requires"]:
      if not _is_package_installed(require):
        _msg("found required package '{}'".format(require))
        action_install(require)
  
  if plat not in package_info["binaries"]:
    _error("there is no '{}' binary for your platform({})".format(package_name, plat))
    
  if "checksum" not in package_info["binaries"][plat]:
    _error("invalid catalog")
    
  package_local_path = os.path.join(CONFIG["install_path"], package_name, package_name+bin_suffix)
  
  if not os.path.isdir(os.path.join(CONFIG["install_path"], package_name)):
    os.mkdir(os.path.join(CONFIG["install_path"], package_name))
  
  hash = package_info["binaries"][plat]["checksum"]
  
  _download_safe(package_local_path, package_info["binaries"][plat]["url"])
  _check_file(package_local_path, hash)
  
  if not _is_package_installed(package_name):
    _error("installing failed due unknown error")
  
  _msg("successfuly installed package '{}'".format(package_name))
  
def action_uninstall(package_name):
  package_name = package_name.strip().lower()
  if not _is_package_installed(package_name):
    _error("cannot uninstall non-installed package '{}'".format(package_name))
  
  shutil.rmtree(os.path.join(CONFIG["install_path"], package_name))
  
  if _is_package_installed(package_name):
    _error("uninstalling failed due unknown error")
  
  _msg("successfuly uninstalled package '{}'".format(package_name))
  
def action_list():
  bin_suffix = ".so"
  plat = platform.system().lower()  
  if plat == "windows":
    bin_suffix = ".dll"
  packages = os.listdir(CONFIG["install_path"])
  for name in packages:
    if name != "catalog_local.json":
      if os.path.isdir(os.path.join(CONFIG["install_path"], name)):
        if os.path.isfile(os.path.join(CONFIG["install_path"], name, name+bin_suffix)):
          print(name)
  

def print_usage(code):
  print("""Usage: lolipop.py <install|uninstall|update|list> [package-name]""")
  sys.exit(code)
  
args = sys.argv
argc = len(args)

if argc <= 1:
  print_usage(1)

if argc >= 2:
  action = args[1].lower().strip()
  
  if action == "install":
    if argc < 3:
      print_usage(1)
    
    action_install(args[2])
  elif action == "uninstall":
    if argc < 3:
      print_usage(1)
    
    action_uninstall(args[2])
  elif action == "update":
    action_update()
  elif action == "list":
    action_list()
  else:
    print_usage(1)
  
    