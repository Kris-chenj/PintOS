<center><font size=5>+-----------------------------------+</font></center>
<center><font size=5>|-------------- CS-130 --------------|</font></center>
<center><font size=5>|--PROJECT 2: USER PROGRAMS--|</font></center>
<center><font size=5>|-------DESIGN-DOCUMENT-------|</font></center>
<center><font size=5>+-----------------------------------+</font></center>

### GROUP

> Fill in the names and email addresses of your group members.

Yifan Zhu <zhuyf1@shanghaitech.edu.cn>  
Zongze Li <lizz@shanghaitech.edu.cn>


### PRELIMINARIES

> If you have any preliminary comments on your submission, notes for the
> TAs, or extra credit, please give them here.

> Please cite any offline or online sources you consulted while
> preparing your submission, other than the Pintos documentation, course
> text, lecture notes, and course staff.
>
> * User and Kernel
>
>   https://www.cnblogs.com/FengZeng666/p/14219477.html
>
> *  Os Guide
>
>   https://static1.squarespace.com/static/5b18aa0955b02c1de94e4412/t/5b85fad2f950b7b16b7a2ed6/1535507195196/Pintos+Guide
>
> 


<center><font size=5>ARGUMENT PASSING</font></center>

### DATA STRUCTURES

> A1: Copy here the declaration of each new or changed `struct` or
> `struct` member, global or static variable, `typedef`, or
> enumeration.  Identify the purpose of each in 25 words or less.

thread.h:
```
struct thread
{
    /* Record the number when process end to print out */
    int exit_num;
    /* Record the parent of thread to release mutex latter */                       
    struct thread* parent;              
}
```
process.h:
```
struct f_info
{
    /* Record arguments in the pointer */
    void *file_name;
    /* Record whether success excuate load() */
    bool success;
    /* Mutex between parent and child */
    struct semaphore sema;
}
```
### ALGORITHMS

> A2: Briefly describe how you implemented argument parsing.  How do you arrange for the elements of argv[] to be in the right order? How do you avoid overflowing the stack page?

How you implemented argument parsing ?

First, pass the argument to process_execute(), and use strtok_r() split the given file_name into arguments. 
Then, create a thread with splited arguments, if the thread is correctly created, it starts executing start_process with file_name as the argument. 
Within start_process, it calls load() with file_name. Within load, it calls setup_stack() to set the stack for the process.
Finally return to start_process(), use strtok_r() split the given argument into several strings with space as delimiter, and each argument is stored in the array argv, and the number of arguments is stored in argc.

How do you arrange for the elements of argv[] to be in the right order?

Firstly, we scan argv array backwards, and decrement esp by the length of the given argument + 1, to correctly allocate space in the stack to copy the argument along with the string terminator. 
Then, we store esp in argv[] array to remember the starting position of each argument in stack. Afterwards, we copy the contents of the argv to the memory pointed by esp. 
Then word align, after all the contents have been pushed, we round the stack pointer down to be multiple of 4 for faster access.
Finally, push the addresses of the actual arguments stored in argv[] array onto the stack, with rightmost argument at the highest address in the stack.
After all the addresses have been pushed, argc is pushed onto the stack to indicate how many arguments are present. Finally, a fake return address (0x0) is pushed on the stack.

How do you avoid overflowing the stack page?

We just not check the esp pointer until it fails. We don't need to do that because we just let it fails, and we handle it in the page fault exception, which is exit(-1) the running thread whenever the address is invalid.

### RATIONALE

> A3: Why does Pintos implement strtok_r() but not strtok()?

strtok() uses an internal static pointer in a static buffer to indicate the position of next token. This means that when there is interrupt, and the thread is preempted, there is a possibility that the contents strtok() was referring to have been changed when the thread executing strtok() gets hold of the CPU again, making the execution incorrect.

strtok_r() is the reentrant version of strtok(). It stores all its information locally or through the arguments passed by the caller, thus it removes the possibility of data being changed by other threads. In Pintos, thread switching is very possible, and to ensure correctness of the code, we must use the reentrant version, strtok_r(). 

