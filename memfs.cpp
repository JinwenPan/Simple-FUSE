// Reference:
// https://www.maastaar.net/fuse/linux/filesystem/c/2019/09/28/writing-less-simple-yet-stupid-filesystem-using-FUSE-in-C/
// https://www.cs.nmsu.edu/~pfeiffer/fuse-tutorial/html/index.html/
// https://libfuse.github.io/doxygen/files.html/
// https://ouonline.net/building-your-own-fs-with-fuse-1/

#define FUSE_USE_VERSION 30

#include <fuse.h>
#include <vector>
#include <unordered_map>
#include <string>
#include <cstring>
#include <errno.h>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <cstdio>
#include <cstdlib>

// 255 of file name doesn't include '\0'
#define MAX_NAME_LENGTH 255
#define MAX_FILE_SIZE 512

enum file_types {
    DIRECTORY,
    REGULAR,
    SYMLINK,
};

struct inode {
    file_types file_type;
    std::string file_name;
    std::vector<std::string> children_files;
    std::vector<char> file_data;
    std::string target_path;
};

std::unordered_map<std::string, inode> my_memfs;

std::string mount_point;
std::string mount_point_parent;
std::string files_backup_path;
std::string dirs_backup_path;
std::string links_backup_path;


std::string get_full_path(const std::string& path_name){
    return mount_point + path_name;
}

// "/" -> ""
std::string get_file_name(const std::string& path_name){
    auto last_slash_index = path_name.find_last_of('/');
    if (last_slash_index != std::string::npos && path_name.length() > 1) return path_name.substr(last_slash_index+1);
    else if (last_slash_index == 0) return "";
    else return path_name;
}

// "/" -> "/"
std::string get_parent_path(const std::string& path_name){
    auto last_slash_index = path_name.find_last_of('/');
    if (last_slash_index != 0) return path_name.substr(0, last_slash_index);
    else if (last_slash_index == 0) return "/";
    else return "";
}

void save_my_memfs(){
    std::ofstream out_file(files_backup_path, std::ios::out);
    std::ofstream out_dir(dirs_backup_path, std::ios::out);
    std::ofstream out_link(links_backup_path, std::ios::out);

    if(out_file.is_open() && out_dir.is_open() && out_link.is_open()){
        for(const auto& map : my_memfs){
            switch(map.second.file_type){
                case REGULAR:
                    out_file << map.first << std::endl;
                    for (char c : map.second.file_data){
                        out_file << c;
                    }
                    out_file << std::endl;
                    break;
                case DIRECTORY:
                    out_dir << map.first << std::endl;
                    for (const auto& name : map.second.children_files){
                        out_dir << name;
                        out_dir << '?';
                    }
                    out_dir << std::endl;
                    break;
                case SYMLINK:
                    out_dir << map.first << std::endl;
                    out_dir << map.second.target_path << std::endl;
                    break;
            }
            
        }
        out_file.close();
        out_dir.close();
        out_link.close();
    }
    else std::cerr << "opening error" << std::endl;
}

void load_my_memfs(){
    std::ifstream in_file(files_backup_path, std::ios::in);

    if(in_file.is_open()){
        std::string line;
        int line_index = 0;
        inode new_node;
        std::string helper;
        while(std::getline(in_file, line)){
            if (line_index % 2 == 0) {
                my_memfs[line] = new_node;
                helper = line;
            }
            else { 
                for (char c : line) my_memfs[helper].file_data.push_back(c); 
                my_memfs[helper].file_name = get_file_name(helper);
                my_memfs[helper].file_type = REGULAR;
            }
            line_index++;
        } 
        in_file.close();
    }
    else std::cerr << "opening error" << std::endl;

    std::ifstream in_dir(dirs_backup_path, std::ios::in);

    if(in_dir.is_open()){
        std::string line;
        int line_index = 0;
        inode new_node;
        std::string helper;
        while(std::getline(in_dir, line)){
            if (line_index % 2 == 0) {
                my_memfs[line] = new_node;
                helper = line;
            }
            else {
                my_memfs[helper].file_type = DIRECTORY;
                my_memfs[helper].file_name = get_file_name(helper);
                size_t begin_index = 0;
                size_t end_index = 0;
                while ((end_index = line.find('?', begin_index)) != std::string::npos){
                    my_memfs[helper].children_files.push_back(line.substr(begin_index, end_index - begin_index));
                    begin_index = end_index + 1;
                }
            }
            line_index++;
        }
        in_dir.close();
    }
    else std::cerr << "opening error" << std::endl;

    std::ifstream in_link(links_backup_path, std::ios::in);

    if(in_link.is_open()){
        std::string line;
        int line_index = 0;
        inode new_node;
        std::string helper;
        while(std::getline(in_link, line)){
            if (line_index % 2 == 0) {
                my_memfs[line] = new_node;
                helper = line;
            }
            else {
                my_memfs[helper].file_type = SYMLINK;
                my_memfs[helper].file_name = get_file_name(helper);
                my_memfs[helper].target_path = line;
            }
            line_index++;
        }
        in_link.close();
    }
    else std::cerr << "opening error" << std::endl;
}

