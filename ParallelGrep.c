/*  
    Copyright 2011 by Peng (Patric) Zhao. All rights reserved.
    
    Email:  patric.zhao@gmail.com
 
    Compile:  gcc grep.c -o grep -lpthread
    
    NAME
       grep - print lines matching a pattern
 
    SYNOPSIS
       grep [OPTIONS] PATTERN [FILE...]
          
       Currently, only one option "-r" is supported with the below format.
	   grep -r PATTERN [FILE...]
	
       Examples:
       1) grep "Patric" /home/patric/grep/grep.c
          * search string "Patrice" in the specified file.

       2) grep -r "Patric" /home/patric/grep
          * search string "Patric" in the directory "grep" recursively.
		  
    DESCRIPTION
		The PARALLEL grep is an enhancement for the stardard grep under the Linux
		system. In the first version, the multi-threads technology is used.

		Now, two kind of parallel modes are employed for both fine and coarse granularity.
	
		1) Fine Parallel for a BIG file
		We use N threads to grep a big file stimultanously based domain
		decomposition method. The big file will be divided into N sub domains and then 
		each thread is going to search PATTERN in one area. All threads exit when 
		finished searching.

		Such as, 

		For sequential grep one process will scan the whole field from 0 to SIZE. 
		file:     |---------------------------------------------------------|
        size:     0                                                        SIZE   
		Process:  |-->   

        For parallel grep multiple threads can scan a part with SIZE/N characters.
		Suppose we have 4 threads
		file:     |---------------------------------------------------------|
        size:     0                                                        SIZE    

        subdomain:|------------|---------------|---------------|-------------|
                  |-->         |-->            |-->            |-->   
        Thread :  0            1               2               3


        2) Coarse Parallel for recursive searching directories
		When grepping directories recursively, there are many files to deal with.
		Thus, it is far away from efficiency to create and destroy threads frequently for each file.
		Instead of domain decomposition is excluded, we maintain a thread pool and let each thread 
		retrieving file from free task list. Therefore, many files will be addressed in the same time
		by different threads. So, it is called "Coarse Parallel".
		Finally, when free list is empty as well as all threads finish the thread pool is destroyed.

		Such as,
        
		The main thread will add the new file into to Tail while each thread gets task from the Head.
        
		free list for files:
		
	     	          File1 --> File2 --> File3 --> ........ --> FileN  
                        ^                                          ^
                        |                                          |
                       Head                                       Tail

		Main thread add and work thread retrieve:
        Suppose we have 2 work threads.

	                File1      File2      File3 --> File4 --> ........ --> FileN --> FileN+1    
                      ^          ^          ^                                            ^
                      |          |          |                                            |
                    Thread1    Thread2     Head                                         Tail
             
    PERFORMANCES

        _____________________________________________________________________________
       | Dir Size (MB)   |   standard grep  |  2 threads  | 4 threads  |  8 threads  |
       |_________________|__________________|_____________|____________|_____________|
       |    50           |     19.16        |    9.80     |   5.30     |   4.00      |
	   |_________________|__________________|_____________|____________|_____________|
       |   Speedup       |                  |    1.96     |   3.62     |   4.79      |
       |_________________|__________________|_____________|____________|_____________| 

	   Note: One reason to limit the performance for current parallel program is that the sequential algorithm
	   is more slower than stardard grep. So that we can hardly get EXPECTED speedup even there is 
	   little dependence between each threads.


	ADVANTAGES
	   1) High Performance and Easy Programming
	      Reduce the additional costs such as communication time;
		  Share memory.

	   2) Scalibility and Portability
	      Benifit from more threads,
		  Suitable for searching big files and huge directories,
		  Easy to implement in different ports.


   DISADVANTAGES
       1) Usibility 
	      Just supports "-r" and is NOT flexible.
	
       2) NOT in order for output
	      Since each thread prints out the results seperately, the matched line will be mixed.

	   3) NOT efficient for single small file
	      The sequential algorithm is slower than standard grep.

	   4) Bugs
	      Many potential bugs :(

	TODO
       1) Support more options and improve usibility

	   2) Improve method reading IO
		  Change from stardard IO to system IO,
		  Support mmap().

	   3) Adjust the number of work threads automatically
	      Based on different type of CPU and machine loading.

	   4) Improve parallel algorithms
	   
	   5) Remove handcodes


 */



/****************************************************************************
 *				INCLUDE 
 *
 ****************************************************************************/

