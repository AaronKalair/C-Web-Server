#include <sys/socket.h>       // socket definitions
#include <sys/types.h>        // socket types
#include <arpa/inet.h>        // inet (3) funtions
#include <unistd.h>           // misc. UNIX functions
#include <signal.h>           // signal handling
#include <stdlib.h>           // standard library
#include <stdio.h>            // input/output library
#include <string.h>           // string library
#include <errno.h>            // error number library
#include <fcntl.h>            // for O_* constants
#include <sys/mman.h>         // mmap library
#include <sys/stat.h>         // more constants
#include <sys/time.h>
#include "routes.c"

// global constants
#define PORT 2001             // port to connect on

int list_s;                   // listening socket

// structure to hold the return code and the filepath to serve to client.
typedef struct {
	int returncode;
	char *filename;
	char *content;
} httpRequest;

// headers to send to clients
char *header200 = "HTTP/1.0 200 OK\nServer: C-Serv v0.2\nContent-Type: %s\n\n";
char *header400 = "HTTP/1.0 400 Bad Request\nServer: C-Serv v0.2\nContent-Type: %s\n\n";
char *header404 = "HTTP/1.0 404 Not Found\nServer: C-Serv v0.2\nContent-Type: %s\n\n";

// public directory
char* publicDir = "public_html";

// get a message from the socket until a blank line is recieved
char *getMessage(int fd) {
  
    // A file stream
    FILE *sstream;
    
    // Try to open the socket to the file stream and handle any failures
    if( (sstream = fdopen(fd, "r")) == NULL)
    {
        fprintf(stderr, "Error opening file descriptor in getMessage()\n");
        exit(EXIT_FAILURE);
    }
    
    // Size variable for passing to getline
    size_t size = 1;
    
    char *block;
    
    // Allocate some memory for block and check it went ok
    if( (block = malloc(sizeof(char) * size)) == NULL )
    {
        fprintf(stderr, "Error allocating memory to block in getMessage\n");
        exit(EXIT_FAILURE);
    }
  
    // Set block to null    
    *block = '\0';
    
    // Allocate some memory for tmp and check it went ok
    char *tmp;
    if( (tmp = malloc(sizeof(char) * size)) == NULL )
    {
        fprintf(stderr, "Error allocating memory to tmp in getMessage\n");
        exit(EXIT_FAILURE);
    }
    // Set tmp to null
    *tmp = '\0';
    
    // Int to keep track of what getline returns
    int end;
    // Int to help use resize block
    int oldsize = 1;
    
    // While getline is still getting data
    while( (end = getline( &tmp, &size, sstream)) > 0)
    {
        // If the line its read is a caridge return and a new line were at the end of the header so break
        if( strcmp(tmp, "\r\n") == 0)
        {
            break;
        }
        
        // Resize block
        block = realloc(block, size+oldsize);
        // Set the value of oldsize to the current size of block
        oldsize += size;
        // Append the latest line we got to block
        strcat(block, tmp);
    }
    
    // Free tmp a we no longer need it
    free(tmp);
    
    // Return the header
    return block;

}

// send a message to a socket file descripter
int sendMessage(int fd, char *msg) {
    return write(fd, msg, strlen(msg));
}

// Extracts the filename needed from a GET request and adds public_html to the front of it
char * getRequestPath(char* msg)
{
    // Variable to store the filename in
    char * file;
    // Allocate some memory for the filename and check it went OK
    if( (file = malloc(sizeof(char) * strlen(msg))) == NULL)
    {
        fprintf(stderr, "Error allocating memory to file in getRequestPath()\n");
        exit(EXIT_FAILURE);
    }
    
    // Get the filename from the header
    sscanf(msg, "GET %s HTTP/1.1", file);

    return file;
}