void delete_my_memfs(){
    std::remove(files_backup_path.c_str());
    std::remove(dirs_backup_path.c_str());
    std::remove(links_backup_path.c_str());
}

static int my_memfs_getattr(const char* path, struct stat* attr){
    if (my_memfs.find(path) != my_memfs.end()){
        switch(my_memfs[path].file_type){
            case DIRECTORY:
                attr->st_mode = S_IFDIR | 0755;
                attr->st_size = 0;
                break;
            case REGULAR:
                attr->st_mode = S_IFREG | 0644;
                attr->st_size = my_memfs[path].file_data.size();
                break;
            case SYMLINK:
                attr->st_mode = S_IFLNK | 0777;
                attr->st_size = my_memfs[my_memfs[path].target_path].file_name.length();
                break;
        }
        return 0;
    }
    return -ENOENT;
}

static int my_memfs_read(const char* path, char* buffer, size_t size, off_t offset, struct fuse_file_info* info){
    if (my_memfs.find(path) != my_memfs.end()){
        if (my_memfs[path].file_type == DIRECTORY) return -EISDIR;
        std::string target = path;
        if (my_memfs[path].file_type == SYMLINK) target = my_memfs[path].target_path; 
        off_t gap = my_memfs[target].file_data.size() - offset;
        if (my_memfs[target].file_data.empty() || gap <= 0){
            return 0;
        }
        else{
            off_t temp = size;
            size_t real_size = static_cast<size_t>(gap < temp ? gap : temp);
            memcpy(buffer, my_memfs[target].file_data.data() + offset, real_size);
            return real_size;
        }
    }
    return -ENOENT;
}

static int my_memfs_write(const char* path, const char* buffer, size_t size, off_t offset, struct fuse_file_info* info){
    if (my_memfs.find(path) != my_memfs.end()){
        switch (my_memfs[path].file_type){
            case REGULAR:{
                off_t data_length_reg = my_memfs[path].file_data.size();
                if ((size + data_length_reg) > MAX_FILE_SIZE) return -ENOSPC;
                if (my_memfs[path].file_data.empty() || (offset >= data_length_reg)){
                    for (size_t i = 0; i < size; ++i){
                        my_memfs[path].file_data.push_back(*buffer);
                        buffer++;
                    }
                }
                else {
                    my_memfs[path].file_data.insert(my_memfs[path].file_data.begin() + offset, buffer, buffer + size);
                }
                save_my_memfs();
                return size;
            }
            case SYMLINK:{
                std::string target = my_memfs[path].target_path;
                if(my_memfs[target].file_type == REGULAR){
                    off_t data_length_sym = my_memfs[target].file_data.size();
                    if ((size + data_length_sym) > MAX_FILE_SIZE) return -ENOSPC;
                    if (my_memfs[target].file_data.empty() || (offset >= data_length_sym)){
                        for (size_t i = 0; i < size; ++i){
                            my_memfs[target].file_data.push_back(*buffer);
                            buffer++;
                        }
                    }
                    else {
                        my_memfs[target].file_data.insert(my_memfs[target].file_data.begin() + offset, buffer, buffer + size);
                    }
                    save_my_memfs();                
                    return size;   
                }
                return -EISDIR;
            }
            case DIRECTORY:
                return -EISDIR;
        }
    }
    return -ENOENT;
}

static int my_memfs_readdir(const char* path, void* buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* info){
    if (my_memfs.find(path) != my_memfs.end()){
        if (my_memfs[path].file_type == REGULAR) return -ENOENT;
        if (my_memfs[path].file_type == SYMLINK) return -ENOENT;
        filler(buffer, ".", NULL, 0);
        filler(buffer, "..", NULL, 0);
        for (const std::string& child_file : my_memfs[path].children_files){
            filler(buffer, child_file.c_str(), NULL, 0);
        }
        return 0;
    }
    return -ENOENT;
}