#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>                      /* For open()                       */
#include <unistd.h>                     /* For close(), lseek()             */
#include <sys/types.h>                  /* For opendir(), readdir(), closedir()   */
#include <dirent.h>         
#include <sys/stat.h>                   /* For stat()/lstat() structure     */
#include <pthread.h>                    /* For pthread_ functions           */ 
#include <string.h>                     /* For strstr()                     */
#include <ftw.h>                        /* For ftw()/nftw()                 */

#define KB             1024             /* 1K                               */
#define MB             (1024*1024)      /* 1M                               */
#define LINEBUF        (4*KB) 
#define THREADSNUM     8 
#define threshold      2 
#define MAXFILES       4096 


/****************************************************************************
 *                            CONSTANTS                                     *
 *                                                                          *
 ****************************************************************************/
/* Save the PATTERN */
const char *targetString_G   = NULL;

/****************************************************************************
 *                           STATIC VARIABLES                               *
 *                                                                          *
 ****************************************************************************/
static int grepDirRec         = 0;
static int useOption          = 0;
static int indexFile          = 0;
static int finishedGrepSubDir = 0;

/****************************************************************************
 *			     PTHREAD DECLARATION			                            *
 * 									                                        *
 ****************************************************************************/
pthread_t workThread[THREADSNUM];
pthread_t workThreadPool[THREADSNUM];
static pthread_mutex_t  workThreadPoolMux               = PTHREAD_MUTEX_INITIALIZER;
//static pthread_cond_t   workThreadPoolCond              = PTHREAD_COND_INITIALIZER; 

/****************************************************************************
 *			     STRUCTURE DECLARATION			                            *
 * 									                                        *
 ****************************************************************************/
struct task {
    char *fname;
    int   outputPath;
    long  start;
    long  end;
};

struct tasklist {
    struct task      task;
    struct tasklist *next;
};

struct tasklist *plHead=NULL, *plTail=NULL;

/****************************************************************************
 *				GLOBAL FUNCTIONS			                                *
 * 									                                        *
 ****************************************************************************/


/****************************************************************************
 * function    : help
 * description : print usage
 * argument(s) : 
 * return      : NULL
 ****************************************************************************/
void
help() 
{
    printf ("Usage : grep [option] PATTERN [FILE|DIRECTORY] \n");
}


/****************************************************************************
 * function    : parseArg 
 * description : split the arguments from commond line.
 * argument(s) : 
 * return      : 
 ****************************************************************************/
void 
parseArg(int num, char *string[])
{
    //TODO: Does not support the options such as -r, -i. Need enhance.
    if ( num < 3) {
        printf("Error: Incorrect arguments!\n");
        help();
        exit (0);
    }

    //TODO: Hardcode now, will enhance in the future for usability. 
    if (!strcmp(string[1],"-r")) {
        useOption          = 1;
        grepDirRec         = 1;
        indexFile          = 3;
        targetString_G     = string[2];
    } else {
        useOption          = 0;
        grepDirRec         = 0;
        // The search destination is from the argv[2] since the argv[0] and argv[1] are program name and target string.
        indexFile          = 2;
        targetString_G     = string[1];
    }
}

/****************************************************************************
 * function    : grepFile
 * description : search the PATTERN in the specified file and print out the results.
 * argument(s) : a structure with four elements.
 *               fname ,  file name
 *               start ,  the location to begin search. For sequential algorithm, 
 *                        start point is zero.
 *               end   ,  the location to end search. For sequential algorithm,
 *                        this is the size of the file.
 *               outputPath  ,  1 (print the path) or 0 ( don't print the path)         
 * return      : NULL 
 ****************************************************************************/
void* 
grepFile(void *arg) {

    struct task *file = arg;
    FILE *fp_status;
    char  buf[LINEBUF];
    long  ret = 0;
    long  totalSize = file->end - file->start;
    long  leftSize  = totalSize;

    if(!(fp_status = fopen(file->fname, "r"))) {
	    printf("Error: File open failed : %s\n", file->fname);
        return NULL;
    }
    
    // Starting from the specified point.
    if (fseek(fp_status, file->start, SEEK_SET) != 0) {
        fclose(fp_status);
        return NULL;
    }

    while (leftSize > 0 && fgets(buf, LINEBUF, fp_status)) {
        if (strstr(buf, targetString_G) != NULL) {
            if ( file->outputPath == 0 ) {
                printf("%s", buf);
            } else {
                printf("%s:%s", file->fname, buf);
            }
        } 
        ret = strlen(buf);
        leftSize -= ret;
    }
    fclose(fp_status);

	return NULL;

}


/****************************************************************************
 * function    : workThreadPoolFun
 * description : The work thread from thread pool. 
 *               Retrieve the file from free list and grep.Exit when no any available task in the list.
 * argument(s) : Needn't
 * return      : NULL
 ****************************************************************************/
