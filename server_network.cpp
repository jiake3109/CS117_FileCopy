#include "serverntwork.h"

#define ACK "ACK"
#define REJ "REJ"
#define LAST "LAST"
#define CONT "CONT"
#define BUFSIZE 512
#define WRITESIZE 512
#define PARTSIZE 5120
#define MAXTIME 30000
#define STATUS 4
#define SHA_MSG 20
#define CONTENT_SIZE 50
#define UPPERBOUND 1000000
using namespace std;
using namespace C150NETWORK;  // for all the comp150 utilities

void printSHA1(unsigned char *partialSHA1dup);
void parseHeaderField(unsigned char *receivedBuf);
bool compareSHA1(unsigned char* receivedSHA1, unsigned char* calculateSHA1);


typedef struct header_info{
    unsigned char filenameSHA1[20];
    unsigned char contentSHA1[20];
    char filename_str[256]; // no filename can have length higher than 255
    size_t contentSize;
} *Socket_Header;

/* * * * * *  * * * * * * * * * *  NETWORK CLIENT FUNCTIONS  * * * * * * * * * * * * * * * */

  
bool 
parseHeaderField(unsigned char *receivedBuf, fileProp& received_file)
{
    cout << "parse header field " << endl;
    // casting received 304 bytes into header object 
    Socket_Header header = (Socket_Header)malloc(sizeof(struct header_info));
    memcpy((void *)header, receivedBuf, sizeof(struct header_info));
    // check if filename SHA1 and filename matches 
    unsigned char *calc_filenameSHA1 = (unsigned char*)malloc(20 * sizeof(unsigned char));
    SHA1((unsigned char *)header->filename_str, strlen(header->filename_str), calc_filenameSHA1);
    if (!compareSHA1(calc_filenameSHA1, header->filenameSHA1)){
        cout << "calculated filnameSHA1" << endl;
        printSHA1(calc_filenameSHA1);
        cout << "received filnameSHA1" << endl;
        printSHA1(header->filenameSHA1);
        return false;  // error raised with unmatched filename SHA1 
    } 
    cout << "after checking filename SHA1 \n";

    // populate fileProp with received information 
    memcpy((void *)received_file.fileSHA1, (void *)header->contentSHA1, 20);
    cout << "recieved file name" << header->filename_str << endl;
    received_file.contentSize = header->contentSize;
    cout << header->contentSize << endl;
    int filename_len = strlen(header->filename_str);
    cout << "file name size "<< filename_len << endl;
    char filename_string_arr[filename_len + 1];
    strcpy(filename_string_arr, header->filename_str);
    cout << "copied file name is  " << filename_string_arr;
    received_file.filename = string(filename_string_arr);
    cout << "copied received filename" << received_file.filename << endl;
    printSHA1(header->contentSHA1);
    free(header);
    cout << "copied received filename" << received_file.filename << endl;
    return true;
}




/* * * * * *  * * * * * * * * * *  NETWORK SERVER FUNCTIONS  * * * * * * * * * * * * * * * */
void 
FileReceiveE2ECheck(C150DgmSocket& sock, vector<fileProp>& allArrivedFiles)
{
    *GRADING << "Begining to receive whole files from client." << endl; 
    while (1){
        if (readfromSrc(sock, allArrivedFiles)) break;
    }
    *GRADING << "Received and successfully read a total of " << allArrivedFiles.size() << " files from client." << endl;
}

/* readSizefromSocket 
 * purpose: read provided amount of bytes from socket 
 * parameter: 
 *      C150DgmSocket& sock: reference to socket object 
 *      size_t bytestoRead: number of bytes to read from socket
 *      char& bytes_storage: character array to store read bytes passed by reference
 * return: True if no timeout and signalize continuation of program, False if timeout 
 *          and this iteration terminates.
 * notes: none
*/
bool 
readSizefromSocket(C150DgmSocket& sock, size_t bytestoRead, char** bytes_storage)
{
    bzero((*bytes_storage), bytestoRead);
    sock.read((*bytes_storage), bytestoRead);
    /* evaluate conditions to send ACK or REJ */
    if (sock.timedout()){ 
        sock.write(REJ, strlen(REJ));
        return false;
    }
    sock.write(ACK, strlen(ACK));
    return true;
    
}

