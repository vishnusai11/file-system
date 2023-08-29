#include "jumbo_file_system.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
// C does not have a bool type, so I created one that you can use
typedef char bool_t;
#define TRUE 1
#define FALSE 0


static block_num_t current_dir;


// optional helper function you can implement to tell you if a block is a dir node or an inode
static bool_t is_dir(block_num_t block_num) {
  //to check if a block is a directory or an inode... have to access the is_dir variable block struct
  //allocating some memory to a buffer variable into which the data of the block is read into
  void *buffer = malloc(BLOCK_SIZE);
  //we can read the given block using the read_block() function
  read_block(block_num, buffer);
  //typecasting the buffer we have to be the struct block type
  struct block *block = (struct block *) buffer; 
  //now go and take the is_dir variable and see if it is 0 or 1
  if((*block).is_dir == 0){ //if it is 0, the block is a directory
    free(buffer);
    return TRUE;
  }else{ //else it is an inode
    free(buffer);
    return FALSE;
  }
}


/* jfs_mount
 *   prepares the DISK file on the _real_ file system to have file system
 *   blocks read and written to it.  The application _must_ call this function
 *   exactly once before calling any other jfs_* functions.  If your code
 *   requires any additional one-time initialization before any other jfs_*
 *   functions are called, you can add it here.
 * filename - the name of the DISK file on the _real_ file system
 * returns 0 on success or -1 on error; errors should only occur due to
 *   errors in the underlying disk syscalls.
 */
int jfs_mount(const char* filename) {
  int ret = bfs_mount(filename);
  current_dir = 1;
  return ret;
}


/* jfs_mkdir
 *   creates a new subdirectory in the current directory
 * directory_name - name of the new subdirectory
 * returns 0 on success or one of the following error codes on failure:
 *   E_EXISTS, E_MAX_NAME_LENGTH, E_MAX_DIR_ENTRIES, E_DISK_FULL
 */
int jfs_mkdir(const char* directory_name) {
  //read the current_directory 
  void *buffer1 = malloc(BLOCK_SIZE);
  //we can read the given block using the read_block() function
  read_block(current_dir, buffer1);
  //typecasting the buffer we have to be the struct block type
  struct block *block1 = (struct block *) buffer1; 
  uint16_t *num_entries1 = &((*block1).contents.dirnode.num_entries);
  //first check if the directory is full
  if(*num_entries1 == MAX_DIR_ENTRIES){
    free(buffer1);
    return E_MAX_DIR_ENTRIES;
  }else{
    //check if the length of the name is greater than is allowed
    if(strlen(directory_name) > MAX_NAME_LENGTH){
      free(buffer1);
      return E_MAX_NAME_LENGTH;
    }else{
      //check if the name exists in the current_directory
      for(int i = 0; i < *num_entries1; i++){
        //compare the given directory_name with the list of the names in the block
        if(strcmp(directory_name, (*block1).contents.dirnode.entries[i].name) == 0){
          free(buffer1);
          return E_EXISTS;
        }
      }
      //now to check if the disk will be full after we add a new subdirectory
      //we try to find an unallocated block using the allocate_block()
      block_num_t new_block = allocate_block();
      //check if the disk is full
      if(new_block == 0){ 
        free(buffer1);
        return E_DISK_FULL;
      }else{
        //now after checking all possible error codes
        //increment the number of entries in the current directory as we are adding a new subdirectory
        *num_entries1 += 1;
        (*block1).contents.dirnode.entries[(*num_entries1) - 1].block_num = new_block;
        strncpy((*block1).contents.dirnode.entries[(*num_entries1) - 1].name, directory_name, strlen(directory_name) + 1);
        //now all these changes are in the buffer 
        //we can make use of the write_block() to write the data from the buffer to the block here current_directory
        write_block(current_dir, buffer1);
        free(buffer1);
        //now let's make this newly added block a directory
        void *buffer2 = malloc(BLOCK_SIZE);
        //we can read the given block using the read_block() function
        read_block(new_block, buffer2);
        //typecasting the buffer we have to be the struct block type
        struct block *block2 = (struct block *) buffer2; 
        (*block2).is_dir = 0; //we are setting the is_dir to 0 as this is a subdirectory
        uint16_t *num_entries2 = &((*block2).contents.dirnode.num_entries);
        //and since this is a new subdirectory, it won't be having any entries yet so making the number of entries as 0
        *num_entries2 = 0;
        write_block(new_block, buffer2);
        free(buffer2);
        return E_SUCCESS;
      }
    }
  }
}