> A4: In Pintos, the kernel separates commands into a executable name and arguments. In Unix-like systems, the shell does this separation. Identify at least two advantages of the Unix approach.

Firstly, when shell does the separation, the time spent in kernel can be reduced. This will reduce system overhead, allowing more time for the user processes to do more useful work, instead of wasting precious CPU time in the kernel code.

Secondly, the user input can be checked before being passed to the kernel. This way, the shell can detect if there is an error in the command and inform accordingly, or check if the command length is too long for the kernel to handle and reject the command. It will prevent passing error-prone command to the kernel and possibly causing kernel error. This will prevent kernel from executing illegal commands, and the worst possible outcome is for the shell to fail, and the kernel will still be running and the whole system will continue running.


<center><font size=5>SYSTEM CALLS</font></center>

### DATA STRUCTURES

> B1: Copy here the declaration of each new or changed `struct` or
> `struct` member, global or static variable, `typedef`, or
> enumeration.  Identify the purpose of each in 25 words or less.

thread.h:
```
struct thread
  {
    ......

   /* the number of files */
   int file_num;
   /* the files the thread owns */ 
   struct list files;
   struct file * tempfile;
   
   /* the all kids of parent */
   struct kid * mykid; 
   struct list kids;

   ......
   }
```
Record the file data of thread, and the children thread data of thread.

```
struct kid
{
   bool death;
   bool already_wait;
   int tid;
   int exit_num;
   struct semaphore sema;
   struct list_elem elem;   
};
```
The structure about children threads, record the children threads information of thread.

```
struct ofile
{
    int fd;
    struct list_elem elem;
    struct file* file_pointer;
};
```
Record the information of files owned by the thread.

syscall.h:
```
struct lock file_lock;
```
A lock that prevents files from being simultaneously operated.


> B2: Describe how file descriptors are associated with open files.
> Are file descriptors unique within the entire OS or just within a
> single process?

File descriptor is like an index of a file. When a file is opened, the file descriptor is given to the opened file. In our implementation, the thread has a value to record the number of opened files which it owns, when the current thread open a new file, we let the file descriptor value of new file be the recorded number, which makes the file descriptor one-to-one mapping to the opened file in a thread. So the file descriptors are unique just within a single process.

### ALGORITHMS

> B3: Describe your code for reading and writing user data from the kernel.

read: check the buffer whether be valid first. If the fd be 0, put input into buffer with `input_getc()`. If the fd be 1, return -1 to eax. Otherwise, find the file, lock the file, call the `file_read()` return value to eax, release the lock.

write: check the buffer whether be valid first. If the fd be 0, return -1 to eax. If fd be 1, print the buffer context to the console with `putbuf()`. Otherwise, find the file, lock the file, call the `file_write()` return value to eax, release the lock.

> B4: Suppose a system call causes a full page (4,096 bytes) of data
> to be copied from user space into the kernel.  What is the least
> and the greatest possible number of inspections of the page table
> (e.g. calls to pagedir_get_page()) that might result?  What about
> for a system call that only copies 2 bytes of data?  Is there room
> for improvement in these numbers, and how much?

The least possible number of inspections of the page table is 1, that means if the first inspection get the page head, we don’t need to inspect any
more. The greatest possible number of inspections of the page table is 4096, if it is not contiguous, then we have to check every address, and if it is contiguous, we just need to check the head and tail.

For 2 bytes of data, same as above, the least possible number of inspections of the page table is 1, and the greatest possible number of inspections of the page table is 2.

We think there is no room to improve.

> B5: Briefly describe your implementation of the "wait" system call
> and how it interacts with process termination.

Firstly, we call the function `process_wait()`, which make the thread wait. In the funcion, we traverse all the children threads to find the target children thread through the given `child_tid`, then set the `already_wait` be 1, which means it is already in the waiting state, use `sema_down()` to block the parent process. When thread exit, it will call `sema_down()`, then the blocked process will run again.

