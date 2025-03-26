# kafka

kafka is a uniquely designed kernel designed to be as small as possible
with having multi-architecture support. kafka is developed as 
an experiment to find the *right* kernel design architecture as I am not
satisfied with how each operating system works under the hood.

The codebase tries to be as maintainable and readable as possible, with
an extensive amount of comments explaining things in many sub-components; just so I cannot get confused.

## Motivation

While many kernel architectures like monolithic and microkernel exist, I personally believe they 
have pros and cons. kafka is an opinionated kernel architecture which is called, "stereo kernel." 
It aims to be more than a microkernel yet not as overwhelming as a monolithic design. This makes 
it easier to comprehend whatever is going on internally as well as adding more architectures 
or simply just to introduce new features.

Many many subsystems are notoriously difficult to test thoroughly due to complexity and edge cases
on monolithic kernels while microkernels sometimes introduce too many boundaries. There are also
some sort of "development barriers" for OSDev as it has a very steep learning curve. 

"Stereo" kernels aim to solve this by reducing the insanity of these pain points that can come and
bite us back later, just like how RISC CPU architecture manages to solves CISC's issues regarding
similar problems.

## Status

There isn't much about kafka yet as it is one of my newly development project and definitely 
will take a significant amount of effort on my end to complete the kernel. Therefore, 
please do not expect the project to be fully completed or bug-free.
