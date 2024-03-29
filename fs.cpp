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
#include <fstream>

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

list <string> Split(const string &abs_path);

typedef struct TreeNode {
        list<TreeNode*> children;
        NODE *node;
        TreeNode *parent;
        string relName;
        TreeNode(NODE *n, TreeNode *parent) : node(n), parent(parent) {
            relName = Split(n->name).back();
        }
} TreeNode;

TreeNode *getTreeNode(const string &path);
TreeNode *getTreeNode(list<string> path);
void printFileTree(TreeNode *root, int level = 0);
int insertNode(NODE* n);
list<TreeNode*> getChildren(const char* path);
NODE *getNodeFromId(uint64_t id);
uint64_t getNumBlocks(const NODE *n);
uint64_t getNumBlocks(uint64_t bytes);
void addId(NODE *n);
uint64_t getAvailId();
void removeBlocks(NODE *n);
uint64_t addBlock(BLOCK *b);
void freeBlock(uint64_t offset);
void removeNode(NODE *n);
void deleteNode(NODE *n);
NODE *defaultNode(const char *path);
bool pathIsValid(const char *path);
void changeName(TreeNode *tn, const char *path);
void recursivelyChangeName(TreeNode *tn, const string &path);
uint64_t getNumNodes();
void addBlockToNode(NODE *node, uint64_t new_blocks,  uint64_t *index);
int IsEnoughSpace(uint64_t new_blocks);
void truncateBlocks(NODE *n, uint64_t num_blocks);

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
	char *buf, *buf_ptr;
    uint64_t i;
    uint64_t index, offset_block_index, blocks_index;
	uint64_t cur_block_count, new_block_count, total_block_count;
    string strpath;
    TreeNode *tn;
    NODE *node;
    BLOCK *b;
    size_t bytes_written, size_left;

    //get the tree node and node
    strpath = string(path);
    tn = getTreeNode(strpath);
	if(tn == NULL) return -ENOENT;
    node = tn->node;

	//get block numbers
	total_block_count = ((uint64_t) size + (uint64_t) offset) / (uint64_t) bh.block_size + 1;
	cur_block_count = getNumBlocks(node);
	new_block_count = (uint64_t) max((long long) 0, ((long long) total_block_count - (long long) cur_block_count));
	
	//check if there is enough space
	if(!(IsEnoughSpace(new_block_count))) return -ENOSPC; 
	//check to see if it is read only
	if(fi->flags & O_RDONLY) return -EROFS;

	//truncate extra blocks
	if(total_block_count < cur_block_count){
		truncateBlocks(node, total_block_count);
	}

	//set up the buffer
	buf = (char *) malloc((size / bh.block_size + 1) * bh.block_size);
	buf_ptr = buf;

	//copy the first block up until offset into the buffer
	//get offset block
	index = offset / bh.block_size;
	blocks_index = node->blocks[index];
	b = blocks[blocks_index];
	
	//calculate how many bytes to copy
	size_t bytes_to_cp = offset - index * bh.block_size;
	memcpy(buf_ptr, b->data, bytes_to_cp);
	buf_ptr += bytes_to_cp;

	//copy the data into the buffer
	memcpy(buf_ptr, data, size);

	//add blocks
	uint64_t *indices = (uint64_t *) malloc(sizeof(uint64_t) * new_block_count);
	for(i = 0; i < new_block_count; i++){
		b = new BLOCK;
		b->data = (char *) malloc(bh.block_size);
		indices[i] = addBlock(b);
	}
	addBlockToNode(node, new_block_count, indices);
	free(indices);

	//write blocks
	size_left = size + bytes_to_cp;
	buf_ptr = buf;
	for(i = index; i < total_block_count; i++){
		size_t write_size = min(size_left, bh.block_size);
		blocks_index = node->blocks[i];
		b = blocks[blocks_index];
		memcpy(b->data, buf_ptr, write_size);
		buf_ptr += write_size;
		size_left -= write_size;
	}
	
	free(buf);
	//add node size
	node->size = size + offset;	
	return size;
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

    NODE *n = defaultNode(path);
    n->mode = mode | S_IFREG;

    if(!IsEnoughSpace(1)) // need enough space to add one block.
        return -ENOSPC;
    BLOCK *b = new BLOCK;
    b->data = new char[bh.block_size];
    blocks.push_back(b);
    n->blocks = new uint64_t[1];
    n->blocks[0] = blocks.size() - 1;
    
    if(insertNode(n) != 0)
        return -ENOENT;

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
    TreeNode *tn = getTreeNode(path);
    if(tn == NULL)
        return -ENOENT;

    NODE *n = tn->node;
    n->mode = mode;

    return 0;
}