static int my_memfs_mkdir(const char* path, mode_t mode){
    if (my_memfs.find(path) == my_memfs.end()){
        if (get_file_name(path).length() > MAX_NAME_LENGTH) return -ENAMETOOLONG;
        inode new_inode;
        my_memfs[path] = new_inode;
        my_memfs[path].file_name = get_file_name(path);
        my_memfs[path].file_type = DIRECTORY;
        my_memfs[get_parent_path(path)].children_files.push_back(my_memfs[path].file_name);
        save_my_memfs();
        return 0;
    }
    return -EEXIST;    
}

static int my_memfs_mknod(const char* path, mode_t mode, dev_t dev){
    if (my_memfs.find(path) == my_memfs.end()){
        if (get_file_name(path).length() > MAX_NAME_LENGTH) return -ENAMETOOLONG;
        inode new_inode;
        my_memfs[path] = new_inode;
        my_memfs[path].file_name = get_file_name(path);
        my_memfs[path].file_type = REGULAR;
        my_memfs[get_parent_path(path)].children_files.push_back(my_memfs[path].file_name);
        save_my_memfs();
        return 0;
    }
    return -EEXIST;
}

// In my file system, file descriptor is not needed for the following operations, so actually nothing special is implemented here.
static int my_memfs_open(const char* path, struct fuse_file_info* info){
    if (my_memfs.find(path) == my_memfs.end()) return -ENOENT;
    if (my_memfs[path].file_type == DIRECTORY) return -EISDIR;
    return 0;
}

static int my_memfs_create(const char* path, mode_t mode, struct fuse_file_info* info){
    if (my_memfs.find(path) == my_memfs.end()){
        if (get_file_name(path).length() > MAX_NAME_LENGTH) return -ENAMETOOLONG;
        inode new_inode;
        my_memfs[path] = new_inode;
        my_memfs[path].file_name = get_file_name(path);
        my_memfs[path].file_type = REGULAR;
        my_memfs[get_parent_path(path)].children_files.push_back(my_memfs[path].file_name);
        save_my_memfs();
        my_memfs_open(path, info);
        return 0;
    }
    return -EEXIST;
}

static int my_memfs_readlink(const char* path, char* buffer, size_t size){
    if(my_memfs.find(path) != my_memfs.end() && my_memfs[path].file_type == SYMLINK){
        strncpy(buffer, my_memfs[path].target_path.c_str(), size-1);
        (my_memfs[path].target_path.length() < size-1) ? (buffer[my_memfs[path].target_path.length()] = '\0') : (buffer[size-1] = '\0');
        return 0;
    }
    return -ENOENT;
}

static int my_memfs_symlink(const char* target_path, const char* link_path){
    if (my_memfs.find(link_path) == my_memfs.end()){
        if (get_file_name(link_path).length() > MAX_NAME_LENGTH) return -ENAMETOOLONG;
        inode new_inode;
        my_memfs[link_path] = new_inode;
        my_memfs[link_path].target_path = target_path;
        my_memfs[link_path].file_type = SYMLINK;
        my_memfs[link_path].file_name = get_file_name(link_path);
        my_memfs[get_parent_path(link_path)].children_files.push_back(my_memfs[link_path].file_name);
        save_my_memfs();
        return 0;
    }
    return -EEXIST;
}

static struct fuse_operations my_memfs_operations = {
    .getattr = my_memfs_getattr,
    .readlink = my_memfs_readlink,
    .mknod = my_memfs_mknod,
    .mkdir = my_memfs_mkdir,
    .symlink = my_memfs_symlink,
    .open = my_memfs_open,
    .read = my_memfs_read,
    .write = my_memfs_write,
    .readdir = my_memfs_readdir,
    .create = my_memfs_create,
};

int main(int argc, char** argv) {
    mount_point = argv[1];
    mount_point_parent = get_parent_path(mount_point);
    if (mount_point_parent == "/") {
        files_backup_path = mount_point_parent + "files_list";
        dirs_backup_path = mount_point_parent + "dirs_list";
        links_backup_path = mount_point_parent + "links_list";
    }
    files_backup_path = mount_point_parent + "/files_list";
    dirs_backup_path = mount_point_parent + "/dirs_list";
    links_backup_path = mount_point_parent + "/links_list";

    files_backup_path = "/tmp/files_list";
    dirs_backup_path = "/tmp/dirs_list";
    links_backup_path = "/tmp/links_list";

    atexit(delete_my_memfs);

    if (!std::filesystem::exists(files_backup_path) && !std::filesystem::exists(dirs_backup_path) && !std::filesystem::exists(links_backup_path)){
        inode root_node;
        root_node.file_type = DIRECTORY;
        root_node.file_name = "";
        my_memfs["/"] = root_node;
    }
    else load_my_memfs();

    return fuse_main(argc, argv, &my_memfs_operations, NULL);
}