char * publicFilePath(char* file) {
    // Allocate some memory not in read only space to store "public_html"
    char *base;
    if( (base = malloc(sizeof(char) * (strlen(file) + 18))) == NULL)
    {
        fprintf(stderr, "Error allocating memory to base in publicFilePath()\n");
        exit(EXIT_FAILURE);
    }
    
    // Copy public_html to the non read only memory
    strcpy(base, publicDir);
    
    // Append the filename after public_html
    strcat(base, file);

    return base;
}

// parse a HTTP request and return an object with return code and filename
httpRequest parseRequest(char *msg){
    httpRequest ret;
    ret.filename = NULL;
    ret.content = NULL;
       
    // A variable to store the name of the file they want
    char* filename;
    // Allocate some memory to filename and check it goes OK
    if( (filename = malloc(sizeof(char) * strlen(msg))) == NULL)
    {
        fprintf(stderr, "Error allocating memory to filename in parseRequest()\n");
        exit(EXIT_FAILURE);
    }
    // Find out what page they want
    filename = getRequestPath(msg);
    printf("request for %s ",filename);
    
    // Check if the public page they want exists
    FILE *exists = fopen(publicFilePath(filename), "r" );
    
    // Check if its a directory traversal attack
    if( strstr(filename, "..") != NULL )
    {
        printf("go away hacker\n");
        // Return a 400 header and 400.html
        ret.returncode = 400;
        ret.filename = "400.html";
    }
    
    // If they asked for / return index.html
    else if(strcmp(filename, "/") == 0)
    {
        ret.returncode = 200;
        ret.filename = "public_html/index.html";
    }
    
    // If they asked for a specific page and it exists because we opened it sucessfully return it 
    else if( exists != NULL )
    {
        ret.returncode = 200;
        ret.filename = publicFilePath(filename);
        // Close the file stream
        fclose(exists);
    }

    // If we get here the file they want doesn't exist so return a 404
    else {
      for (int i = 0; i < routeCount; i++) {
        if (strcmp(routes[i]->routename, filename) == 0) {
          // TODO do the custom route action
          ret.returncode = 200;
          ret.content = (routes[i]->routeFnPtr)(10);
          return ret;
        }
      }
      ret.returncode = 404;
      ret.filename = "404.html";
    }
    
    // Return the structure containing the details
    return ret;
}

// print a file out to a socket file descriptor
int printFile(int fd, char *filename) {
  
    /* Open the file filename and echo the contents from it to the file descriptor fd */
    
    // Attempt to open the file 
    FILE *read;
    if( (read = fopen(filename, "r")) == NULL)
    {
        fprintf(stderr, "Error opening file in printFile()\n");
        exit(EXIT_FAILURE);
    }
    
    // Get the size of this file for printing out later on
    int totalsize;
    struct stat st;
    stat(filename, &st);
    totalsize = st.st_size;
    
    // Variable for getline to write the size of the line its currently printing to
    size_t size = 1;
    
    // Get some space to store each line of the file in temporarily 
    char *temp;
    if(  (temp = malloc(sizeof(char) * size)) == NULL )
    {
        fprintf(stderr, "Error allocating memory to temp in printFile()\n");
        exit(EXIT_FAILURE);
    }
    
    // Int to keep track of what getline returns
    int end;
    
    // While getline is still getting data
    while( (end = getline( &temp, &size, read)) > 0)
    {
        sendMessage(fd, temp);
    }
    
    // Final new line
    sendMessage(fd, "\n");
    
    // Free temp as we no longer need it
    free(temp);
    
    // Return how big the file we sent out was
    return totalsize;
  
}

// clean up listening socket on ctrl-c
void cleanup(int sig) {
    
    printf("Cleaning up connections and exiting.\n");
    
    // try to close the listening socket
    if (close(list_s) < 0) {
        fprintf(stderr, "Error calling close()\n");
        exit(EXIT_FAILURE);
    }
    
    // exit with success
    exit(EXIT_SUCCESS);
}