void*
workThreadPoolFun(void *arg)
{
    struct tasklist *plTmp = NULL;

    while (1) {
        pthread_mutex_lock(&workThreadPoolMux);
		// TODO: Awake by a signal from main thread to avoid "while" loop 
		//       in order to save the CPU resources.
        //pthread_cond_wait(&workThreadPoolCond, &workThreadPoolMux);
        if (plHead != NULL && plHead->next != NULL) {
           // Get task from the tail of the list.
           if (plHead->next == plTail) {
                plTmp        = plTail;
                plTail       = plHead;
                plTail->next = NULL;
            } else {
                plTmp        = plHead->next;
                plHead->next = plTmp->next;
                plTmp->next  = NULL;
            }
            pthread_mutex_unlock(&workThreadPoolMux);
            
            grepFile((void *)&plTmp->task);
        
			// free memory from malloc/strdup by addFilesIntoFreeList
            if (plTmp->task.fname != NULL) {
                free(plTmp->task.fname);
                plTmp->task.fname = NULL;
            }
            
            if (plTmp != NULL) {
                free(plTmp);
                plTmp = NULL;
            }
        } else {
            // All tasks are finished so exit, otherwise continue wait the new task
            // added into list.
            if (finishedGrepSubDir == 1) {
                pthread_mutex_unlock(&workThreadPoolMux);
				// Memory leak caused by pthread_exit in some environment.
				// This is a pthread bug.
                pthread_exit(NULL);
            }
            pthread_mutex_unlock(&workThreadPoolMux);
        }
    }
}


/****************************************************************************
 * function    : initThreadPool
 * description : create THREADSNUM threads to work later.
 * argument(s) : 
 * return      : return number of available threads.
 ****************************************************************************/
int
initThreadPool() 
{
    // create threads pool
    int i     = 0;
    int error = 0;
    int num   = 0;

    for (i = 0; i < THREADSNUM; i++) {
        error = pthread_create(&workThreadPool[i], NULL, workThreadPoolFun, NULL);
        if (error != 0) {
            break;
        } 
        ++num;
    }

    return num;
}


/****************************************************************************
 * function    : joinThreadPool
 * description : wait all threads finish.
 * argument(s) : 
 * return      : 
 ****************************************************************************/
void 
joinThreadPool() 
{
    int i = 0;

    // wait working thread to join
    for (i = 0; i < THREADSNUM; i++) {
        pthread_join(workThreadPool[i],NULL);
    }
    
    return;
}


/****************************************************************************
 * function    : addFilesIntoFreeList 
 * description : add the file into the tail of free list.
 * argument(s) : 
 * return      : 0 (continue) or other (break from nftw)
 ****************************************************************************/
static int
addFilesIntoFreeList(const char *fpath, const struct stat *sb,
             int tflag, struct FTW *ftwbuf)
{

    struct tasklist *plTmp = NULL;

    if (tflag == FTW_F) {

       // Add a file in tail of the task list. 
       plTmp = (struct tasklist *) malloc (sizeof(struct tasklist));
       if (plTmp == NULL) {
           // No enough memory to save file info so that the program will exit
           // by returning a non-zero number.
           return 1;
       }
       // Save file info. strdup will allocate additional memory so don't forget free it.
       plTmp->task.fname      = strdup(fpath);
       plTmp->task.start      = 0;
       plTmp->task.end        = sb->st_size;
       plTmp->task.outputPath = 1;
       plTmp->next            = NULL;

       // When tail and head point to the same node, will have conflict thus using lock protect.
       pthread_mutex_lock(&workThreadPoolMux);
       plTail->next = plTmp;
       plTail       = plTail->next;

       // TODO: Awake sleeped thread but the signal will lose if all thread is working now.
	   //       Need a new approach.
       //pthread_cond_signal(&workThreadPoolCond);
       pthread_mutex_unlock(&workThreadPoolMux);

    }

    // return 0 to continue.
    return 0;

}


/****************************************************************************
 * function    : testList 
 * description : just for testing.
 * argument(s) : 
 * return      : 
 ****************************************************************************/
void 
testList(struct tasklist * head) {
    
    struct tasklist *plTmp = NULL;
    
    while (head != plTail && head->next != NULL) {
        plTmp = head->next;
        head->next = plTmp->next;
        plTmp->next= NULL;
       // printf("Patric name = %s, size = %d\n", plTmp->task.fname, plTmp->task.end - plTmp->task.start);
        if (plTmp->task.fname != NULL) {
            free(plTmp->task.fname);
            plTmp->task.fname = NULL;
        }
        if (plTmp != NULL) {
            free(plTmp);
            plTmp = NULL;
        }
    }
}

