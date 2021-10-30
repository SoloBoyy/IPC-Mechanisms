#include <sys/wait.h>
#include "common.h"
#include "FIFOreqchannel.h"
#include "MQreqchannel.h"
#include "SHMreqchannel.h"
#include <chrono>
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>

using namespace std;
using namespace chrono;

int main(int argc, char *argv[])
{
    int opt, c;
    int p = 0, e = 1;
    double t = -1.0;
    int buffS = MAX_MESSAGE; // buffer size
    string filename = "";
    bool DP = false; // data point section
    bool NC = false; // new channel section
    bool RF = false; // requesting files section
    string ipc = "f";
    int num_chan = 1;

    while ((c = getopt(argc, argv, "p:t:e:m:f:c:i:")) != -1)
    {
        switch (c)
        {
        case 'p':
            DP = true;
            p = atoi(optarg);
            break;
        case 't':
            t = atof(optarg);
            break;
        case 'e':
            e = atoi(optarg);
            break;
        case 'm':
            buffS = atoi(optarg);
            break;
        case 'c':
            NC = true;
            num_chan = atoi(optarg);
            break;
        case 'f':
            RF = true;
            filename = optarg;
            break;
        case 'i':
            ipc = optarg;
            break;
        }
    }
    //starting clock
    auto timeStart = high_resolution_clock::now();

    // child server creation using fork() and execvp()
    if (fork() == 0) 
    {
        cout << "Using child process" << endl;
        char *args[] = {"./server", "-m", (char *)to_string(buffS).c_str(), "-i", (char*)ipc.c_str(), nullptr};
        execvp("./server", args);
        exit(0);
    }

    //establishing request channel
    RequestChannel *control_chan = NULL;
    if (ipc == "f")
        control_chan = new FIFORequestChannel("control", FIFORequestChannel::CLIENT_SIDE);
    else if (ipc == "q")
        control_chan = new MQRequestChannel("control", FIFORequestChannel::CLIENT_SIDE);
    else if (ipc == "s")
        control_chan = new SHMRequestChannel("control", FIFORequestChannel::CLIENT_SIDE, buffS);

    cout << "Client/server connection established" << endl;
    RequestChannel *chan = control_chan;

    //loop through number of channels entered via -c flag
    for (int x = 0; x < num_chan; x++)
    {
        //creating new channel based on channel (-i) input
        if (NC)
        {
            cout << "Checking for new channel" << endl;
            MESSAGE_TYPE m = NEWCHANNEL_MSG;
            control_chan->cwrite(&m, sizeof(m));
            char newchanname[100];
            control_chan->cread(newchanname, sizeof(newchanname));
            if (ipc == "f")
                chan = new FIFORequestChannel(newchanname, RequestChannel::CLIENT_SIDE);
            else if (ipc == "q")
                chan = new MQRequestChannel(newchanname, RequestChannel::CLIENT_SIDE);
            else if (ipc == "s")
                chan = new SHMRequestChannel(newchanname, RequestChannel::CLIENT_SIDE, buffS);
            cout << "Channel " << newchanname << " is created" << endl;
            cout << "Now using channel " << newchanname << endl;
        }

        if(DP)
        {
            datamsg x(p, t, e);
            //3 arrays that will store 1000 values 
            double timeT [1000];
            double ecg1T [1000];
            double ecg2T [1000];
            //timeCounter will store the new times as the loop goes over
            double timeCounter = 0;
            //initalizing i for loop
            int i = 0;

            //requesting data points
            
            if(t < 0)
            {
                ofstream myfile("./received/x1.csv");
                while (i < 1000)
                { 
                    datamsg temp1 (p, timeCounter, 1);
                    chan->cwrite (&temp1, sizeof (datamsg)); // question
                    double reply1;
                    int nbytes = chan->cread (&reply1, sizeof(double)); //answer
                    timeT [i] = timeCounter; //storing current time into timeT array
                    ecg1T [i] = reply1; //storing value of ecg1 at the current time into the ecg1 array
                    datamsg temp2 (p, timeCounter, 2);
                    chan->cwrite (&temp2, sizeof (datamsg)); // question
                    double reply2;
                    nbytes = chan->cread (&reply2, sizeof(double)); //answer
                    ecg2T [i] = reply2;

                    timeCounter += 0.004; 
                    i = i + 1;  
                }
                i = 0;  
                
                while (i < 1000)
                {
                    myfile << timeT[i] << "," << ecg1T[i] << "," << ecg2T[i] << "\n"; //stores values from array into csv
                    if (e == 1)
                    {
                        cout << ecg1T[i] << endl;
                    }
                    if (e == 2)
                    {
                        cout << ecg2T[i] << endl;
                    } 
                    
                    i = i + 1;
                }
                myfile.close();
            }
            else if (t > 0) 
            {
                chan->cwrite (&x, sizeof (datamsg)); // question
                double reply;
                int nbytes = chan->cread (&reply, sizeof(double)); //answer
                cout << "For person " << p <<", at time " << t << ", the value of ecg "<< e <<" is " << reply << endl;
            }
        
        }
        //file transfer
        else if (RF)
        {
            filemsg f(0, 0); 
            char *buf = new char[sizeof(filemsg) + filename.size() + 1];
            memcpy(buf, &f, sizeof(filemsg));
            strcpy(buf + sizeof(filemsg), filename.c_str());
            chan->cwrite(buf, sizeof(filemsg) + filename.size() + 1);
            __int64_t fileLen;
            chan->cread(&fileLen, sizeof(__int64_t));
            
            if(x == num_chan -1)
            {
                cout << "File Size: " << fileLen/num_chan + fileLen%num_chan << endl;
            }
            else
            {
                cout << "File Size: " << fileLen/num_chan << endl;
            }
            
            filemsg* file = (filemsg*) buf;
            __int64_t rem = fileLen/ num_chan;
            string openFile = string("received/") + filename;
            FILE* outfile;

            if(x == 0)
            {
                outfile = fopen (openFile.c_str(), "wb");
            }
            else
            {
                outfile = fopen (openFile.c_str(), ("wb", "a+"));
            }

            file->offset = rem*x;
            if(x == num_chan -1)
            {
                rem+= fileLen % num_chan;
            }


            int set = sizeof(filemsg) + filename.size() + 1;
           /* char *buff_1 = new char[set];
            memcpy(buff_1, file, sizeof(filemsg));
            strcpy(buff_1 + sizeof(filemsg), filename.c_str());
            chan->cwrite(buff_1, set);*/


            char *handler = new char[MAX_MESSAGE];
                    while (rem > 0)
                    {
                        file->length = (int) min (rem, (__int64_t) MAX_MESSAGE);
                        chan->cwrite (buf, set);
                        chan->cread (handler, MAX_MESSAGE);
                        fwrite (handler, 1, file->length, outfile);
                        rem -= file->length;
                        file->offset += file->length;
                                
                        /*int len = min((__int64_t)buffS, cycle);
                        ((filemsg*)buff_1)->length = len;
                        chan->cwrite(buff_1, set);
                        chan->cread(handler, len);
                        fwrite(handler, 1, len, ff);
                        ((filemsg*)buff_1)->offset+= len;
                        cycle-=len;*/
                        
                        // cout << fm->offset << endl;
                    }
                
            
            
            fclose(outfile);
            delete handler;
            delete buf;
        }
        MESSAGE_TYPE q = QUIT_MSG;
        chan->cwrite(&q, sizeof(MESSAGE_TYPE));
        delete chan;
    }

    // time process creation 
    auto timeEnd = high_resolution_clock::now();
    auto processDuration = duration_cast<microseconds>(timeEnd - timeStart).count();
    cout << "Time taken: " << (double)processDuration / 1000 << " ms." << endl;

    //closing channels
    MESSAGE_TYPE q = QUIT_MSG;
    if (chan != control_chan)
    { 
        control_chan->cwrite(&q, sizeof(MESSAGE_TYPE));
    }
    wait(0);
}

