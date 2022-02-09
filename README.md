# IPC Mechanisms

This is a C++ assignment that new and advanced IPC methods to enable client-server communication and compare their performance during the exchange of large volumes of data. 

The FIFORequestChannel class uses a mechanism called “namedpipes” or “FIFOs” to communicate between the two sides of the channel. However, FIFOs are only one of several different IPC mechanisms, each of which has its own particular uses that make them suited to particular applications. In this programming, we will use 2 “new” IPC mechanisms: Message Queues and Shared Memory, in addition to named pipes:, and implement useKernel Semaphores for synchronization.

This project was part of my operating systems class, and was recently moved to my public repo.

## Background

To complete this project I created 3 dfferent IPC-method base request channels. I began with an class named RequestChannel and used the 3 derived class:
* FIFORequestChannel
* MQReuestChannel
* SHMRequeust Channel

The definition of the base classRequestChannel is as follows. Its structure reflects some basic principles of Object-Oriented Programming (OOP). For instance, moving constructs common to all derived classes higher up in the hierarchy (e.g.,Side,Mode, and the base constructor)and leaving specific implementations to the lower/derived classes (e.g., the pure virtual functionscread and cwrite).
              class RequestChannel {
              public:
              typedef enum {SERVER_SIDE, CLIENT_SIDE} Side;
              typedef enum {READ_MODE, WRITE_MODE} Mode;
              /* CONSTRUCTOR/DESTRUCTOR */
              RequestChannel (const string _name, const Side _side);
              virtual ~RequestChannel() = 0;
              /* destruct operation should be derived class-specific */
              virtual int cread (void* msgbuf, int bufcapacity) = 0;
              /* Blocking read; returns the number of bytes read.
              If the read fails, it returns -1. */
              virtual int cwrite (void* msgbuf, int bufcapacity) = 0;
              /* Write the data to the channel. The function returns
              the number of characters written, or -1 when it fails */
              };
              
## Example Commands

* “f” for FIFO (default value)
* “q” for Message Queue
* “s” for Shared Memory

To request the first 1K ECG data points — of the format (time, ecg1, ecg2) — for the given person through each of those channels:
* ./client -c <# of new channels> -p <person no> -e <ecg no> -i [f|q|s]
  
  And, the following command will get the specified file using the given number of
channels of type given by “-i”:
* ./client -c <# of new channels> -f <filename> -i [f|q|s] -m <buffer capacity>
