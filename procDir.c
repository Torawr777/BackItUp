#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <libgen.h> 
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>   


typedef struct route {

    int backUpRestore, threadNum, copyBytes; 
    char *address; 
    struct route *next; 

} route;

// Create .backup directory if none exist
void createBackup() {

    char *backDirectory = ".backup";
    struct stat backAttribute;

    if(mkdir(".backup", 0700) == 0) {
        printf("Creating .backup directory\n");
    }

    else if(stat(backDirectory, &backAttribute) == 0 && S_ISDIR(backAttribute.st_mode)) {
        printf(".backup directory already exists\n"); 
    }
}

// Test if address matches the ./BackItUp 
int confirmAddress(char *path) {

    char cmp[4096];

    getcwd(cmp, sizeof(cmp));
    strcat(cmp, "/BackItUp");

    if(strcmp(path, cmp) == 0) { return 1; }
    else { return 0; }
}


// Check to see original vs backup last modification
int recent(char *originalFile, char *backupFile) {

    struct stat originalInfo, backupInfo;

    if(stat(originalFile, &originalInfo) != 0) { return -1; }

    if(stat(backupFile, & backupInfo) != 0) { return - 1; }

    // backup is more recent
    if(backupInfo.st_mtime > originalInfo.st_mtime) { return 1; }

    // original is more recent
    else { return 0; }

}

// Simplifies the linked list op
route *dummyNode() {

    route *head = (route *) calloc(1, sizeof(route));

    // allocate memory for 5 characters + 1 for the null terminator
    (*head).address = calloc(6, sizeof(char)); 

    strcpy((*head).address, "dummy");

    (*head).next = NULL;

    // Resize the memory allocation of the `head` pointer
    route *temp = (route *) realloc(head, sizeof(route) * 2);

    if(temp == NULL) {
        printf("Reallocation failed\n");
        return head;
    }

    head = temp;
    return head;
}


// Append a new node to the end of linked list
route *append(route *prevfp, char *filePath) {
   
    route *lastfp = prevfp;
    for(; (*lastfp).next != NULL; lastfp = (*lastfp).next) {}

    route *p = (route *)malloc(sizeof(route));
    
    if(p == NULL) {
        printf("Allocation failed\n");
        return NULL;
    }

    (*p).address = strdup(filePath);

    if((*p).address == NULL) {
        printf("Allocation failed\n");
        free(p);
        return NULL;
    }

    (*lastfp).next = p;
    (*p).next = NULL;
    return(p);
}

// Create the name of the backup file with .backup appended into path
char *generateBackUp(char *backAttribute, char *backUpAddress, int backUpRestore) {

    char cwd[4096];
    char *addressSave = strdup(backAttribute);
    char *pComponent = strtok(addressSave, "/");

    getcwd(cwd, sizeof(cwd));

    switch(backUpRestore) {

        case 0:
            while(pComponent != NULL) { 
                strcat(backUpAddress, "/");
                strcat(backUpAddress, pComponent);

                if(strcmp(cwd, backUpAddress) == 0) { break; }
                pComponent = strtok(NULL, "/");   
            } 
            strcat(backUpAddress, "/.backup");
            break;
            
        case 1:
            while(pComponent != NULL) { 
                if(strcmp(pComponent, ".backup") == 0) { break; }
                pComponent = strtok(NULL, "/");   
            } 
            strcpy(backUpAddress, cwd);
            break;
            
        default:
            printf("Not either backUp(0) or restore(1)");
            break;
    }

    pComponent = strtok(NULL, "/");

    while(pComponent != NULL) {
        strcat(backUpAddress, "/");
        strcat(backUpAddress, pComponent);
        pComponent = strtok(NULL, "/"); 
    }

    free(addressSave);
    return(backUpAddress);
}


