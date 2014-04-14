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
		system. In this version, the multi-threads technology is used. The GPGPU 
		will be applied in next version.

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