/* compareSHA1 
 * purpose: compare the receivedSHA1 and calculatedSHA1 byte by byte 
 * parameter: 
 * return: True if every byte match, False if any byte does not match
 */
bool 
compareSHA1(unsigned char* receivedSHA1, unsigned char* calculateSHA1)
{
    for (int j = 0; j < 20; j++) {
        if (receivedSHA1[j] != calculateSHA1[j])
            return false;
    }
    return true;

}

/* readContentfromSocket 
 * purpose: read the incoming packets that represent content from server socket
 * parameter: 
 *     C150DgmSocket& sock: reference to socket object
 *     size_t contentSize: total bytes to read from the content
 *     unsigned char** read_result_addr: address of pointer to the finally read bytes
 * return: True if no timeout and signalize continuation of program, False if timeout 
 *          and this iteration terminates. 
 * notes: none
*/
bool
readContentfromSocket(C150DgmSocket& sock, int contentlen, unsigned char** read_result_addr)
{
    vector<unsigned char> allFileContent; /* dynamically expanded memory for all content bytes */
    ssize_t totalBytes = 0;
    ssize_t chunk = 0;
    ssize_t curBytes = 0;
    unsigned char* temporaryBuf = (unsigned char*)malloc(contentlen * sizeof(unsigned char));
    int readtimes = 0;
    while(totalBytes != contentlen) { /* read until all bytes from sockets are received */
        if (readtimes == 10){
            unsigned char* temp = (unsigned char*)malloc(curBytes * sizeof(unsigned char));
            bzero(temp, curBytes); /* zero out the field */
            int index = 0;
            for (unsigned int i = totalBytes - curBytes; i < totalBytes; i++) {
                *(temp + index) = allFileContent.at(i); 
                index++;
            }
            unsigned char *pobuf = (unsigned char*) malloc(SHA_MSG * sizeof(unsigned char));
            SHA1((const unsigned char*)temp, curBytes, pobuf);
            char *partialSHA1 = (char*)malloc(SHA_MSG * sizeof(char));
            if (!readSizefromSocket(sock, SHA_MSG, &partialSHA1)){
                return false;
            } 
            unsigned char *partialSHA1dup = (unsigned char*)malloc(SHA_MSG * sizeof(unsigned char)); 
            bzero(partialSHA1dup, 20);
            for (int j = 0; j < 20; j++){ 
                partialSHA1dup[j] = (unsigned char)partialSHA1[j];
            }
            if (!compareSHA1(pobuf, partialSHA1dup)) {
                *GRADING << "The received <" << "> bytes SHA1 do not match, requesting resend." << endl;
                sock.write(REJ, strlen(REJ));
                return false;
            } else {
                sock.write(ACK, strlen(ACK));
            }
            readtimes = 0;
            curBytes = 0;
            free(temp);
            free(pobuf);
            free(partialSHA1);
            free(partialSHA1dup);
        }
        chunk = sock.read((char *)temporaryBuf, BUFSIZE);
        for (ssize_t t = 0; t < chunk; t += sizeof(unsigned char)) {
            /* put read bytes into dynamically expanding memories */
            allFileContent.push_back(*(temporaryBuf + t)); }
        bzero(temporaryBuf, chunk); /* clean up the buf for next read */  
        curBytes += chunk;
        totalBytes += chunk;
        readtimes++;
        if (sock.timedout()){
            printf("oops timed out.\n");
            sock.write(REJ, strlen(REJ));
            return false;
        }
    }
    /* evaluate conditions to send ACK */
    if (!sock.timedout()){ /* confirm entire read for content */
        sock.write(ACK, strlen(ACK));
    }else{
        sock.write(REJ, strlen(REJ));
        return false;
    }
    /* compare total received bytes with received contentlen field */
    if (totalBytes == contentlen){
        sock.write(ACK, strlen(ACK));
    } else {
        sock.write(REJ, strlen(REJ));
        return false;
    }

    /* calculate SHA1 of the received content bytes */
    unsigned char* prod = (unsigned char*)malloc((allFileContent.size())*sizeof(unsigned char));
    bzero(prod, allFileContent.size()); /* zero out the field */
    for (unsigned int i = 0; i < allFileContent.size(); i ++) {
        *(prod + i) = allFileContent.at(i); 
    }
    *read_result_addr = prod;
    return true;
}