/* jfs_chdir
 *   changes the current directory to the specified subdirectory, or changes
 *   the current directory to the root directory if the directory_name is NULL
 * directory_name - name of the subdirectory to make the current
 *   directory; if directory_name is NULL then the current directory
 *   should be made the root directory instead
 * returns 0 on success or one of the following error codes on failure:
 *   E_NOT_EXISTS, E_NOT_DIR
 */
int jfs_chdir(const char* directory_name) {
  //first read the argument... if it is NULL then make the root as the current_directory
  if(directory_name == NULL){
    current_dir = 1; //1 is the root directory
    return E_SUCCESS;
  }else{
    //as before read the current_directory
    void *buffer1 = malloc(BLOCK_SIZE);
    //we can read the given block using the read_block() function
    read_block(current_dir, buffer1);
    //typecasting the buffer we have to be the struct block type
    struct block *block1 = (struct block *) buffer1; 
    uint16_t *num_entries1 = &((*block1).contents.dirnode.num_entries);
    //iterate through each of the entries and see if the name exists
    for(int i = 0; i < *num_entries1; i++){
      //compare the given directory_name with the list of the names in the block
      if(strcmp(directory_name, (*block1).contents.dirnode.entries[i].name) == 0){
        //now if the name exists, check if it is a directory
        //and for this we use the is_dir() helper function
        if(is_dir((*block1).contents.dirnode.entries[i].block_num)==0){
          free(buffer1);
          return E_NOT_DIR;
        }else{
          //set the given directory as the subdirectory
          current_dir = (*block1).contents.dirnode.entries[i].block_num;
          free(buffer1);
          return E_SUCCESS;
        }
      }
    }
    //if the name does not exist then we have to return the error code cause without the subdirectory to what can we set the current_directory
    free(buffer1);
    return E_NOT_EXISTS;
  }
}


/* jfs_ls
 *   finds the names of all the files and directories in the current directory
 *   and writes the directory names to the directories argument and the file
 *   names to the files argument
 * directories - array of strings; the function will set the strings in the
 *   array, followed by a NULL pointer after the last valid string; the strings
 *   should be malloced and the caller will free them
 * file - array of strings; the function will set the strings in the
 *   array, followed by a NULL pointer after the last valid string; the strings
 *   should be malloced and the caller will free them
 * returns 0 on success or one of the following error codes on failure:
 *   (this function should always succeed)
 */
int jfs_ls(char* directories[MAX_DIR_ENTRIES+1], char* files[MAX_DIR_ENTRIES+1]) {
  //read the current_directory
  //take all the names inside it and check if it is a file or a directory and accordingly add to one of the arguments
  void *buffer1 = malloc(BLOCK_SIZE);
  //we can read the given block using the read_block() function
  read_block(current_dir, buffer1);
  //typecasting the buffer we have to be the struct block type
  struct block *block1 = (struct block *) buffer1; 
  uint16_t *num_entries1 = &((*block1).contents.dirnode.num_entries);
  //iterate through each of the entries and see if the name exists
  int count1 = 0; 
  int count2 = 0;
  for(int i = 0; i < *num_entries1; i++){
    //check if the name of the entry is a directory or not
    if(is_dir((*block1).contents.dirnode.entries[i].block_num)){     
      directories[count1] = (char *)malloc(strlen((*block1).contents.dirnode.entries[i].name) + 1);
      strncpy(directories[count1], (*block1).contents.dirnode.entries[i].name, strlen((*block1).contents.dirnode.entries[i].name) + 1);
      count1 += 1;
    }else{
      files[count2] = (char *)malloc(strlen((*block1).contents.dirnode.entries[i].name) + 1);
      strncpy(files[count2], (*block1).contents.dirnode.entries[i].name, strlen((*block1).contents.dirnode.entries[i].name) + 1);
      count2 += 1;
    }
  }
  //after the last valid string in both arrays
  //rest of the pointers have to be set to NULL
  while(count1 < MAX_DIR_ENTRIES + 1){
    directories[count1] = NULL;
    count1 += 1;
  }
  while(count2 < MAX_DIR_ENTRIES + 1){
    files[count2] = NULL;
    count2 += 1;
  }
  free(buffer1);
  return E_SUCCESS;
}