// Thread function is passed a filePath to copy into the .backup backDirectory/current working directory
void *backupFile(void *file) {

    FILE *inFile, *outFile;
    int byteCounter = 0;
    char buffer[4096];
    size_t rBytes;

    route *fileToCopy = file;

    char *backUpAddress = calloc(4096, sizeof(char));

    // If backing up, append /.backup and .bak to the path
    if((*fileToCopy).backUpRestore == 0) {
        backUpAddress = generateBackUp((*fileToCopy).address, backUpAddress, 0);
        strcat(backUpAddress, ".bak\0");
    }

    // If restoring, remove /.backup and .bak from the path
    else if((*fileToCopy).backUpRestore == 1) {
        backUpAddress = generateBackUp((*fileToCopy).address, backUpAddress, 1);
        backUpAddress[strlen(backUpAddress) - 4] = 0;
    }

    int n = recent((*fileToCopy).address, backUpAddress);

    // If backup is recent, no action needed
    if(n == 1) {

        if((*fileToCopy).backUpRestore == 0) {
            char *filename1 = basename((*fileToCopy).address);
            printf("[thread %d] %s is already the most current version\n", (*fileToCopy).threadNum, filename1);
        }

        else if((*fileToCopy).backUpRestore == 1) {
            printf("[thread %d] %s is already the most current version\n", (*fileToCopy).threadNum, basename(backUpAddress));
        } 
    }

    // But if original file is more recently modified or backup isnt created yet, copy into .backup/cwd
    else if(n == -1 || n == 0) {

        // If we're restoring, dont overwrite the current ./BackItUp executable
        if((*fileToCopy).backUpRestore == 1) {

            if(confirmAddress(backUpAddress) == 1) {
                free(backUpAddress);
                return 0;
            }
        }

        // If backup file already exists in .backup
        if(n == 0) { printf("[thread %d] WARNING: Overwriting %s\n", (*fileToCopy).threadNum, basename(backUpAddress)); }
        
        if((inFile = fopen((*fileToCopy).address, "rb")) == NULL || (outFile = fopen(backUpAddress, "wb")) == NULL) {
            fprintf(stderr, "Error: fopen %d (%s)\n", errno, strerror(errno));
            free(backUpAddress);
            exit(1);
        }

        // Copy inFile contents into outFile
        do {
            rBytes = fread(buffer, 1, 4096, inFile);
            if(rBytes > 0) {
                size_t bytes_written = fwrite(buffer, 1, rBytes, outFile);

                if(bytes_written != rBytes) {
                    perror("[thread %d] backupFile fwrite error");
                    free(backUpAddress);
                    return 0;
                }
                byteCounter += bytes_written;
            }
        } while (rBytes > 0);

        (*fileToCopy).copyBytes = byteCounter;
        char *filename1 = basename((*fileToCopy).address);
        printf("[thread %d] Copied %d bytes from %s to %s\n", (*fileToCopy).threadNum, (*fileToCopy).copyBytes, filename1, basename(backUpAddress));
        
        // Close our FILE
        if(fclose(inFile) != 0 || fclose(outFile) != 0) {
            fprintf(stderr, "Error: fclose %d (%s)\n", errno, strerror(errno));
            free(backUpAddress);
            exit(1);
        }
    }

    free(backUpAddress);
    return 0;
}