/* readfromSrc 
 * purpose: read any incoming packets in order from the server socket 
 * parameter: 
 *     C150DgmSocket& sock: reference to socket object 
 * return: True if the socket have received the last packet in the 
 *          network stream, False if the socket is expecting more
 *          packets in the next read 
 * notes: none  
 */
bool
readfromSrc(C150DgmSocket& sock, vector<fileProp>& allArrivedFiles)
{
    /* 1ACK-receive fileheader from client */
    unsigned char *fileHeader = (unsigned char*)malloc(sizeof(struct header_info));
    struct fileProp *fileInfo = (struct fileProp *)malloc(sizeof(struct fileProp));
    bzero(fileHeader, sizeof(struct header_info));
    sock.read((char*)fileHeader, sizeof(struct header_info));
    cout << "error after line 383 " << endl;
    /* evaluate conditions to send ACK or REJ */
    // if (sock.timedout()){ 
    //     cerr << "timed out error in write" << endl;
    //     sock.write(REJ, strlen(REJ));
    //     return false;
    // }
    /*1ACK-receive header information and correct filenameSHA1 */
    if (parseHeaderField(fileHeader, *fileInfo))
    {
        sock.write(ACK, strlen(ACK));
    }else{
        sock.write(REJ, strlen(REJ));
        return false;
    }

    /* 2ACK-receive content from client */
    /* many ACKs for receiving contents in packets and with correct SHA1 */
    int contentlen = fileInfo->contentSize;
    unsigned char* prod = nullptr;
    if (!readContentfromSocket(sock, contentlen, &prod)){
        /* unsuccessful read of the entire content */
        *GRADING << "File: <" << fileInfo->filename << "> failed at socket to socket check for failing to receive contents from client." << endl;
        free(prod);
        return false;
    }
    *GRADING << "File: <" << fileInfo->filename << "> succeed at socket to socket check with receiving the entire content." << endl;
    unsigned char *obuf = (unsigned char*)malloc(SHA_MSG * sizeof(unsigned char));
    SHA1((const unsigned char *)prod, contentlen, obuf);

    /*9ACK-check consistency in received and calculated SHA1 to client */
    if (!compareSHA1(obuf, fileInfo->fileSHA1)) {
        sock.write(REJ, strlen(REJ));
        *GRADING << "File: <" << fileInfo->filename << "> socket to socket failed at inconsistency in received and calculated content SHA1." << endl;
        return false;
    }
    sock.write(ACK, strlen(ACK)); /* log for purpose of recording failure caused by network nastiness */
    *GRADING << "File: <" << fileInfo->filename << ">  succeed at socket to socket check with consistency in size and SHA1 of the content." << endl;
    fileInfo->contentbuf = prod;
    allArrivedFiles.push_back(*fileInfo);
    free(obuf);
    free(prod);
    *GRADING << "File: <" << fileInfo->filename << "> socket to socket end-to-end check succeeded." << endl;
    return true;
}


/* Helper function for viewing SHA1 */
void 
printSHA1(unsigned char *partialSHA1dup)
{
    printf("received SHA1[");
    for (int i = 0; i < 20; i++)
    {
        printf ("%02x", (unsigned int) partialSHA1dup[i]);
    }
    printf("].\n");
}