/* jfs_rmdir
 *   removes the specified subdirectory of the current directory
 * directory_name - name of the subdirectory to remove
 * returns 0 on success or one of the following error codes on failure:
 *   E_NOT_EXISTS, E_NOT_DIR, E_NOT_EMPTY
 */
int jfs_rmdir(const char* directory_name) {
  //this is very similar to mkdir
  //read the current directory
  void *buffer1 = malloc(BLOCK_SIZE);
  //we can read the given block using the read_block() function
  read_block(current_dir, buffer1);
  //typecasting the buffer we have to be the struct block type
  struct block *block1 = (struct block *) buffer1; 
  uint16_t *num_entries1 = &((*block1).contents.dirnode.num_entries);
  //iterate through each of the entries and see if the name exists
  for(int i = 0; i < *num_entries1; i++){
    //compare the given directory_name with the list of the names in the block
    if(strcmp(directory_name, (*block1).contents.dirnode.entries[i].name) == 0){
      //now if the name exists, check if it is a directory
      //and for this we use the is_dir() helper function
      if(is_dir((*block1).contents.dirnode.entries[i].block_num)==0){
        free(buffer1);
        return E_NOT_DIR;
      }else{
        //check if the directory that we are gonna remove is empty
        //and for that as before we need to access the num_entries variable
        //and as before we need to read this subdirectory block
        block_num_t subdirectory = (*block1).contents.dirnode.entries[i].block_num;    
        void *buffer2 = malloc(BLOCK_SIZE);
        //we can read the given block using the read_block() function
        read_block(subdirectory, buffer2);
        //typecasting the buffer we have to be the struct block type
        struct block *block2 = (struct block *) buffer2; 
        uint16_t *num_entries2 = &((*block2).contents.dirnode.num_entries);
        //now if this num_entries is 0, then it is empty else no
        if(*num_entries2 != 0){
          free(buffer1);
          free(buffer2);
          return E_NOT_EMPTY;
        }else{
          //now after all error codes are checked 
          if(i != *num_entries1 - 1){
            (*block1).contents.dirnode.entries[i].block_num = (*block1).contents.dirnode.entries[*num_entries1 - 1].block_num;
            int len = strlen((*block1).contents.dirnode.entries[*num_entries1 - 1].name);
            strncpy((*block1).contents.dirnode.entries[i].name, (*block1).contents.dirnode.entries[*num_entries1 - 1].name, len + 1);
          }
          //now decrement the number of entries by 1 as we are removing a subdirectory
          *num_entries1 -= 1;
          write_block(current_dir, buffer1);
          //we can use the release_block() 
          release_block(subdirectory);
          free(buffer1);
          free(buffer2);
          return E_SUCCESS;
        }
        }
      }
    }
    //this is when the name does not match with any of the names inside the current_directory. which means the directory does not exist
    free(buffer1);
    return E_NOT_EXISTS;
}


/* jfs_creat
 *   creates a new, empty file with the specified name
 * file_name - name to give the new file
 * returns 0 on success or one of the following error codes on failure:
 *   E_EXISTS, E_MAX_NAME_LENGTH, E_MAX_DIR_ENTRIES, E_DISK_FULL
 */