> B6: Any access to user program memory at a user-specified address
> can fail due to a bad pointer value.  Such accesses must cause the
> process to be terminated.  System calls are fraught with such
> accesses, e.g. a "write" system call requires reading the system
> call number from the user stack, then each of the call's three
> arguments, then an arbitrary amount of user memory, and any of
> these can fail at any point.  This poses a design and
> error-handling problem: how do you best avoid obscuring the primary
> function of code in a morass of error-handling?  Furthermore, when
> an error is detected, how do you ensure that all temporarily
> allocated resources (locks, buffers, etc.) are freed?  In a few
> paragraphs, describe the strategy or strategies you adopted for
> managing these issues.  Give an example.

We will call a series of check functions to check: `check_pointer_validity()`, `check_pointer_validity_void()` and `check_pointer_validity_char()`. In these function, we will check the pointer whether be NULL or exceed legal address, and call the function `pagedir_get_page()` and `is_user_vaddr()`, they check whether the map from virtual memory to physical memory is valid and whether the pointer is stored in the valid virtual user memory. If an invalid condition is detected, then set the `exit_num` be -1, and call the `thread_exit()`. Then the `thread_exit()` will release the resource and end the thread. 

Example: If we call the function `sysread()`, we will check the `buffer` with `check_pointer_validity_void()`, and the `buffer` is checked be invalid. Then the `exit_num` will be set -1, run the `thread_exit()`. Release all relevant resources. In this way, we ensure that all temporarily allocated resources are freed.

### SYNCHRONIZATION

> B7: The "exec" system call returns -1 if loading the new executable
> fails, so it cannot return before the new executable has completed
> loading.  How does your code ensure this?  How is the load
> success/failure status passed back to the thread that calls "exec"?

We set up a struct named 'f_info' which has a element 'struct semaphore sema' record whether `load()` function excuate successfully
If success, we set true to bool and do next instruction
If fail, we set false to bool, and in `process_exec()` we directly return -1.

> B8: Consider parent process P with child process C.  How do you
> ensure proper synchronization and avoid race conditions when P
> calls wait(C) before C exits?  After C exits?  How do you ensure
> that all resources are freed in each case?  How about when P
> terminates without waiting, before C exits?  After C exits?  Are
> there any special cases?

If P wait C, P will call `sema_down()` and wait C exit, then it will call `sema_up()` (to the same semaphore), it's obvious no race condition.

Also we use `process_wait()` to achieve all resource are freed, we just freed all of them using files list and it's not be affected by the order of process P and C.


### RATIONALE

> B9: Why did you choose to implement access to user memory from the
> kernel in the way that you did?

We chose to check only that the pointer is below PHYS_BASE and deference the pointer using the get_user() method. In the Pintos manual, it is said that this method is faster than the method that verifies the validity of the user-provided pointer, so we chose to implement this way. This method did not seem too complicated, and since it is the faster option of the two, it seemed to be a better choice.


> B10: What advantages or disadvantages can you see to your design
> for file descriptors?

Advantages:
1) Thread-struct’s space is minimized
2) Kernel is aware of all the open files, which gains more flexibility to
   manipulate the opened files.

Disadvantages:
1) Consumes kernel space, user program may open lots of files to crash the
kernel.
2) The inherits of open files opened by a parent require extra effort to be
implement.

> B11: The default tid_t to pid_t mapping is the identity mapping.
> If you changed it, what advantages are there to your approach?

We don't need to distinguish them because there is no multithreaded process in the current system, which make it easier. If the process is multithreaded, the mapping between them need to be identity.

<center><font size=5>SURVEY QUESTIONS</font></center>

Answering these questions is optional, but it will help us improve the
course in future quarters.  Feel free to tell us anything you
want--these questions are just to spur your thoughts.  You may also
choose to respond anonymously in the course evaluations at the end of
the quarter.

> In your opinion, was this assignment, or any one of the three problems
> in it, too easy or too hard?  Did it take too long or too little time?

Too hard!!! It takes too long time!!!

> Did you find that working on a particular part of the assignment gave
> you greater insight into some aspect of OS design?

Yes.

> Is there some particular fact or hint we should give students in
> future quarters to help them solve the problems?  Conversely, did you
> find any of our guidance to be misleading?

Good.

> Do you have any suggestions for the TAs to more effectively assist
> students, either for future quarters or the remaining projects?

No.

> Any other comments?