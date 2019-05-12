The `PARALLEL grep` is a demo to show how to apply parallel algorithm into `grep` under the Linux system. 

In this version, the multi-threads technology is used under the X86 architecture. 

Welcome to contribute to improve the code :)

**NAME**

     pgrep - print lines matching a pattern

**COMPILE**

     gcc ParallelGrep.c -o pgrep -lpthread
 
**SYNOPSIS**

     pgrep [OPTIONS] PATTERN [FILE...]
	
   
   Currently, only one option "-r" is supported with the below format.
   
     *pgrep -r PATTERN [FILE...]*     
   		  
**DESCRIPTION**

Now, two kind of parallel modes are employed for both fine and coarse granularity.
 
 1. Fine Parallel for a BIG file
 
We use N threads to grep a big file stimultanously based domain	decomposition method. The big file will be divided into N sub domains and then each thread is going to search PATTERN in one area. All threads exit when finished searching.

 - For sequential grep one process will scan the whole field from 0 to SIZE. 

     	file:     |--------------------------------------------------------|
        size:     0                                                      SIZE   

 - For parallel grep multiple threads can scan a part with SIZE/N characters.
    Suppose we have 4 threads
    	
        |------------|---------------|---------------|-------------|
        |-->         |-->            |-->            |-->          |
        Thread :     0               1               2             3


 2. Coarse Parallel for recursive searching directories
 
When grepping directories recursively, there are many files to deal with.Thus, it is far away from efficiency to create and destroy threads frequently for each file. Instead of domain decomposition is excluded, we maintain a thread pool and let each thread retrieving file from free task list. Therefore, many files will be addressed in the same time by different threads. So, it is called "Coarse Parallel". Finally, when free list is empty as well as all threads finish the thread pool is destroyed.
Such as, the main thread will add the new file into to Tail while each thread gets task from the Head.
        
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
             
**PERFORMANCE**

*We compare the performance between `grep 2.20` and parallel version. 
The latest grep is quick faster so the `parallel grep` dosesn't faster than standard grep again, see [this issue](https://github.com/PatricZhao/ParallelGrep/issues/3).*

        _____________________________________________________________________________
       | Dir Size (MB)   |   standard grep  |  2 threads  | 4 threads  |  8 threads  |
       |_________________|__________________|_____________|____________|_____________|
       |    50           |     19.16        |    9.80     |   5.30     |   4.00      |
       |_________________|__________________|_____________|____________|_____________|
       |   Speedup       |                  |    1.96     |   3.62     |   4.79      |
       |_________________|__________________|_____________|____________|_____________| 

 *Note: One reason to limit the performance for current parallel program is that the sequential algorithm is more slower than stardard grep. So that we can't get expected speedup even there is little dependence between each threads.*


 **EXAMPLES**
  
    $ wget https://dumps.wikimedia.org/enwiki/latest/enwiki-latest-abstract.xml.gz
    $ gunzip enwiki-latest-abstract.xml.gz
    
 NOTE: please run each command several times to avoid cold loading from the disk.
 
    $ time grep "deep learning" enwiki-latest-abstract.xml >& /dev/null

> real 0m3.094s

> user 0m2.089s

> sys 0m1.005s

    $ time ./pgrep "deep learning" enwiki-latest-abstract.xml > /dev/null

> real 0m1.173s

> user 0m7.814s

> sys 0m1.326s

 
 **ADVANTAGES**
 
 * High Performance and Easy Programming
 * Scalibility and Portability

 **DISADVANTAGES**
 
 * Usibility: Just supports "-r" and is NOT flexible.
 * NOT in order for output: since each thread prints out the results seperately, the matched line will be mixed.
 * NOT efficient for single small file: the sequential algorithm is slower than standard grep.
 * Bugs: may have potential bugs :( If you find one, email me.

 **TODO**
 
* Support more options and improve usibility
* Improve method reading IO
* Support mmap().
* Adjust the number of work threads automatically
* Based on different type of CPU and machine loading.
* Improve parallel algorithms
* Remove handcodes