int jfs_creat(const char* file_name) {
  //read the current directory 
  //similar to mkdir
  //read the current_directory 
  void *buffer1 = malloc(BLOCK_SIZE);
  //we can read the given block using the read_block() function
  read_block(current_dir, buffer1);
  //typecasting the buffer we have to be the struct block type
  struct block *block1 = (struct block *) buffer1; 
  uint16_t *num_entries1 = &((*block1).contents.dirnode.num_entries);
  //first check if the directory is full
  if(*num_entries1 == MAX_DIR_ENTRIES){
    free(buffer1);
    return E_MAX_DIR_ENTRIES;
  }else{
    //check if the length of the name is greater than is allowed
    if(strlen(file_name) > MAX_NAME_LENGTH){
      free(buffer1);
      return E_MAX_NAME_LENGTH;
    }else{
      //check if the name exists in the current_directory
      for(int i = 0; i < *num_entries1; i++){
        //compare the given directory_name with the list of the names in the block
        if(strcmp(file_name, (*block1).contents.dirnode.entries[i].name) == 0){
          free(buffer1);
          return E_EXISTS;
        }
      }
      //now to check if the disk will be full after we add a new subdirectory
      //we try to find an unallocated block using the allocate_block()
      block_num_t new_block = allocate_block();
      //check if the disk is full
      if(new_block == 0){ 
        free(buffer1);
        return E_DISK_FULL;
      }else{
        //now after checking all possible error codes
        //increment the number of entries in the current directory as we are adding a new file
        *num_entries1 += 1;
        (*block1).contents.dirnode.entries[(*num_entries1) - 1].block_num = new_block;
        strncpy((*block1).contents.dirnode.entries[(*num_entries1) - 1].name, file_name, strlen(file_name) + 1);
        //now all these changes are in the buffer 
        //we can make use of the write_block() to write the data from the buffer to the block here current_directory
        write_block(current_dir, buffer1);
        free(buffer1);
        //now let's make this newly added block a directory
        void *buffer2 = malloc(BLOCK_SIZE);
        //we can read the given block using the read_block() function
        read_block(new_block, buffer2);
        //typecasting the buffer we have to be the struct block type
        struct block *block2 = (struct block *) buffer2; 
        (*block2).is_dir = 1; //we are setting the is_dir to 1 as this is a file
        (*block2).contents.inode.file_size = 0;
        write_block(new_block, buffer2);
        free(buffer2);
        return E_SUCCESS;
      }
    }
  }
}


/* jfs_remove
 *   deletes the specified file and all its data (note that this cannot delete
 *   directories; use rmdir instead to remove directories)
 * file_name - name of the file to remove
 * returns 0 on success or one of the following error codes on failure:
 *   E_NOT_EXISTS, E_IS_DIR
 */
int jfs_remove(const char* file_name) {
  //read the current directory
  //first check if the name exists and also if it is a file
  void *buffer1 = malloc(BLOCK_SIZE);
  //we can read the given block using the read_block() function
  read_block(current_dir, buffer1);
  //typecasting the buffer we have to be the struct block type
  struct block *block1 = (struct block *) buffer1; 
  uint16_t *num_entries1 = &((*block1).contents.dirnode.num_entries);
  //check if the name exists in the current_directory
  for(int i = 0; i < *num_entries1; i++){
    //compare the given file with the list of the names in the block
    if(strcmp(file_name, (*block1).contents.dirnode.entries[i].name) == 0){
      //now if the name exists, check if it is a directory
      //and for this we use the is_dir() helper function
      if(is_dir((*block1).contents.dirnode.entries[i].block_num)){
        free(buffer1);
        return E_IS_DIR;
      }else{
        block_num_t file = (*block1).contents.dirnode.entries[i].block_num;        
        void *buffer2 = malloc(BLOCK_SIZE);
        read_block(file, buffer2);
        struct block *block2 = (struct block *) buffer2;
        //swap and make the file that we want to remove as the last file
        (*block1).contents.dirnode.entries[i].block_num = (*block1).contents.dirnode.entries[*num_entries1 - 1].block_num;
        if(i != *num_entries1 - 1){
          int len = strlen((*block1).contents.dirnode.entries[*num_entries1 - 1].name);
          strncpy((*block1).contents.dirnode.entries[i].name, (*block1).contents.dirnode.entries[*num_entries1 - 1].name, len + 1);
        }
        //decrement the number of entries
        *num_entries1 -= 1;
        write_block(current_dir, buffer1);
        //unlike the rmdir, we can't just release the blocks
        //we have to see the data blocks too
        uint32_t file_size = block2->contents.inode.file_size;
        uint32_t data_blocks = file_size / BLOCK_SIZE;
        uint32_t check = file_size % BLOCK_SIZE;
        //if there are any partially filled blocks add them up
        if(check != 0){
          data_blocks += 1;
        }
        //since there maybe multiple datablocks for a file, iterate through them and release one by one
        for(uint32_t i1 = 0; i1 < data_blocks; i1++){
          release_block(block2->contents.inode.data_blocks[i1]);
        }
        //after all this is done we release
        release_block(file);
        free(buffer1);
        free(buffer2);
        return E_SUCCESS;
      }
    }
  }
  //this is when the name does not match with any of the names inside the current_directory. which means the file does not exist
  free(buffer1);
  return E_NOT_EXISTS;
}

