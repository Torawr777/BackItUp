#include "procDir.c"


int main(int argc, char const *argv[]) {

    if(argc == 1) {

        char current[4096];

        printf("# BackUp \n");
     
        createBackup();
        route *head = dummyNode();
        getcwd(current, sizeof(current));
        recursiveSearch(current, head, 0);
        createCopyThreads(head, 0);

        route *buffer;

        while(head) {
            buffer = (*head).next;
            free((*head).address);   
            free(head);
            head = buffer;
        }

        return(0);
    }

    else if(argc == 2 && strcmp(argv[1], "-r") == 0) {

        char path[4096];

        printf("# Restoring \n");
       
        route *head = dummyNode();

        getcwd(path, sizeof(path)); 
        strcat(path, "/.backup");
        recursiveSearch(path, head, 1);
        createCopyThreads(head, 1);

        route *buffer;
        while(head) {
            buffer = (*head).next;
            free((*head).address);   
            free(head);
            head = buffer;
        }

        return 0;
    }
}