int printHeader(int fd, int returncode, char* contentType)
{
    char* header;
    // Print the header based on the return code
    switch (returncode)
    {
        case 200:
          header = header200;
        break;
        
        case 400:
          header = header400;
        break;
        
        case 404:
          header = header404;
        break;
    }
    if (header != NULL) {
      char interpolatedHeader[ strlen(header) + strlen(contentType) ];
      sprintf( interpolatedHeader, header, contentType );
      sendMessage(fd, interpolatedHeader);
      return strlen(interpolatedHeader);
    } else {
      return -1;
    }
}


int main(int argc, char *argv[]) {
    int conn_s;                  //  connection socket
    short int port = PORT;       //  port number
    struct sockaddr_in servaddr; //  socket address structure
    int childrenToSpawn = 10;    // default to 10
    if (argc >= 2) {
      childrenToSpawn = atoi(argv[1]);
    }
    
    
    // set up signal handler for ctrl-c
    (void) signal(SIGINT, cleanup);
    
    // create the listening socket
    if ((list_s = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
        fprintf(stderr, "Error creating listening socket.\n");
        exit(EXIT_FAILURE);
    }
    
    // set all bytes in socket address structure to zero, and fill in the relevant data members
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port        = htons(port);
    
    // bind to the socket address
    if (bind(list_s, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0 ) {
        fprintf(stderr, "Error calling bind()\n");
        exit(EXIT_FAILURE);
    }
    
    
    // Listen on socket list_s
    if( (listen(list_s, 10)) == -1)
    {
        fprintf(stderr, "Error Listening\n");
        exit(EXIT_FAILURE);
    } 
    
    // Size of the address
    unsigned int addr_size = sizeof(servaddr);
    
    // Sizes of data were sending out
    int headersize;
    int pagesize;
    // Number of child processes we have spawned
    int children = 0;
    // Variable to store the ID of the process we get when we spawn
    pid_t pid;

    printf("server starting on port %d\n", port);
    // Loop infinitly serving requests
    while(1)
    {
    
        if (children < childrenToSpawn) {
          pid = fork();
          children++;
        }
        
        // If the pid is -1 the fork failed so handle that
        if( pid == -1)
        {
            fprintf(stderr,"can't fork, error %d\n" , errno);
            exit (1);
        }
        
        // Have the child process deal with the connection
        if ( pid == 0)
        {	
            printf("process starting\n");
            // Have the child loop infinetly dealing with a connection then getting the next one in the queue
            while(1)
            {
                // Accept a connection
                conn_s = accept(list_s, (struct sockaddr *)&servaddr, &addr_size);
                struct timeval start;
                gettimeofday(&start,NULL);
                    
                // If something went wrong with accepting the connection deal with it
                if(conn_s == -1)
                {
                    fprintf(stderr,"Error accepting connection \n");
                    exit (1);
                }
                
                // Get the message from the file descriptor
                char * header = getMessage(conn_s);
                
                // Parse the request
                httpRequest details = parseRequest(header);
                
                // Free header now were done with it
                free(header);
                
                // Print out the correct header
                char * contentType;
                if (details.filename != NULL) {
                  contentType = "text/html";
                } else {
                  contentType = "application/json";
                }
                headersize = printHeader(conn_s, details.returncode, contentType);
                
                if (details.filename != NULL) {
                  // Print out the file they wanted
                  pagesize = printFile(conn_s, details.filename);
                } else {
                  pagesize = strlen(details.content);
                  sendMessage(conn_s, details.content);
                  sendMessage(conn_s, "\n");
                }
                
                struct timeval endtime;
                gettimeofday(&endtime,NULL);
                // Print out which process handled the request and how much data was sent
                printf("served by process %d for %d bytes in %d usec\n", getpid(), headersize+pagesize, endtime.tv_usec - start.tv_usec);
                
                // Close the connection now were done
                close(conn_s);
            }
        }
    }
    
    return EXIT_SUCCESS;
}