/* jfs_stat
 *   returns the file or directory stats (see struct stat for details)
 * name - name of the file or directory to inspect
 * buf  - pointer to a struct stat (already allocated by the caller) where the
 *   stats will be written
 * returns 0 on success or one of the following error codes on failure:
 *   E_NOT_EXISTS
 */
int jfs_stat(const char* name, struct stats* buf) {
  //all the stats or information required is in the block struct, we just have to read the required informtion and add it to the stats struct
  void *buffer1 = malloc(BLOCK_SIZE);
  //we can read the given block using the read_block() function
  read_block(current_dir, buffer1);
  //typecasting the buffer we have to be the struct block type
  struct block *block1 = (struct block *) buffer1; 
  uint16_t *num_entries1 = &((*block1).contents.dirnode.num_entries);
  //check if the name exists in the current_directory
  for(int i = 0; i < *num_entries1; i++){
    //compare the given file with the list of the names in the block
    if(strcmp(name, (*block1).contents.dirnode.entries[i].name) == 0){
      //if the name exists
      //we first check if it is a directory or a file...
      //as if it is a directory some stats can be omitted
      block_num_t stats = (*block1).contents.dirnode.entries[i].block_num;        
      void *buffer2 = malloc(BLOCK_SIZE);
      read_block(stats, buffer2);
      struct block *block2 = (struct block *) buffer2;
      if(is_dir(stats)){
        //we know the value of a directory is a 0
        buf->is_dir = 0;
        strncpy(buf->name, name, strlen(name) + 1);
        buf->block_num = stats;
      }else{
         //we know the value of a file is a 1
        buf->is_dir = 1;
        strncpy(buf->name, name, strlen(name) + 1);
        buf->block_num = stats;
        buf->file_size = block2->contents.inode.file_size;
        uint32_t data_blocks= buf->file_size / BLOCK_SIZE;
        uint32_t check1 = buf->file_size % BLOCK_SIZE;
        if(check1 != 0){
          data_blocks += 1;
        }
        buf->num_data_blocks = data_blocks;
      }
      free(buffer1);
      free(buffer2);
      return E_SUCCESS;
    }
  }
  //this is when the name does not match with any of the names inside the current_directory. 
  free(buffer1);
  return E_NOT_EXISTS;
}


/* jfs_write
 *   appends the data in the buffer to the end of the specified file
 * file_name - name of the file to append data to
 * buf - buffer containing the data to be written (note that the data could be
 *   binary, not text, and even if it is text should not be assumed to be null
 *   terminated)
 * count - number of bytes in buf (write exactly this many)
 * returns 0 on success or one of the following error codes on failure:
 *   E_NOT_EXISTS, E_IS_DIR, E_MAX_FILE_SIZE, E_DISK_FULL
 */