/****************************************************************************
 * function    : grepDirParallel 
 * description : search the directory recursively
 *               create and join the thread pool
 * argument(s) : 
 * return      : 
 ****************************************************************************/
void 
grepDirParallel(const char *path) {

    int    flag = 0;

    pthread_mutex_lock(&workThreadPoolMux);
    // The head node used to point to files that need to be searched.
    // Don't save actual file info into head node and just use for a pointer.
    plHead =  (struct tasklist *) malloc (sizeof(struct tasklist)); 
    if (plHead == NULL) {
        return;
    }
    plHead->next = NULL;
    plTail       = plHead;

    // Don't go into the linked dir.
    flag |= FTW_PHYS;

    // Tell work threads that new tasks will be added into list, keep working. 
    finishedGrepSubDir = 0;
    pthread_mutex_unlock(&workThreadPoolMux);

    initThreadPool();

    // Using nftw() recursive search all files.
    nftw(path, addFilesIntoFreeList, MAXFILES, flag);

    // Tell work threads that they could exit when finished current task. 
    pthread_mutex_lock(&workThreadPoolMux);
    finishedGrepSubDir = 1;
    pthread_mutex_unlock(&workThreadPoolMux);

	// Intenal testing. Exclusive with thread pool.
    //testList(plHead);

    joinThreadPool();

    if (plHead != NULL) {
        free (plHead);
        plHead = NULL;
    }

    return;
}


/****************************************************************************
 * function    : grepFileParallel 
 * description : divide a big file into several small parts.
 * argument(s) : 
 * return      : 
 ****************************************************************************/
void 
grepFileParallel(const char *file, long size, int threadNum) {

    FILE*  fp     = NULL;
    int    i      = 0;
    int    posAdd = 0;
    long   blockSize =  size / threadNum;
    struct  task arg[threadNum];
    
    if ((fp = fopen(file,"r")) == NULL) {
		printf("Error: Could not open the file! \n");
        return;
    }

    for (i = 0; i < threadNum - 1; i++) {
        // Basic size of each block for threads.
        arg[i].fname       = (char *)file;
        arg[i].start       = i       * blockSize + posAdd;
        arg[i].end         = (i + 1) * blockSize - 1 ;
        arg[i].outputPath  = 0;
        
        // Adjust the size to the next '\n', thus the file could be divided by line.
        fseek(fp, arg[i].end, SEEK_SET);
        for (posAdd = 0; fgetc(fp) != '\n'; posAdd++ );
        
        arg[i].end += posAdd ;

        // Start thread to grep sub-domain.
        pthread_create(&workThread[i], NULL, grepFile, (void *)&arg[i]); 
    }

    // The last domain is an irregular block compared with former blocks.
    arg[threadNum - 1].fname      = (char *) file;
    arg[threadNum - 1].start      = (threadNum - 1) * blockSize + posAdd;
    arg[threadNum - 1].end        = size - 1;
    arg[threadNum - 1].outputPath = 0;
    pthread_create(&workThread[i], NULL, grepFile, (void *)&arg[i]); 
    
    // The file descriptor is only used in main thread. 
    // Thus, close it without influence for other threads.
    fclose(fp);


    for (i = 0; i < threadNum; i++) {
        pthread_join(workThread[i],NULL);
    }
    
}
    

/****************************************************************************
 * function    : main 
 * description : 
 *               grep PATTERN FILE
 *               grep PATTERN DIR
 * argument(s) : 
 * return      : 
 ****************************************************************************/
int 
main(int argc, char *argv[]) {

    struct stat info;

    parseArg(argc,argv);

    while (indexFile < argc) {
        if (lstat(argv[indexFile], &info) == -1) {
            printf("Error: Could not open the specified file or directory.\n");
        }

        // The marco S_ISDIR is defined in stat.h
        if (S_ISDIR(info.st_mode)) {
            // Recursive search if "-r" option is used. 
            if (grepDirRec == 1) {
                grepDirParallel(argv[indexFile]);
            }
        } else {
            // For small files don't bother PARALLEL algrithm. 
            if ( info.st_size > threshold * MB ) { 
                grepFileParallel(argv[indexFile], info.st_size, THREADSNUM);
            } else {
                struct task fileInfo;
                fileInfo.fname = argv[indexFile];
                fileInfo.start = 0;
                fileInfo.end   = info.st_size;
				// Print out the file path when search more than one file.
				if (indexFile > 3) {
                    fileInfo.outputPath = 1;
				} else {
                    fileInfo.outputPath = 0;
				}
                grepFile((void *)&fileInfo);
            }
        }
        // Deal with the next one.
        ++indexFile;
    }

    return 0;
}


