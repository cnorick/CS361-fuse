//COSC 361 Spring 2017
//FUSE Project Template
//Group Name
//Group Member 1 Name
//Group Member 2 Name

#ifndef __cplusplus
#error "You must compile this using C++"
#endif
#include <fuse.h>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <fs.h>
#include <vector>
#include <string>
#include <list>
#include <queue>
#include <algorithm>

using namespace std;

//Use debugf() and NOT printf() for your messages.
//Uncomment #define DEBUG in block.h if you want messages to show

//Here is a list of error codes you can return for
//the fs_xxx() functions
//
//EPERM          1      /* Operation not permitted */
//ENOENT         2      /* No such file or directory */
//ESRCH          3      /* No such process */
//EINTR          4      /* Interrupted system call */
//EIO            5      /* I/O error */
//ENXIO          6      /* No such device or address */
//ENOMEM        12      /* Out of memory */
//EACCES        13      /* Permission denied */
//EFAULT        14      /* Bad address */
//EBUSY         16      /* Device or resource busy */
//EEXIST        17      /* File exists */
//ENOTDIR       20      /* Not a directory */
//EISDIR        21      /* Is a directory */
//EINVAL        22      /* Invalid argument */
//ENFILE        23      /* File table overflow */
//EMFILE        24      /* Too many open files */
//EFBIG         27      /* File too large */
//ENOSPC        28      /* No space left on device */
//ESPIPE        29      /* Illegal seek */
//EROFS         30      /* Read-only file system */
//EMLINK        31      /* Too many links */
//EPIPE         32      /* Broken pipe */
//ENOTEMPTY     36      /* Directory not empty */
//ENAMETOOLONG  40      /* The name given is too long */

list <string> Split(string abs_path);

typedef struct TreeNode {
        list<TreeNode*> children;
        NODE *node;
        TreeNode *parent;
        string relName;
        TreeNode(NODE *n, TreeNode *parent) : node(n), parent(parent) {
            relName = Split(n->name).back();
        }
} TreeNode;

TreeNode *getTreeNode(string path);
TreeNode *getTreeNode(list<string> path);
void printFileTree(TreeNode *root, int level = 0);
void insertNode(NODE *n);
list<TreeNode*> getChildren(const char* path);
NODE *getNodeFromId(uint64_t id);
uint64_t getNumBlocks(const NODE *n);
void addId(NODE *n);
uint64_t getAvailId();
void removeBlocks(NODE *n);
void addBlock(BLOCK *b);
void freeBlock(uint64_t offset);
void removeNode(NODE *n);

//////////////////////////////////////////////////////////////////
// 
// GLOBALS
//
/////////////////////////////////////////////////////////////////
vector<BLOCK*> blocks;
queue<int> freeBlocks;
TreeNode *root = NULL;
BLOCK_HEADER bh;
vector<NODE*> idList; // list of nodes with id as index.


//Use debugf and NOT printf() to make your
//debug outputs. Do not modify this function.
#if defined(DEBUG)
int debugf(const char *fmt, ...)
{
    int bytes = 0;
    va_list args;
    va_start(args, fmt);
    bytes = vfprintf(stderr, fmt, args);
    va_end(args);
    return bytes;
}
#else
int debugf(const char *fmt, ...)
{
    return 0;
}
#endif