int jfs_write(const char* file_name, const void* buf, unsigned short count) {
  //the toughest part
  //first read the current directory
  void *buffer1 = malloc(BLOCK_SIZE);
  //we can read the given block using the read_block() function
  read_block(current_dir, buffer1);
  //typecasting the buffer we have to be the struct block type
  struct block *block1 = (struct block *) buffer1; 
  uint16_t *num_entries1 = &((*block1).contents.dirnode.num_entries);
  //check if the name exists in the current_directory
  for(int i = 0; i < *num_entries1; i++){
    //compare the given file with the list of the names in the block
    if(strcmp(file_name, (*block1).contents.dirnode.entries[i].name) == 0){
      //now if the name exists, check if it is a directory
      //and for this we use the is_dir() helper function
      if(is_dir((*block1).contents.dirnode.entries[i].block_num)){
        free(buffer1);
        return E_IS_DIR;
      }else{
        //if the name exists and if it is a file
        //read the file
        block_num_t file = (*block1).contents.dirnode.entries[i].block_num;        
        void *buffer2 = malloc(BLOCK_SIZE);
        read_block(file, buffer2);
        struct block *block2 = (struct block *) buffer2;
        //check for E_MAX_FILE_SIZE
        uint32_t file_size1 = block2->contents.inode.file_size;
        uint32_t file_size2 = file_size1 + count; //we are adding count to the current filesize as count is the number of bytes in buf 
        if(file_size2 > MAX_FILE_SIZE){
          free(buffer1);
          free(buffer2);
          return E_MAX_FILE_SIZE;
        }else{
          //if the file won't exceed size after appending the data
          //check if the disk won't exceed capacity after appending the data
          uint32_t data_block1 = file_size1 / BLOCK_SIZE;
          uint32_t check1 = file_size1 % BLOCK_SIZE;
          if(file_size1 != 0 && check1 != 0){
            data_block1 += 1;
          }
          //the number of data blocks after appending the data
          uint32_t data_block2 = file_size2 / BLOCK_SIZE;
          uint32_t check2 = file_size2 % BLOCK_SIZE;
          if(file_size2 != 0 && check2 != 0){
            data_block2 += 1;
          }
          int32_t data_block3 = data_block2 - data_block1;
          //since for writing we may need multiple data blocks...we generate an array of new_blocks using the allocate_block() function
          block_num_t new_block[data_block3];
          int32_t i2 = 0;
          //and since it is an array of blocks
          //we need to generate one by one through a loop
          while(i2 < data_block3){
            //allocating each of the multiple blocks needed 
            new_block[i2] = allocate_block();
            if(new_block[i2] == 0){ //if at the middle of the array of new_blocks, allocate_block() cannot find anymore unallocated blocks
              i2 -= 1;
              //we have to unallocate the blocks allocated till now
              while(i2 >= 0){
                release_block(new_block[i2]);
                i2--;
              }
              //and then return the error code after freeing the involved buffers
              free(buffer1);
              free(buffer2);
              return E_DISK_FULL;
            }
            i2 += 1;
          }
          //update the file_size and the datablocks of the file to which we are appending
          block2->contents.inode.file_size = file_size2;
          //adding in the newly allocated blocks
          for(int32_t i3 = 0; i3 < data_block3; i3++){
            block2->contents.inode.data_blocks[i3 + data_block1] = new_block[i3];
          }
          write_block(file, buffer2);
          //now to append the data from the buf to the file, we need an additional two buffers
          void *buffer3; 
          if(file_size1 != 0){
            buffer3 = malloc(BLOCK_SIZE * (data_block3 + 1));
            memset(buffer3, -1, BLOCK_SIZE * (data_block3 + 1));
            //read the last datablock
            void *buffer4 = malloc(BLOCK_SIZE);
            read_block(block2->contents.inode.data_blocks[data_block1 - 1], buffer4);
            //the last datablock is copied into the buffer
            memcpy(buffer3, buffer4, BLOCK_SIZE); 
            free(buffer4);
            if(check1 == 0){ //if the datablock is not partially full
              memcpy(buffer3 + BLOCK_SIZE, buf, count);
            }else{ //if it is partially full
              memcpy(buffer3 + check1, buf, count);
            } 
          }else{ //if the file was empty 
          buffer3 = malloc(BLOCK_SIZE * data_block3);
          memset(buffer3, -1, BLOCK_SIZE * data_block3);
          memcpy(buffer3, buf, count);
        }
        uint32_t start = 0;
        if(data_block1 > 0){
          start = data_block1 - 1;
        }
        for(uint32_t i4 = 0; start < data_block2; i4++){
          write_block(block2->contents.inode.data_blocks[start], buffer3 + BLOCK_SIZE * i4);
          start += 1;
        }
        free(buffer1);
        free(buffer2);
        free(buffer3);
        return E_SUCCESS;
        }
      }
    }
  }
  //this is when the name does not match with any of the names inside the current_directory. 
  free(buffer1);
  return E_NOT_EXISTS;
}