//////////////////////////////////////////////////////////////////
//Change the ownership of <path> to user id <uid> and group id <gid>
//////////////////////////////////////////////////////////////////
int fs_chown(const char *path, uid_t uid, gid_t gid)
{
    debugf("fs_chown: %s\n", path);
    
    TreeNode *tn = getTreeNode(path);
    if(tn == NULL)
        return -ENOENT;
    
    NODE *n = tn->node;
    n->uid = uid;
    n->gid = gid;

    return 0;
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
       // removeBlocks(n);
	   free(n->blocks);
    }

    // If either a link or reg, Remove node from file tree.
    removeNode(n);
    deleteNode(n);

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

    // Check for existence.
    if(getTreeNode(path) != NULL)
        return -EEXIST;

    if(strlen(path) > NAME_SIZE)
        return -ENAMETOOLONG;

    NODE *n = defaultNode(path);
    n->mode = mode | S_IFDIR;

    if(insertNode(n) != 0)
        return -ENOENT;

    printFileTree(root, 0);

    return 0;
}

//////////////////////////////////////////////////////////////////
//Remove a directory. You have to check to see if it is
//empty first. If it isn't, return -ENOTEMPTY, otherwise
//remove the directory and return 0.
//////////////////////////////////////////////////////////////////
int fs_rmdir(const char *path)
{
    debugf("fs_rmdir: %s\n", path);

    TreeNode *tn = getTreeNode(path);
    if(tn == NULL)
        return -ENOENT;
    if(!tn->children.empty())
        return -ENOTEMPTY;
    
    NODE *n = tn->node;
    if(!S_ISDIR(n->mode))
        return -ENOTDIR;

    removeNode(n);
    deleteNode(n);

    return 0;
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

    // Get current Treenode.
    TreeNode *tn = getTreeNode(path);
    if(tn == NULL)
        return -ENOENT;
    if(strlen(new_name) > NAME_SIZE)
        return -ENAMETOOLONG;
   
    // Remove tn from its parent's list.
    TreeNode *parent = tn->parent;
    parent->children.remove(tn);

    // Add tn to its new parent's list.
    list<string> parsedNewName = Split(new_name);
    parsedNewName.pop_back();
    TreeNode *newParent = getTreeNode(parsedNewName);
    if(newParent == NULL) // If getTreeNode returns null, the path doesn't exist.
        return -ENOENT;

    // If newName exists, delete it, so that it can be replaced
    TreeNode *newTn = getTreeNode(new_name);
    if(newTn != NULL) {
        NODE *newNode = newTn->node;
        removeNode(newNode);
        deleteNode(newNode);
    }

    newParent->children.push_back(tn);

    // Change tn's parent.
    tn->parent = newParent;

    // Change the name of tn's node and all of its children's nodes.
    changeName(tn, new_name);

    printFileTree(root, 0);

    return 0;
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

    
    TreeNode *tn = getTreeNode(path);
    if(tn == NULL) return -ENOENT;
    NODE *n = tn->node;
    if((uint64_t)size > n->size)
        return -EINVAL;

    uint64_t oldBlocks = getNumBlocks(n);
    uint64_t newBlocks = getNumBlocks(size);

    for(uint64_t i = newBlocks; i < oldBlocks; i++) {
        uint64_t index = n->blocks[i];
        freeBlock(index);
    }

    n->blocks = (uint64_t*)realloc(n->blocks, newBlocks);
    n->size = size;
    

    return 0;
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
    
	ofstream fout(filename, ios::binary);
	if (!fout) {
		return;
	}

    bh.blocks = blocks.size();
    bh.nodes = getNumNodes();
	fout.write((char*)&bh, sizeof(bh));

    for(vector<NODE*>::iterator it = idList.begin(); it != idList.end(); it++) {
        NODE *n = *it;
        if(n != NULL) {
            fout.write((char*)n, ONDISK_NODE_SIZE);
            fout.write((char*)n->blocks, sizeof(uint64_t) * getNumBlocks(n));
        }
    }

    for(vector<BLOCK*>::iterator it = blocks.begin(); it != blocks.end(); it++) {
        fout.write(((*it)->data), bh.block_size);
    }

    //TODO: Delete everything.
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
list <string> Split(const string &abs_path){
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

TreeNode *getTreeNode(const string &path) {
    list<string> parsedNames = Split(path);
    return getTreeNode(parsedNames);
}

// Insert NODE n into the correct place in the file tree based on the full path in n.name.
// Returns 0 on success and 1 on failure.
int insertNode(NODE* n) {
    if(root == NULL) {
        root = new TreeNode(n, NULL);
        addId(n);
        return 0;
    }

    list<string> parsedNames = Split(n->name);
    parsedNames.pop_back();
    TreeNode *parent = getTreeNode(parsedNames); // The parent dir of NODE n.

    // If getTreeNode returns null, the path doesn't exist.
    if(parent == NULL)
        return 1;

    parent->children.push_back(new TreeNode(n, parent));
    addId(n);

    return 0;
}

// Returns true if the path leading up to the last '/' exists.
bool pathIsValid(const char *path) {
    list<string> parsedNames = Split(path);
    parsedNames.pop_back();
    TreeNode *parent = getTreeNode(parsedNames); // The parent dir of NODE n.

    // If getTreeNode returns null, the path doesn't exist.
    if(parent == NULL)
        return false;
    return true;
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
uint64_t addBlock(BLOCK *b) {
    if(!freeBlocks.empty()) {
        int i = freeBlocks.back();
        freeBlocks.pop();
        blocks[i] = b;
		return (uint64_t) i;
    }
    else {
        blocks.push_back(b);
		return ((uint64_t) blocks.size() - 1);
    }
}

// Remove all blocks associated with n.
void removeBlocks(NODE *n) {
    for(uint64_t i = 0; i < getNumBlocks(n); i++) {
        freeBlock(n->blocks[i]);
    }
}

void truncateBlocks(NODE *n, uint64_t num_blocks){
	uint64_t *blocks_table;
	uint64_t cur_blocks, blocks_index;

	//free the blocks from the blocks vector
	cur_blocks = getNumBlocks(n);
	for(uint64_t i = num_blocks; i < cur_blocks; i++){
		blocks_index = n->blocks[i];
		freeBlock(blocks_index);
	}

	//create the new blocks indices table within the node 
	blocks_table = (uint64_t *) malloc(sizeof(uint64_t) * num_blocks);
	for(uint64_t i = 0; i < num_blocks; i++){
		blocks_table[i] = n->blocks[i];
	}
	free(n->blocks);
	n->blocks = blocks_table;
}

// Removes node from File tree only.
// Deletes treenode.
// Does not delete n.
void removeNode(NODE *n) {
    debugf("%s\n", n->name);
    TreeNode *tn = getTreeNode(n->name);
    TreeNode *parent = tn->parent;

    parent->children.remove(tn);
    delete tn;
}

// deletes n and removes its id from the idList.
void deleteNode(NODE *n) {
    idList[n->id] = NULL;
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
        return getNumBlocks(n->size);
}

uint64_t getNumBlocks(uint64_t bytes) {
    return bytes / bh.block_size + 1;
}

int IsEnoughSpace(uint64_t new_blocks){
    if(MAX_DRIVE_SIZE - bh.blocks * bh.block_size - new_blocks * bh.block_size > 0) return 1;
    return 0;
}

// Constructor for NODE
NODE *defaultNode(const char *path) {
    NODE *n = new NODE;
    strcpy(n->name, path);
    n->uid = getuid();
    n->gid = getgid();
    n->size = 0;

    int t = time(NULL);
    n->ctime = t;
    n->atime = t;
    n->mtime = t;
    n->id = getAvailId();

    return n;
}

// Change both the tn.relName and tn.node.name for tn and the tn.node.name of all of its children.
// tn->parent must already be set to the new parent.
// path is the full path up to and including tn.
void changeName(TreeNode *tn, const char *path) {
    string parentName = tn->parent->relName;
    list<string> parsedNames = Split(path);

   tn->relName = parsedNames.back();

    // Remove the last element of parsedNames (The relName of tn).
    while(parsedNames.back() != parentName)
        parsedNames.pop_back();

    // Reconstruct path without tn's relname.
    string s;
    for (list<string>::const_iterator it = parsedNames.begin(); it != parsedNames.end(); ++it)
        s += *it + '/';
    s.pop_back(); // Git rid of last '/'.
    debugf("Name: %s\n", s.c_str());

    recursivelyChangeName(tn, s);
}

// tn is the TreeNode whose name to change. Path is the full path up to and including tn's parent.
void recursivelyChangeName(TreeNode *tn, const string &path) {
    string newName = path + '/' + tn->relName;
    strcpy(tn->node->name, newName.c_str());
    for(list<TreeNode*>::iterator it = tn->children.begin(); it != tn->children.end(); it++) {
        recursivelyChangeName(*it, newName);
    }
}

// Returns the number of nodes in the idList.
uint64_t getNumNodes() {
    uint64_t num = 0;
    for(vector<NODE*>::iterator it = idList.begin(); it != idList.end(); it++) {
        num += *it != NULL ? 1 : 0;
    }
    return num;
}

void addBlockToNode(NODE *node, uint64_t new_blocks,  uint64_t *index){
    uint64_t cur_blk_count, new_blk_count;
    uint64_t *new_blocks_array;

    //set the current block count and the new block count
    cur_blk_count = getNumBlocks(node);
	new_blk_count = cur_blk_count + new_blocks;

    //allocate a new, bigger array
    new_blocks_array = (uint64_t *) malloc(sizeof(uint64_t) * new_blk_count);
    
    //copy the contents from the old array to the new array
    for(uint64_t i = 0; i < cur_blk_count; i++){
        new_blocks_array[i] = node->blocks[i];
    }

    //add the new block index
	for(uint64_t i = cur_blk_count; i < new_blk_count; i++){
		new_blocks_array[i] = index[i - cur_blk_count];
	}
    
    //free the old array
    free(node->blocks);

    //set the new array
    node->blocks = new_blocks_array;  
}