//////////////////////////////////////////////////////////////////
//
// START HERE W/ fs_drive()
//
//////////////////////////////////////////////////////////////////
//Read the hard drive file specified by dname
//into memory. You may have to use globals to store
//the nodes and / or blocks.
//Return 0 if you read the hard drive successfully (good MAGIC, etc).
//If anything fails, return the proper error code (-EWHATEVER)
//Right now this returns -EIO, so you'll get an Input/Output error
//if you try to run this program without programming fs_drive.
//////////////////////////////////////////////////////////////////
int fs_drive(const char *dname)
{
    debugf("fs_drive: %s\n", dname);

    FILE *fin;
    NODE *n;
    BLOCK *b;
    uint64_t numBlocks;

    // Open dname.
    if((fin = fopen(dname, "r")) == NULL) {
        debugf("unable to open file: %s\n", dname);
        return -EIO;
    }

    // Read the block header into bh.
    if(fread(&bh, sizeof(BLOCK_HEADER), 1, fin) != 1) {
        debugf("unable to read header from file: %s\n", dname);
        return -EIO;
    }

    // Check magic.
    if(strncmp(bh.magic, MAGIC, 8) != 0) {
        debugf("MAGIC does not match: %s\n", bh.magic);
        return -EIO;
    }

    // Read in nodes.
    for(unsigned int i = 0; i < bh.nodes; i++) {
        n = new NODE();
        if(fread(n, ONDISK_NODE_SIZE, 1, fin) != 1) {
            debugf("unable to read node %d from hard_drive\n", i);
            return -EIO;
        }

        // Make room in memory for the list of block pointers.
        numBlocks = getNumBlocks(n);

        n->blocks = (uint64_t*)malloc(sizeof(uint64_t *) * numBlocks);

        // Read in the list of block indices.
        for(unsigned int j = 0; j < numBlocks; j++) {
            uint64_t k;
            if(fread(&k, sizeof(uint64_t), 1, fin) != 1) {
                debugf("unable to read block offset %d from node %d\n", j, i);
                return -EIO;
            }
            n->blocks[j] = k;
        }

        // Save n.
        insertNode(n);
    }


    // Read in blocks.
    for(unsigned int i = 0; i < bh.blocks; i++) {
        b = new BLOCK();
        b->data = (char*)malloc(sizeof(char) * bh.block_size);
        if(fread(b->data, sizeof(char) * bh.block_size, 1, fin) != 1) {
            debugf("unable to read block %d\n", i);
            return -EIO;
        }

        // Save the block.
        blocks.push_back(b);
    }


    // Close the file.
    if(fclose(fin) != 0) {
        debugf("unable to close file: %s\n", dname);
        return -EIO;
    }

    return 0;
}

//////////////////////////////////////////////////////////////////
//Open a file <path>. This really doesn't have to do anything
//except see if the file exists. If the file does exist, return 0,
//otherwise return -ENOENT
//////////////////////////////////////////////////////////////////
int fs_open(const char *path, struct fuse_file_info *fi)
{
    debugf("fs_open: %s\n", path);

    TreeNode *tn = getTreeNode(path);
    if(tn == NULL)
        return -ENOENT;
    else if(S_ISDIR(tn->node->mode))
        return -EISDIR;
    else
        return 0;
}

//////////////////////////////////////////////////////////////////
//Read a file <path>. You will be reading from the block and
//writing into <buf>, this buffer has a size of <size>. You will
//need to start the reading at the offset given by <offset>.
//////////////////////////////////////////////////////////////////
int fs_read(const char *path, char *buf, size_t size, off_t offset,
        struct fuse_file_info *fi)
{
    debugf("fs_read: %s\n", path);

    TreeNode *tn = getTreeNode(path);
    if(tn == NULL)
        return -ENOENT;

    NODE *n = tn->node;

    if(S_ISDIR(n->mode))
        return -EISDIR;
    else if(S_ISLNK(n->mode))
        n = getNodeFromId(n->link_id);



    char *ptr = buf;
    size_t totalSize = 0;
    uint64_t numBlocks = getNumBlocks(n);

    for(uint64_t i = 0; i < numBlocks && totalSize < size; i++) {
        uint64_t offset = n->blocks[i];
        BLOCK *b = blocks[offset];

        size_t numToCopy = min(size - totalSize, bh.block_size);
        
        memcpy(ptr, b->data, numToCopy);
        ptr += numToCopy;
        totalSize += numToCopy;
    }

    return totalSize;
}

//////////////////////////////////////////////////////////////////
//Write a file <path>. If the file doesn't exist, it is first
//created with fs_create. You need to write the data given by
//<data> and size <size> into this file block. You will also need
//to write data starting at <offset> in your file. If there is not
//enough space, return -ENOSPC. Finally, if we're a read only file
//system (fi->flags & O_RDONLY), then return -EROFS
//If all works, return the number of bytes written.
//////////////////////////////////////////////////////////////////
int fs_write(const char *path, const char *data, size_t size, off_t offset,
        struct fuse_file_info *fi)
{
    debugf("fs_write: %s\n", path);
    return -EIO;
}