/* jfs_read
 *   reads the specified file and copies its contents into the buffer, up to a
 *   maximum of *ptr_count bytes copied (but obviously no more than the file
 *   size, either)
 * file_name - name of the file to read
 * buf - buffer where the file data should be written
 * ptr_count - pointer to a count variable (allocated by the caller) that
 *   contains the size of buf when it's passed in, and will be modified to
 *   contain the number of bytes actually written to buf (e.g., if the file is
 *   smaller than the buffer) if this function is successful
 * returns 0 on success or one of the following error codes on failure:
 *   E_NOT_EXISTS, E_IS_DIR
 */
int jfs_read(const char* file_name, void* buf, unsigned short* ptr_count) {
  //read the current_directory as before
  //first read the current directory
  void *buffer1 = malloc(BLOCK_SIZE);
  //we can read the given block using the read_block() function
  read_block(current_dir, buffer1);
  //typecasting the buffer we have to be the struct block type
  struct block *block1 = (struct block *) buffer1; 
  uint16_t *num_entries1 = &((*block1).contents.dirnode.num_entries);
  //check if the name exists in the current_directory
  for(int i = 0; i < *num_entries1; i++){
    //compare the given file with the list of the names in the block
    if(strcmp(file_name, (*block1).contents.dirnode.entries[i].name) == 0){
      //only if the name exists, we can read data from it 
      //so now check if it is a directory or a file 
      if(is_dir((*block1).contents.dirnode.entries[i].block_num)){
        free(buffer1);
        return E_IS_DIR;
      }else{
        //if it is not a directory and is a file
        //we need to have another similar setup to read the file
        block_num_t file = (*block1).contents.dirnode.entries[i].block_num;
        void *buffer2 = malloc(BLOCK_SIZE);
        //we can read the given block using the read_block() function
        read_block(file, buffer2);
        //typecasting the buffer we have to be the struct block type
        struct block *block2 = (struct block *) buffer2; 
        uint32_t file_size = block2->contents.inode.file_size;
        //if the data present is less than the size of the buf
        if(file_size < *ptr_count){
          //adjusting the size of the buf using the pointer
          *ptr_count = file_size;
        }
        int32_t file_size1 = (int32_t) *ptr_count;
        uint32_t i = 0;
        //to take the data from the file and to write to the buf, we need a buffer
        uint32_t data_blocks = file_size1 / BLOCK_SIZE;
        if(file_size1 != 0 && file_size1 % BLOCK_SIZE != 0){
          data_blocks += 1;
        }
        void *buffer3 = malloc(BLOCK_SIZE * (data_blocks + 1));
        while(file_size1 > 0){
          //data from the datablocks of the file is written into the temp buffer
          read_block(block2->contents.inode.data_blocks[i], buffer3 + i * BLOCK_SIZE);
          i += 1;
          file_size1 -= BLOCK_SIZE;
        }
        //the data from the file which is stored in the temp buffer can now be copied into the buf provided
        memcpy(buf, buffer3, *ptr_count);
        free(buffer1);
        free(buffer2);
        free(buffer3);
        return E_SUCCESS;
      }
    }
  }
  //this is when the name does not match with any of the names inside the current_directory. 
  free(buffer1);
  return E_NOT_EXISTS;
}



/* jfs_unmount
 *   makes the file system no longer accessible (unless it is mounted again).
 *   This should be called exactly once after all other jfs_* operations are
 *   complete; it is invalid to call any other jfs_* function (except
 *   jfs_mount) after this function complete.  Basically, this closes the DISK
 *   file on the _real_ file system.  If your code requires any clean up after
 *   all other jfs_* functions are done, you may add it here.
 * returns 0 on success or -1 on error; errors should only occur due to
 *   errors in the underlying disk syscalls.
 */
int jfs_unmount() {
  int ret = bfs_unmount();
  return ret;
}
