#include <cassert>
#include <cstring>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <sys/types.h>
#include <sys/stat.h>

#include <thread>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <vector>
#include <math.h>
#include <unistd.h>
#include "FIFOreqchannel.h"
#include "MQreqchannel.h"
#include "SHMreqchannel.h"


int buffercapacity = MAX_MESSAGE;
char* buffer = NULL; // buffer used by the server, allocated in the main


int nchannels = 0;
pthread_mutex_t newchannel_lock;
void handle_process_loop(RequestChannel *_channel);
char ival;
vector<string> all_data [NUM_PERSONS];
string ipcmethod = "f";

void process_newchannel_request (RequestChannel *_channel){
	nchannels++;
	string new_channel_name = "data" + to_string(nchannels) + "_";
	char buf [30];
	strcpy (buf, new_channel_name.c_str());
	_channel->cwrite(buf, new_channel_name.size()+1);

	RequestChannel *data_channel;
	if(ipcmethod == "f") 
		data_channel = new FIFORequestChannel (new_channel_name, RequestChannel::SERVER_SIDE);
	else if(ipcmethod == "q")
		data_channel = new MQRequestChannel (new_channel_name, RequestChannel::SERVER_SIDE);
	else if(ipcmethod == "s")
		data_channel = new SHMRequestChannel (new_channel_name, RequestChannel::SERVER_SIDE, buffercapacity);
	thread thread_for_client (handle_process_loop, data_channel);
	thread_for_client.detach();
}

void populate_file_data (int person){
	//cout << "populating for person " << person << endl;
	string filename = "BIMDC/" + to_string(person) + ".csv";
	char line[100];
	ifstream ifs (filename.c_str());
	if (ifs.fail()){
		EXITONERROR ("Data file: " + filename + " does not exist in the BIMDC/ directory");
	}
	int count = 0;
	while (!ifs.eof()){
		line[0] = 0;
		ifs.getline(line, 100);
		if (ifs.eof())
			break;
		double seconds = stod (split (string(line), ',')[0]);
		if (line [0])
			all_data [person-1].push_back(string(line));
	}
}

double get_data_from_memory (int person, double seconds, int ecgno){
	int index = (int)round (seconds / 0.004);
	string line = all_data [person-1][index]; 
	vector<string> parts = split (line, ',');
	double sec = stod(parts [0]);
	double ecg1 = stod (parts [1]);
	double ecg2 = stod (parts [2]); 
	if (ecgno == 1)
		return ecg1;
	else
		return ecg2;
}

void process_file_request (RequestChannel* rc, char* request){
	
	filemsg f = *(filemsg *) request;
	string filename = request + sizeof (filemsg);
	filename = "BIMDC/" + filename; // adding the path prefix to the requested file name
	//cout << "Server received request for file " << filename << endl;

	if (f.offset == 0 && f.length == 0){ // means that the client is asking for file size
		__int64_t fs = get_file_size (filename);
		rc->cwrite ((char *)&fs, sizeof (__int64_t));
		return;
	}

	/* request buffer can be used for response buffer, because everything necessary have
	been copied over to filemsg f and filename*/
	char* response = request; 

	// make sure that client is not requesting too big a chunk
	if (f.length > buffercapacity){
		cerr << "Client is requesting a chunk bigger than server's capacity" << endl;
		cerr << "Returning nothing (i.e., 0 bytes) in response" << endl;
		rc->cwrite (response, 0);
	}
	FILE* fp = fopen (filename.c_str(), "rb");
	if (!fp){
		cerr << "Server received request for file: " << filename << " which cannot be opened" << endl;
		rc->cwrite (buffer, 0);
		return;
	}
	fseek (fp, f.offset, SEEK_SET);
	int nbytes = fread (response, 1, f.length, fp);
	/* making sure that the client is asking for the right # of bytes,
	this is especially imp for the last chunk of a file when the 
	remaining lenght is < buffercap of the client*/
	assert (nbytes == f.length); 
	rc->cwrite (response, nbytes);
	fclose (fp);
}

void process_data_request (RequestChannel* rc, char* request){
	datamsg* d = (datamsg* ) request;
	double data = get_data_from_memory (d->person, d->seconds, d->ecgno);
	rc->cwrite((char *) &data, sizeof (double));
}

void process_unknown_request(RequestChannel *rc){
	char a = 0;
	rc->cwrite (&a, sizeof (a));
}


int process_request(RequestChannel *rc, char* _request)
{
	MESSAGE_TYPE m = *(MESSAGE_TYPE *) _request;
	if (m == DATA_MSG){
		usleep (rand () % 5000);
		process_data_request (rc, _request);
	}
	else if (m == FILE_MSG){
		process_file_request (rc, _request);
	}else if (m == NEWCHANNEL_MSG){
		process_newchannel_request(rc);
	}else{
		process_unknown_request(rc);
	}
}

void handle_process_loop(RequestChannel *channel){
	/* creating a buffer per client to process incoming requests
	and prepare a response */
	char* buffer = new char [buffercapacity];
	if (!buffer){
		EXITONERROR ("Cannot allocate memory for server buffer");
	}
	while (true){
		int nbytes = channel->cread (buffer, buffercapacity);
		if (nbytes < 0){
			cerr << "Client-side terminated abnormally" << endl;
			break;
		}else if (nbytes == 0){
			// could not read anything in current iteration
			continue;
		}
		MESSAGE_TYPE m = *(MESSAGE_TYPE *) buffer;
		if (m == QUIT_MSG){
			cout << "Client-side is done and exited" << endl;
			break;
			// note that QUIT_MSG does not get a reply from the server
		}
		process_request(channel, buffer);
	}
	delete buffer;
}

int main(int argc, char *argv[]){
	buffercapacity = MAX_MESSAGE;
	int opt;
	while ((opt = getopt(argc, argv, "m:i:")) != -1) {
		switch (opt) {
			case 'm':
				buffercapacity = atoi (optarg);
				cout << "buffer capacity is changed to " << buffercapacity << " bytes" << endl;
				break;
			case 'i':
				ipcmethod = optarg;
				cout << "IPC method is changed to " << ipcmethod << endl;
				break;
		}
	}
	srand(time_t(NULL));
	for (int i=0; i<NUM_PERSONS; i++){
		populate_file_data(i+1);
	}
	
	RequestChannel* control_channel = nullptr;
	if(ipcmethod == "f") 
		control_channel = new FIFORequestChannel ("control", RequestChannel::SERVER_SIDE);
	else if(ipcmethod == "q")
		control_channel = new MQRequestChannel ("control", RequestChannel::SERVER_SIDE);
	else if(ipcmethod == "s")
		control_channel = new SHMRequestChannel ("control", RequestChannel::SERVER_SIDE, buffercapacity);
	handle_process_loop (control_channel);
	cout << "Server terminated" << endl;
	delete control_channel;
}