//////////////////////////////////////////////////////////////////
//Create a file <path>. Create a new file and give it the mode
//given by <mode> OR'd with S_IFREG (regular file). If the name
//given by <path> is too long, return -ENAMETOOLONG. As with
//fs_write, if we're a read only file system
//(fi->flags & O_RDONLY), then return -EROFS.
//Otherwise, return 0 if all succeeds.
//////////////////////////////////////////////////////////////////
int fs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    debugf("fs_create: %s\n", path);

    // Check for existence.
    if(getTreeNode(path) != NULL)
        return -EEXIST;

    if(strlen(path) > NAME_SIZE)
        return -ENAMETOOLONG;

    if(fi->flags & O_RDONLY)
        return -EROFS;

    NODE *n = new NODE;

    strcpy(n->name, path);
    n->id = getAvailId();
    n->uid = getuid();
    n->gid = getgid();
    n->mode = mode | S_IFREG;
    n->size = 0;

    int t = time(NULL);
    n->ctime = t;
    n->atime = t;
    n->mtime = t;


   // if(!isEnoughSpace(1)) // need enough space to add one block.
   //     return -ENOSPC;
    BLOCK *b = new BLOCK;
    b->data = new char[bh.block_size];
    blocks.push_back(b);
    n->blocks = new uint64_t[1];
    n->blocks[0] = blocks.size() - 1;
    
    insertNode(n);

    printFileTree(root, 0);

    return 0;
}

//////////////////////////////////////////////////////////////////
//Get the attributes of file <path>. A static structure is passed
//to <s>, so you just have to fill the individual elements of s:
//s->st_mode = node->mode
//s->st_atime = node->atime
//s->st_uid = node->uid
//s->st_gid = node->gid
// ...
//Most of the names match 1-to-1, except the stat structure
//prefixes all fields with an st_*
//Please see stat for more information on the structure. Not
//all fields will be filled by your filesystem.
//////////////////////////////////////////////////////////////////
int fs_getattr(const char *path, struct stat *s)
{
    debugf("fs_getattr: %s\n", path);
    TreeNode *tn = getTreeNode(path);
    if(tn == NULL)
        return -ENOENT;

    NODE *n = tn->node;

    s->st_ino = n->id;
    s->st_mode = n->mode;
    s->st_nlink = 0;
    s->st_uid = n->uid;
    s->st_gid = n->gid;
    s->st_atime = n->atime;
    s->st_mtime = n->mtime;
    s->st_ctime = n->ctime;
    s->st_blksize = bh.block_size;
    s->st_blocks = getNumBlocks(n);

    if(S_ISREG(n->mode))
        s->st_size = n->size;
    else if(S_ISLNK(n->mode))
        s->st_size = strlen(getNodeFromId(n->link_id)->name);

    return 0;
}

//////////////////////////////////////////////////////////////////
//Read a directory <path>. This uses the function <filler> to
//write what directories and/or files are presented during an ls
//(list files).
//
//filler(buf, "somefile", 0, 0);
//
//You will see somefile when you do an ls
//(assuming it passes fs_getattr)
//////////////////////////////////////////////////////////////////
int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
        off_t offset, struct fuse_file_info *fi)
{
    debugf("fs_readdir: %s\n", path);

    //filler(buf, <name of file/directory>, 0, 0)
    filler(buf, ".", 0, 0);
    filler(buf, "..", 0, 0);

    //You MUST make sure that there is no front slashes in the name (second parameter to filler)
    //Otherwise, this will FAIL.
    
    list<TreeNode*> children = getChildren(path);
    for(list<TreeNode*>::iterator it = children.begin(); it != children.end(); it++)
        filler(buf, (*it)->relName.c_str(), 0, 0);

    return 0;
}

//////////////////////////////////////////////////////////////////
//Open a directory <path>. This is analagous to fs_open in that
//it just checks to see if the directory exists. If it does,
//return 0, otherwise return -ENOENT
//////////////////////////////////////////////////////////////////
int fs_opendir(const char *path, struct fuse_file_info *fi)
{
    debugf("fs_opendir: %s\n", path);
    NODE *n = getTreeNode(path)->node;
    if(S_ISDIR(n->mode))
        return 0;
    else 
        return -ENOENT;
}

//////////////////////////////////////////////////////////////////
//Change the mode (permissions) of <path> to <mode>
//////////////////////////////////////////////////////////////////
int fs_chmod(const char *path, mode_t mode)
{
    return -EIO;
}

//////////////////////////////////////////////////////////////////
//Change the ownership of <path> to user id <uid> and group id <gid>
//////////////////////////////////////////////////////////////////
int fs_chown(const char *path, uid_t uid, gid_t gid)
{
    debugf("fs_chown: %s\n", path);
    return -EIO;
}

