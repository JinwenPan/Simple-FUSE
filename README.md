# FUSE Filesystem

This project implements a custom userspace filesystem using the [FUSE](https://en.wikipedia.org/wiki/Filesystem_in_Userspace) (Filesystem in Userspace) library. The system is designed to be a lightweight and simple file system that can be mounted and used just like a traditional file system.

## Prerequisites

Before using the file system, ensure you have the following dependencies installed:

- **FUSE** (version 3.0)
- **GCC** (for compiling)
- **Make** (for building)

You can install FUSE version 3.0 using the following command (on a Debian-based system):

```bash
sudo apt-get install libfuse3-dev
```

## Build Instructions

1. Clone or download the repository:
```bash
git clone <repository_url>
cd <repository_directory>
```
2. Run `make` to compile the project:
```bash
make
```
3. After successful compilation, the executable `memfs` will be created.

## Usage
### Mounting the Filesystem
To mount the custom file system, run the following command:
```bash
./memfs <mount_point>
```
where `<mount_point>` is the directory where you want to mount the file system (make sure the directory exists).

### Example:
```bash
mkdir /tmp/my_mount
./memfs /tmp/my_mount
```
This will mount the custom file system at `/tmp/my_mount`.

### Unmounting the FileSystem
To unmount the file system, use the `fusermount` command:
```bash
fusermount -u /tmp/my_mount
```

## Features

1. The ability to create flat files and directories in the in-memory filesystem i.e, all files and directories stored in a single directory which is the root of the filesystem.

2. The ability to create hierarchical files and directories in the in-memory filesystem i.e, create files and directories in directories inside the root directory of the filesystem.

3. The ability to write data to and read data from files.

4. The ability to append data to an existing file.

5. The ability to create symlinks to files

6. The filesystem can return correct filesizes after write operations

7. The ability to restore its previous data after a crash

8. Maximum file name length of 255 ascii characters.

9. Maximum file size of 512 bytes.

10. High-level APIs are used. 