// Recursively add files to route, creating folders in .backup
int recursiveSearch(char *directory, route *p, int backUpRestore) {

    int entryCount = 0;   
    char currentPath[4096], fBuffer[4096];    
    struct dirent *current; 

    if(access(directory, F_OK | R_OK) != 0) { 
        fprintf(stderr, "Error: access %d (%s)\n", errno, strerror(errno)); 
        return -1;
    }

    // Avoid backing up cwd. Just retrieve cwd and use it for comparison
    getcwd(currentPath, sizeof(currentPath)); 

    // When restoring, don't restore .backup folder. Concat backup to cwd for comparison
    if(backUpRestore == 1) { strcat(currentPath, "/.backup"); }

    // If directory is not cwd, we backup | directory is not .backup, we create backup
    if(strcmp(directory, currentPath) != 0) {

        char *backUpAddress = calloc(4096, sizeof(char));
        
        if(backUpAddress == NULL) {
            fprintf(stderr, "Error: backupAddress %d (%s)\n", errno, strerror(errno)); 
            return -1;
        }

        // Add /.backup to the path
        if(backUpRestore == 0) { 
            backUpAddress = generateBackUp(directory, backUpAddress, 0); 
        }

        // Remove /.backup from the path and concat to cwd
        else if(backUpRestore == 1) { 
            backUpAddress = generateBackUp(directory, backUpAddress, 1); 
        }

        // Ensure the string is null-terminated
        backUpAddress[4095] = '\0';

        // backUpAddress directory already exists, no action is needed. 
        struct stat backAttribute;
        if(stat(backUpAddress, &backAttribute) == 0 && S_ISDIR(backAttribute.st_mode)) { }

        // backup directory doesn't exist, so create it.
        else { mkdir(backUpAddress, 0700); }

        free(backUpAddress);
    }

        
    route *newPath;        

    DIR *dir = opendir(directory);

    if(dir == NULL) {
       fprintf(stderr, "Error: opendir %d (%s)\n", errno, strerror(errno)); 
        return -1;
    }               

    // Loop through the directory
    while((current = readdir(dir))) { 

        // If the directory is cwd, parent directory, or backup, skip to next loop iteration
        if(strcmp((*current).d_name,".") == 0 || 
           strcmp((*current).d_name,"..") == 0 || 
           strcmp((*current).d_name,".backup") == 0) {
           continue;  
        }           

        // Append the next file to the fBuffer
        size_t dirLength = strlen(directory);
        size_t nameLength = strlen((*current).d_name);
        if(dirLength + 1 + nameLength >= sizeof(fBuffer)) {
            fprintf(stderr, "Error: file path exceeded\n");
            return -1;
        }

        strcpy(fBuffer, directory);
        strcat(fBuffer, "/");
        strcat(fBuffer, (*current).d_name);

        // If the file is a regular and readable
        if((*current).d_type == DT_REG && access(fBuffer, R_OK) == 0) { 

            // If it's the first read, append it to the dummy in route
            if(entryCount == 0) {
                newPath = append(p, fBuffer);
                entryCount = entryCount + 1;
            }

            // Append to the previous read fBuffer in route
            else {
                newPath = append(newPath, fBuffer);
                entryCount = entryCount + 1;
            }
        }

        // Otherwise if it's a directory
        else if((*current).d_type == DT_DIR) { 

            // If its the first read, call recursiveSearch with the dummy node
            if(entryCount == 0) {
                if(backUpRestore == 0) { recursiveSearch(fBuffer, p, 0); }
                else if(backUpRestore == 1) { recursiveSearch(fBuffer, p, 1); }
            }
            // Otherwise, call recursiveSearch with the previous fBuffer
            else {
                if(backUpRestore == 0) { recursiveSearch(fBuffer, newPath, 0); }
                else if(backUpRestore == 1) { recursiveSearch(fBuffer, newPath, 1); }
            }
        }
    }

    // Close the directory
    closedir(dir);
    return 0;
}


// Create threads for each file to copy
int createCopyThreads(route *head, int backUpRestore) {

    int addressCount = 0, threadCount = 1, bytes = 0, fCount = 0;
   
    route *p = (*head).next;
  
    while(p != NULL) {
        addressCount = addressCount + 1;
        p = (*p).next;
    }

    pthread_t threads[addressCount];           
  
    p = (*head).next;

    while(p != NULL) {

        (*p).threadNum = threadCount;
        (*p).backUpRestore = backUpRestore;
        (*p).copyBytes = 0;

        char *filename1;
        char *filename2;
        
        switch(backUpRestore) {

            case 0:
                filename1 = basename((*p).address);
                printf("[thread %d] Backing up %s\n", (*p).threadNum, filename1);
                break;

            case 1:
                filename2 = basename((*p).address);
                printf("[thread %d] Restoring %s\n", (*p).threadNum, filename2);
                break;

            default:
                printf("Not either backUp(0) or restore(1)");
                break;
        }
        
        if(pthread_create(&threads[threadCount - 1], NULL, backupFile, p) != 0) {
            fprintf(stderr, "Error: pthread_create %d (%s)\n", errno, strerror(errno)); 
            return -1;
        }

        threadCount++;
        p = (*p).next;
    }

    int x = 0;
    while(x < addressCount) {
        if(pthread_join(threads[x], NULL) != 0) {
            fprintf(stderr, "Error: pthread_join %d (%s)\n", errno, strerror(errno)); 
            return -1;
        }
        x++;
    }

    p = (*head).next;

    while(p != NULL) {

        int x = (*p).copyBytes;

        if(x != 0) {
            bytes = bytes + x;
            fCount = fCount + 1;
        }
        p = (*p).next;
    }

    printf("Successfully copied %d files (%d bytes)\n", fCount, bytes);
    return 0;
}