//////////////////////////////////////////////////////////////////
//Unlink a file <path>. This function should return -EISDIR if a
//directory is given to <path> (do not unlink directories).
//Furthermore, you will not need to check O_RDONLY as this will
//be handled by the operating system.
//Otherwise, delete the file <path> and return 0.
//////////////////////////////////////////////////////////////////
int fs_unlink(const char *path)
{
    debugf("fs_unlink: %s\n", path);


    TreeNode *tn = getTreeNode(path);
    if(tn == NULL)
        return -ENOENT;
    
    NODE *n = tn->node;

    if(S_ISDIR(n->mode))
        return -EISDIR;

    if(S_ISREG(n->mode)) {
        removeBlocks(n);
    }

    // If either a link or reg, Remove node from file tree.
    removeNode(n);

    return 0;
}

//////////////////////////////////////////////////////////////////
//Make a directory <path> with the given permissions <mode>. If
//the directory already exists, return -EEXIST. If this function
//succeeds, return 0.
//////////////////////////////////////////////////////////////////
int fs_mkdir(const char *path, mode_t mode)
{
    debugf("fs_mkdir: %s\n", path);
    return -EIO;
}

//////////////////////////////////////////////////////////////////
//Remove a directory. You have to check to see if it is
//empty first. If it isn't, return -ENOTEMPTY, otherwise
//remove the directory and return 0.
//////////////////////////////////////////////////////////////////
int fs_rmdir(const char *path)
{
    debugf("fs_rmdir: %s\n", path);
    return -EIO;
}

//////////////////////////////////////////////////////////////////
//Rename the file given by <path> to <new_name>
//Both <path> and <new_name> contain the full path. If
//the new_name's path doesn't exist return -ENOENT. If
//you were able to rename the node, then return 0.
//////////////////////////////////////////////////////////////////
int fs_rename(const char *path, const char *new_name)
{
    debugf("fs_rename: %s -> %s\n", path, new_name);
    return -EIO;
}

//////////////////////////////////////////////////////////////////
//Rename the file given by <path> to <new_name>
//Both <path> and <new_name> contain the full path. If
//the new_name's path doesn't exist return -ENOENT. If
//you were able to rename the node, then return 0.
//////////////////////////////////////////////////////////////////
int fs_truncate(const char *path, off_t size)
{
    debugf("fs_truncate: %s to size %d\n", path, size);
    return -EIO;
}

//////////////////////////////////////////////////////////////////
//fs_destroy is called when the mountpoint is unmounted
//this should save the hard drive back into <filename>
//////////////////////////////////////////////////////////////////
void fs_destroy(void *ptr)
{
    const char *filename = (const char *)ptr;
    debugf("fs_destroy: %s\n", filename);

    //Save the internal data to the hard drive
    //specified by <filename>
}

//////////////////////////////////////////////////////////////////
//int main()
//DO NOT MODIFY THIS FUNCTION
//////////////////////////////////////////////////////////////////
int main(int argc, char *argv[])
{
    fuse_operations *fops;
    char *evars[] = { "./fs", "-f", "mnt", NULL };
    int ret;

    if ((ret = fs_drive(HARD_DRIVE)) != 0) {
        debugf("Error reading hard drive: %s\n", strerror(-ret));
        return ret;
    }
    //FUSE operations
    fops = (struct fuse_operations *) calloc(1, sizeof(struct fuse_operations));
    fops->getattr = fs_getattr;
    fops->readdir = fs_readdir;
    fops->opendir = fs_opendir;
    fops->open = fs_open;
    fops->read = fs_read;
    fops->write = fs_write;
    fops->create = fs_create;
    fops->chmod = fs_chmod;
    fops->chown = fs_chown;
    fops->unlink = fs_unlink;
    fops->mkdir = fs_mkdir;
    fops->rmdir = fs_rmdir;
    fops->rename = fs_rename;
    fops->truncate = fs_truncate;
    fops->destroy = fs_destroy;

    debugf("Press CONTROL-C to quit\n\n");

    return fuse_main(sizeof(evars) / sizeof(evars[0]) - 1, evars, fops,
            (void *)HARD_DRIVE);
}


////////////////////////////////////////////////
// Helper Functions
///////////////////////////////////////////////

