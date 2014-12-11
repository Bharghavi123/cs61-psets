README for CS 61 Problem Set 6
------------------------------
YOU MUST FILL OUT THIS FILE BEFORE SUBMITTING!

YOUR NAME: Jordan Canedy
YOUR HUID: 70871939

(Optional, for partner)
YOUR NAME: Yuan Jiang
YOUR HUID: 20864400

RACE CONDITIONS
---------------
Write a SHORT paragraph here explaining your strategy for avoiding
race conditions. No more than 400 words please.

In order to avoid race conditions, we first addressed the issue of global variables that would be accessed concurrently by multiple threads. These variables included our global counters such as "concurrent_threads" and "nconnections", global flags such as "loss" and "stop", and global structs such as "available_connections", our connections table. In order to sychronize we must provide mutual exclusion in critical sections. To prevent race conditions we used the mutex variable to lock/unlock threads when accessing (by either reading or writing) to these global variables. For instance, when inserting into and removeing connections from our global connection table object "available_connections", we used the function pthread_mutex_lock/unlock (Lines 305-315, 381-394). So, in this example, when a specific thread needs to insert into the the connection table it will obtain a lock on the mutex, and all other threads will block until the mutex is unlocked by the owning thread. Next, we addressed race conditions by using the synchronization variables convar when we wanted the program to enter a specific condition. For example, we wanted to pause the main thread with pthread_cond_wait (Lines 499-506) whenever one of the threads was in a loss period. Finally, we thought about the order in which variables were accessible to multiple threads were being accessed.



OTHER COLLABORATORS AND CITATIONS (if any):



KNOWN BUGS (if any):



NOTES FOR THE GRADER (if any):



EXTRA CREDIT ATTEMPTED (if any):