// Splits the abs_path into a list of the strings delimited by /.
list <string> Split(string abs_path){
    list <string> retList = *(new list<string>);
    string str = "";

    for(unsigned int i = 0; i < abs_path.size(); i++){
        if(abs_path[i] == '/'){
            retList.push_back(str);
            str = "";
        }
        else
            str += abs_path[i];    
    }
    if(str != "") retList.push_back(str);

    return retList;
}

// Traverses the directory tree and returns a pointer to the node specified by path.
// If the node doesn't exist, null is returned.
// path is a list returned by Split.
TreeNode *getTreeNode(list<string> path) {
    TreeNode *cur = root;

    // Traverse the directory tree looking for matching file names at each level.
    // If one on of the levels isn't there, we can't get the requested node.
    list<string>::iterator name = path.begin();
    name++;
    for(; name != path.end(); name++) {
        bool found = false;

        for(list<TreeNode*>::iterator child = cur->children.begin(); child != cur->children.end(); child++) {
            // If there is a matching subdirectory.
            if((*child)->relName == *name) {
                cur = *child;
                found = true;
                break;
            }
        }
        // If no child found for this child, it's an error.
        if(!found)
            return NULL;
    }

    return cur;
}

TreeNode *getTreeNode(string path) {
    list<string> parsedNames = Split(path);
    return getTreeNode(parsedNames);
}

// Insert NODE n into the correct place in the file tree based on the full path in n.name.
void insertNode(NODE* n) {
    if(root == NULL) {
        root = new TreeNode(n, NULL);
        addId(n);
        return;
    }

    list<string> parsedNames = Split(n->name);
    parsedNames.pop_back();
    TreeNode *parent = getTreeNode(parsedNames); // The parent dir of NODE n.

    // If getTreeNode returns null, the path doesn't exist.
    if(parent == NULL)
        return;

    parent->children.push_back(new TreeNode(n, parent));
    addId(n);
}

// Deletes the block stored at block[offset], and adds offset to the free list.
void freeBlock(uint64_t offset) {
    BLOCK *b = blocks[offset];
    if(b == NULL)
        return;

    delete b;
    freeBlocks.push(offset);
}

// First searches the free list for a place to put the block. Else, appends it to the end of blocks.
void addBlock(BLOCK *b) {
    if(!freeBlocks.empty()) {
        int i = freeBlocks.back();
        freeBlocks.pop();
        blocks[i] = b;
    }
    else {
        blocks.push_back(b);
    }
}

// Remove all blocks associated with n.
void removeBlocks(NODE *n) {
    for(uint64_t i = 0; i < getNumBlocks(n); i++) {
        freeBlock(n->blocks[i]);
    }
}

// Removes node from File tree and from idList.
void removeNode(NODE *n) {
    TreeNode *tn = getTreeNode(n->name);
    TreeNode *parent = tn->parent;

    parent->children.remove(tn);
    idList[n->id] = NULL;
    delete tn;
    delete n;
}

void addId(NODE *n) {
    if(idList.size() < n->id + 1)
        idList.resize(n->id + 1, NULL);

    idList[n->id] = n;
}

uint64_t getAvailId() {
    int size = 20; // How much to resize by when it's needed.

    for(uint64_t i = 0; i < idList.size(); i++) {
        if(idList[i] == NULL)
            return i;
    }

    uint64_t ret = idList.size();
    idList.resize(size, NULL);

    return ret;
}

// Returns the NODEs located at path.
list<TreeNode*> getChildren(const char* path) {
    return getTreeNode(path)->children;
}

// For debugging. Prints file tree structure.
void printFileTree(TreeNode *root, int level) {
    for(int i = 0; i < level; i++)
        debugf(" ");
    debugf("%s", root->relName.c_str());
    if(S_ISDIR(root->node->mode))
        debugf("/");
    debugf("\n");

    for(list<TreeNode*>::iterator child = root->children.begin(); child != root->children.end(); child++) {
        printFileTree(*child, level+1);
    }
}

// Recursively search for the node n with n.id == id.
// start defaults to root.
// Returns Null if no id is found.
NODE *getNodeFromId(uint64_t id) {
    return idList[id];
}

uint64_t getNumBlocks(const NODE *n) {
    if(S_ISDIR(n->mode) || S_ISLNK(n->mode))
        return 0;
    else
        return n->size / bh.block_size + 1;
}
