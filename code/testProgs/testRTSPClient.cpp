/**********
This library is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 2.1 of the License, or (at your
option) any later version. (See <http://www.gnu.org/copyleft/lesser.html>.)

This library is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
more details.

You should have received a copy of the GNU Lesser General Public License
along with this library; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
**********/
// Copyright (c) 1996-2012, Live Networks, Inc.  All rights reserved
// A demo application, showing how to create and run a RTSP client (that can potentially receive multiple streams concurrently).
//
// NOTE: This code - although it builds a running application - is intended only to illustrate how to develop your own RTSP
// client application.  For a full-featured RTSP client application - with much more functionality, and many options - see
// "openRTSP": http://www.live555.com/openRTSP/

#include "liveMedia.hh"
#include "BasicUsageEnvironment.hh"
#include <string.h>
#include <stdlib.h>

#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>


/*FIX*/
extern "C" {
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <termios.h>
	typedef enum 
	{
		PIC_QCIF_CLT = 0,
		PIC_CIF__CLT,	  
		PIC_2CIF_CLT,	 
		PIC_HD1_CLT,	 
		PIC_D1_CLT, 	 
		PIC_960H_CLT,
		PIC_1280H_CLT,
		PIC_1440H_CLT,
		
		PIC_QVGA_CLT,	 /* 320 * 240 */
		PIC_VGA_CLT,	 /* 640 * 480 */	
		PIC_XGA_CLT,	 /* 1024 * 768 */	
		PIC_SXGA_CLT,	 /* 1400 * 1050 */	  
		PIC_UXGA_CLT,	 /* 1600 * 1200 */	  
		PIC_QXGA_CLT,	 /* 2048 * 1536 */
	
		PIC_WVGA_CLT,	 /* 854 * 480 */
		PIC_WSXGA_CLT,	 /* 1680 * 1050 */		
		PIC_WUXGA_CLT,	 /* 1920 * 1200 */
		PIC_WQXGA_CLT,	 /* 2560 * 1600 */
		
		PIC_HD720_CLT,	 /* 1280 * 720 */
		PIC_HD1080_CLT,  /* 1920 * 1080 */
		PIC_qHD_CLT,	 /*960 * 540*/
	
		PIC_UHD4K_CLT,	 /* 3840*2160 */
		
		PIC_BUTT_CLT
	}EPicSizeCLT;
		
	typedef enum {
		FALSE_CLT = 0,
		TRUE_CLT  = 1, 
	}EBOOlCLT;
		
	typedef struct
	{
		unsigned char*	pu8Addr;			/* stream address */
		unsigned int	u32Len; 			/* stream len */
		long long	u64PTS; 			/* time stamp */
		int 	 bEndOfFrame;		/* is the end of a frame */
		int 	 bEndOfStream;		/* is the end of all stream */
	}SVdecStreamCLT;
	
	typedef struct tagSStream
	{	
		char			url[256];	//流的url 
		int 			srcWidth;	//视频宽 		
		int 			srcHeight;	//视频高
		short			destX;		//显示矩形左上角x坐标
		short			destY;		//显示矩形左上角y坐标
		short			destWidth;	//显示矩形宽	
		short			destHeight; //显示矩高
		EPicSizeCLT 	pic_clt;
		int 			payloadtype;
	}SStream;	
	
	int  MpiVdecSendStreamCLT(int VdChn, const SVdecStreamCLT	*pstStream, int s32MilliSec);
	int  VoInit(void);
	void VoExit(void );
	void VoSendStream(void );
	void SendParamToVO(const SStream *pStream);
	void SetLocation(void);



};

static unsigned rtspClientCount = 0; // Counts how many streams (i.e., "RTSPClient"s) are currently in use.
RTSPClient* rtspClient[16]; 

	
/*END FIX*/

// Forward function definitions:

// RTSP 'response handlers':
void continueAfterDESCRIBE(RTSPClient* rtspClient, int resultCode, char* resultString);
void continueAfterSETUP(RTSPClient* rtspClient, int resultCode, char* resultString);
void continueAfterPLAY(RTSPClient* rtspClient, int resultCode, char* resultString);

// Other event handler functions:
void subsessionAfterPlaying(void* clientData); // called when a stream's subsession (e.g., audio or video substream) ends
void subsessionByeHandler(void* clientData); // called when a RTCP "BYE" is received for a subsession
void streamTimerHandler(void* clientData);
  // called at the end of a stream's expected duration (if the stream has not already signaled its end using a RTCP "BYE")

// The main streaming routine (for each "rtsp://" URL):
void openURL(UsageEnvironment& env,  char const* rtspURL);

// Used to iterate through each stream's 'subsessions', setting up each one:
void setupNextSubsession(RTSPClient* rtspClient);

// Used to shut down and close a stream (including its "RTSPClient" object):
void shutdownStream(RTSPClient* rtspClient, int exitCode = 1);

// A function that outputs a string that identifies each stream (for debugging output).  Modify this if you wish:
UsageEnvironment& operator<<(UsageEnvironment& env, const RTSPClient& rtspClient) {
  return env << "[URL:\"" << rtspClient.url() << "\"]: ";
}

// A function that outputs a string that identifies each subsession (for debugging output).  Modify this if you wish:
UsageEnvironment& operator<<(UsageEnvironment& env, const MediaSubsession& subsession) {
  return env << subsession.mediumName() << "/" << subsession.codecName();
}

void usage(UsageEnvironment& env, char const* progName) {
  env << "Usage: " << progName << " <rtsp-url-1> ... <rtsp-url-N>\n";
  env << "\t(where each <rtsp-url-i> is a \"rtsp://\" URL)\n";
}

char eventLoopWatchVariable = 0;
char eventLoopWatchVariable1 = 0;
char eventLoopWatchVariable3 = 0;



SStream stream[16];

void *thr_fn(void *arg){
	FILE *fp1; /* 源文件和目标文件 */
	time_t lastTime=0;

	while(1){
		
	    char recvbuf[2500];
		int recvBufLen=0;
		int streamNum=-1;

		char* pFind;
		int findLength=0;
		
		struct stat buf;
		int result;

		result =stat( "a.config", &buf );
		if(buf.st_mtime==lastTime)
		{
			//printf("there are no change");
			

		}
		else{
			printf("*thr_fn\r\n");
			memset(stream,0,sizeof(stream));

		    if((fp1 = fopen("a.config", "r")) == NULL){ /* 打开源文件 */
		        perror("fail to open");
		        exit(1);
		    }

		    /* 开始复制文件，每次读写一个字符 */
			int num=fgetc(fp1);
			while( num != EOF){
				 *(recvbuf+recvBufLen)=(char)num;
				  recvBufLen++; 
				 
				   num=fgetc(fp1);
			
			}

			pFind=recvbuf;
			for(int i=0;i<16;i++)
			{	char *st;
				
				st = strstr(pFind, "streamNum=");
				if(	NULL==st)
				{
					printf("find error at the %d stream ",i+1);
					break;
				
				}
				pFind=st+10;
				
				if('1'==*pFind)
				{
						if(*(pFind+1)=='\r'||*(pFind+1)=='\n')
					{
						streamNum=*(pFind)-'1';
						
					}
					else
					{
						streamNum=*(pFind+1)-'1'+10;
							//printf("streamNUM>=9");
					}
				}
				else
					streamNum=*(pFind)-'1';
				
				st = strstr(pFind, "url=");
				if(	NULL==st)
				{
					printf("find error at the %d stream ",i+1);
					break;
					
				}		
				pFind=st+4;
				while(pFind[findLength]!='\r'&&pFind[findLength]!='\n')
				{
					findLength++;
				}
				memcpy(stream[streamNum].url,pFind,findLength);
				pFind+=findLength;
				findLength=0;

				st = strstr(pFind, "srcWidth=");
				if(	NULL==st)
				{
					printf("find error at the %d stream ",i+1);
					break;
					
				}

				pFind=st+9;
				stream[streamNum].srcWidth=atoi(pFind);
				while(pFind[findLength]!='\r'&&pFind[findLength]!='\n')
				{
					findLength++;
				}
				pFind+=findLength;
				findLength=0;

				st = strstr(pFind, "srcHeight=");
				if(	NULL==st)
				{
					printf("find error at the %d stream ",i+1);
					break;
					
				}
				pFind=st+10;
				stream[streamNum].srcHeight=atoi(pFind);
				while(pFind[findLength]!='\r'&&pFind[findLength]!='\n')
				{
					findLength++;
				}
				pFind+=findLength;
				findLength=0;

				st = strstr(pFind, "destX=");
				if(	NULL==st)
				{
					printf("find error at the %d stream ",i+1);
					break;
				}

				pFind=st+6;
				stream[streamNum].destX=atoi(pFind);
				while(pFind[findLength]!='\r'&&pFind[findLength]!='\n')
				{
					findLength++;
				}
				pFind+=findLength;
				findLength=0;

				st = strstr(pFind, "destY=");
				if(	NULL==st)
				{
					printf("find error at the %d stream ",i+1);
					break;
				}
				pFind=st+6;
				stream[streamNum].destY=atoi(pFind);
				while(pFind[findLength]!='\r'&&pFind[findLength]!='\n')
				{
					findLength++;
				}
				pFind+=findLength;
				findLength=0;
				
				st = strstr(pFind, "destWidth=");
				if( NULL==st)
				{
					printf("find error at the %d stream ",i+1);
					break;
				}
				pFind=st+10;
				stream[streamNum].destWidth=atoi(pFind);
				while(pFind[findLength]!='\r'&&pFind[findLength]!='\n')
				{
					findLength++;
				}
				pFind+=findLength;
				findLength=0;		

				st = strstr(pFind, "destHeight=");
				if(	NULL==st)
				{
					printf("find error at the %d stream ",i+1);
					break;
				}
				pFind=st+11;
				stream[streamNum].destHeight=atoi(pFind);
				while(pFind[findLength]!='\r'&&pFind[findLength]!='\n')
				{
					findLength++;
				}
				pFind+=findLength;
				findLength=0;
				streamNum=-1;
			}
			fclose(fp1);
			//printf("change happened\r\n ");	
			if(lastTime!=0)
			{
			printf("change happened\r\n ");
			
			eventLoopWatchVariable1=1;
			eventLoopWatchVariable3=1;
			}
			lastTime=buf.st_mtime;
		}
	}
    return ((void *)0);

}

pthread_t ntid;
//bool changeHappened=0;

int original_main() {

	 rtspClientCount = 0;
	  TaskScheduler* scheduler = BasicTaskScheduler::createNew();
	  UsageEnvironment* env = BasicUsageEnvironment::createNew(*scheduler);
	  eventLoopWatchVariable=0;
	    eventLoopWatchVariable1=0;
	    int err;
	    err = pthread_create(&ntid, NULL,thr_fn,NULL);
	    if(err!=0){
	        printf("pthread_create error\r\n");
	    }
		//printf("pthread_create success\r\n");
	    usleep(1000);
	int j;

  // There are argc-1 URLs: argv[1] through argv[argc-1].  Open and start streaming each one:
  for ( j= 0; stream[j].url[0]=='r'; ++j) {
    openURL(*env, stream[j].url);
	printf("\r\n#3333333333333333333333333333333333333333333333333 j=====%d\r\n",j);
  }

  // All subsequent activity takes place within the event loop:
  env->taskScheduler().doEventLoop(&eventLoopWatchVariable);
    // This function call does not return, unless, at some point in time, "eventLoopWatchVariable" gets set to something non-zero.
	  printf("\r\nthe end coming\r\n");
	 VoExit();
	  env->reclaim();
	 env = NULL;
	 delete scheduler;
	  scheduler = NULL;
	


  return 0;

  // If you choose to continue the application past this point (i.e., if you comment out the "return 0;" statement above),
  // and if you don't intend to do anything more with the "TaskScheduler" and "UsageEnvironment" objects,
  // then you can also reclaim the (small) memory used by these objects by uncommenting the following code:
  /*
    env->reclaim(); env = NULL;
    delete scheduler; scheduler = NULL;
  */
}

int main()
{
	//rtspClient = (RTSPClient**) malloc(16*sizeof(RTSPClient*));

	do{
		original_main();


	}while(1);

}



// Define a class to hold per-stream state that we maintain throughout each stream's lifetime:

class StreamClientState {
public:
  StreamClientState();
  virtual ~StreamClientState();

public:
  MediaSubsessionIterator* iter;
  MediaSession* session;
  MediaSubsession* subsession;
  TaskToken streamTimerTask;
  double duration;
};

// If you're streaming just a single stream (i.e., just from a single URL, once), then you can define and use just a single
// "StreamClientState" structure, as a global variable in your application.  However, because - in this demo application - we're
// showing how to play multiple streams, concurrently, we can't do that.  Instead, we have to have a separate "StreamClientState"
// structure for each "RTSPClient".  To do this, we subclass "RTSPClient", and add a "StreamClientState" field to the subclass:

class ourRTSPClient: public RTSPClient {
public:
  static ourRTSPClient* createNew(UsageEnvironment& env, char const* rtspURL,
				  int verbosityLevel = 0,
				  char const* applicationName = NULL,
				  portNumBits tunnelOverHTTPPortNum = 0);

protected:
  ourRTSPClient(UsageEnvironment& env, char const* rtspURL,
		int verbosityLevel, char const* applicationName, portNumBits tunnelOverHTTPPortNum);
    // called only by createNew();
  virtual ~ourRTSPClient();

public:
	
  StreamClientState scs;
};

// Define a data sink (a subclass of "MediaSink") to receive the data for each subsession (i.e., each audio or video 'substream').
// In practice, this might be a class (or a chain of classes) that decodes and then renders the incoming audio or video.
// Or it might be a "FileSink", for outputting the received data into a file (as is done by the "openRTSP" application).
// In this example code, however, we define a simple 'dummy' sink that receives incoming data, but does nothing with it.

class DummySink: public MediaSink {
public:
  static DummySink* createNew(UsageEnvironment& env,
			      MediaSubsession& subsession, // identifies the kind of data that's being received
			      char const* streamId = NULL,int const chnnum=-1); // identifies the stream itself (optional)
	void * SAMPLE_COMM_VDEC_SendStream_clt(unsigned int          receivebyte);

private:
  DummySink(UsageEnvironment& env, MediaSubsession& subsession, char const* streamId,int const chnnum);
    // called only by "createNew()"
  virtual ~DummySink();

  static void afterGettingFrame(void* clientData, unsigned frameSize,
                                unsigned numTruncatedBytes,
				struct timeval presentationTime,
                                unsigned durationInMicroseconds);
  void afterGettingFrame(unsigned frameSize, unsigned numTruncatedBytes,
			 struct timeval presentationTime, unsigned durationInMicroseconds);

private:
  // redefined virtual functions:
  virtual Boolean continuePlaying();

private:
  u_int8_t* fReceiveBuffer;
  MediaSubsession& fSubsession;
  char* fStreamId;
   int dummychnnum;
      u_int8_t vps[64];
   u_int8_t sps[64];
   u_int8_t pps[64];
   u_int8_t sei[64];
   unsigned vpsSize;
   unsigned spsSize;
   unsigned ppsSize;
   unsigned seiSize;
public:

};

#define RTSP_CLIENT_VERBOSITY_LEVEL 1 // by default, print verbose output from each "RTSPClient"




void openURL(UsageEnvironment& env, char const* rtspURL) {
  // Begin by creating a "RTSPClient" object.  Note that there is a separate "RTSPClient" object for each stream that we wish
  // to receive (even if more than stream uses the same "rtsp://" URL).
  
 RTSPClient* rtspClienttemp= ourRTSPClient::createNew(env, rtspURL, RTSP_CLIENT_VERBOSITY_LEVEL);
   
  rtspClient[rtspClientCount]=rtspClienttemp;
  if (rtspClient[rtspClientCount] == NULL) {
    env << "Failed to create a RTSP client for URL \"" << rtspURL << "\": " << env.getResultMsg() << "\n";
    return;
  }
  rtspClient[rtspClientCount]->chnnumparam=rtspClientCount;
  //for(int i=0;i<5;i++)
 // printf("rtspClient->chnnumparamopenURL  is %d\r\n",rtspClient->chnnumparam);


  ++rtspClientCount;
 
printf("dddddddddddddddddddddddddddddddddddddddddddddd%d",rtspClientCount);
  // Next, send a RTSP "DESCRIBE" command, to get a SDP description for the stream.
  // Note that this command - like all RTSP commands - is sent asynchronously; we do not block, waiting for a response.
  // Instead, the following function call returns immediately, and we handle the RTSP response later, from within the event loop:
  rtspClient[rtspClientCount-1]->sendDescribeCommand(continueAfterDESCRIBE); 
  
}


// Implementation of the RTSP 'response handlers':

void continueAfterDESCRIBE(RTSPClient* rtspClient, int resultCode, char* resultString) {
  do {
    UsageEnvironment& env = rtspClient->envir(); // alias
    StreamClientState& scs = ((ourRTSPClient*)rtspClient)->scs; // alias

    if (resultCode != 0) {
      env << *rtspClient << "Failed to get a SDP description: " << resultString << "\n";
      break;
    }

    char* const sdpDescription = resultString;
    env << *rtspClient << "Got a SDP description:\n" << sdpDescription << "\n";

    // Create a media session object from this SDP description:
    scs.session = MediaSession::createNew(env, sdpDescription);
    delete[] sdpDescription; // because we don't need it anymore
    if (scs.session == NULL) {
      env << *rtspClient << "Failed to create a MediaSession object from the SDP description: " << env.getResultMsg() << "\n";
      break;
    } else if (!scs.session->hasSubsessions()) {
      env << *rtspClient << "This session has no media subsessions (i.e., no \"m=\" lines)\n";
      break;
    }

    
    // Then, create and set up our data source objects for the session.  We do this by iterating over the session's 'subsessions',
    // calling "MediaSubsession::initiate()", and then sending a RTSP "SETUP" command, on each one.
    // (Each 'subsession' will have its own data source.)
    scs.iter = new MediaSubsessionIterator(*scs.session);
    setupNextSubsession(rtspClient);
    return;
  } while (0);

  // An unrecoverable error occurred with this stream.
  shutdownStream(rtspClient);
}

void setupNextSubsession(RTSPClient* rtspClient) {
  UsageEnvironment& env = rtspClient->envir(); // alias
  StreamClientState& scs = ((ourRTSPClient*)rtspClient)->scs; // alias
    
  scs.subsession = scs.iter->next();

	if (scs.subsession != NULL) {
	    if (!scs.subsession->initiate()) {
		      env << *rtspClient << "Failed to initiate the \"" << *scs.subsession << "\" subsession: " << env.getResultMsg() << "\n";
		      setupNextSubsession(rtspClient); // give up on this subsession; go to the next one
	    }
		else
		{
		      env << *rtspClient << "Initiated the \"" << *scs.subsession
			  << "\" subsession (client ports " << scs.subsession->clientPortNum() << "-" << scs.subsession->clientPortNum()+1 << ")\n";
			  	printf("rtspClient->chnnum()=%d\r\n",rtspClient->chnnum());
				if(rtspClient->chnnum()==0&&eventLoopWatchVariable3==0 )
				{
					  int videoWidth,videoHeight;
						videoWidth=scs.subsession->videoWidth();
						videoHeight=scs.subsession->videoHeight();
						//printf("scs.subsession->videoWidth()=%d\r\n",scs.subsession->videoWidth());
						//
						//printf("scs.subsession->videoHeight()=%d\r\n",scs.subsession->videoHeight());

						if(scs.subsession->videoWidth()==0&&scs.subsession->videoHeight()==0){
						  videoWidth=stream[rtspClient->chnnum()].srcWidth;
						  videoHeight=stream[rtspClient->chnnum()].srcHeight;
						}
						stream[rtspClient->chnnum()].pic_clt=0;
					
					  if(strcmp( scs.subsession->mediumName(),"video")==0){
					
						if(videoWidth==1920&&videoHeight==1080){
						  stream[rtspClient->chnnum()].pic_clt= PIC_HD1080_CLT; 
						  }
						else if(videoWidth==1280&&videoHeight==720){
						  stream[rtspClient->chnnum()].pic_clt = PIC_HD720_CLT;
						  }
						else if(videoWidth==320&&videoHeight==240){
						  stream[rtspClient->chnnum()].pic_clt = PIC_QVGA_CLT;
						  }
						else if(videoWidth==640 &&videoHeight==480){
						  stream[rtspClient->chnnum()].pic_clt = PIC_VGA_CLT;
						  }
						else if(videoWidth==1024&&videoHeight==768){
						  stream[rtspClient->chnnum()].pic_clt = PIC_XGA_CLT;
						  }
						else if(videoWidth==1400&&videoHeight==1050){
						  stream[rtspClient->chnnum()].pic_clt = PIC_SXGA_CLT;
						  }
						else if(videoWidth==1600&&videoHeight==1200){
						  stream[rtspClient->chnnum()].pic_clt = PIC_UXGA_CLT;
						  }
						else if(videoWidth==2048&&videoHeight==1536){
						 stream[rtspClient->chnnum()].pic_clt = PIC_QXGA_CLT;
						  }
						else if(videoWidth==854&&videoHeight==480){
						  stream[rtspClient->chnnum()].pic_clt = PIC_WVGA_CLT;
						  }
						else if(videoWidth==1680&&videoHeight==1050){
						  stream[rtspClient->chnnum()].pic_clt = PIC_WSXGA_CLT;
						  }
						else if(videoWidth==1920&&videoHeight==1200){
						  stream[rtspClient->chnnum()].pic_clt =  PIC_WUXGA_CLT;
						  }
						else if(videoWidth==2560&&videoHeight==1600){
						  stream[rtspClient->chnnum()].pic_clt = PIC_WQXGA_CLT;
						  }
						else if(videoWidth==960&&videoHeight==540){
						  stream[rtspClient->chnnum()].pic_clt = PIC_qHD_CLT;
						  }
						else if(videoWidth==3840&&videoHeight==2160){
						  stream[rtspClient->chnnum()].pic_clt = PIC_UHD4K_CLT;
						  }
					
						stream[rtspClient->chnnum()].payloadtype=-1;
						if((strcmp(scs.subsession->codecName(),"H264"))==0){
						  stream[rtspClient->chnnum()].payloadtype=0;
						  }
						if((strcmp(scs.subsession->codecName(),"H265"))==0){
						  stream[rtspClient->chnnum()].payloadtype=1;
						  }
						SendParamToVO(stream);
						VoInit();
					  }
					  if(eventLoopWatchVariable3==1){
					  	
					  		SendParamToVO(stream);
					  		SetLocation();
							eventLoopWatchVariable3=0;
					  }
				}
		      	// Continue setting up this subsession, by sending a RTSP "SETUP" command:
		     	 rtspClient->sendSetupCommand(*scs.subsession, continueAfterSETUP);
	    }
    return;
  }

  // We've finished setting up all of the subsessions.  Now, send a RTSP "PLAY" command to start the streaming:
  if (scs.session->absStartTime() != NULL) {
    // Special case: The stream is indexed by 'absolute' time, so send an appropriate "PLAY" command:
    rtspClient->sendPlayCommand(*scs.session, continueAfterPLAY, scs.session->absStartTime(), scs.session->absEndTime());
  } else {
    scs.duration = scs.session->playEndTime() - scs.session->playStartTime();
    rtspClient->sendPlayCommand(*scs.session, continueAfterPLAY);
  }
}

void continueAfterSETUP(RTSPClient* rtspClient, int resultCode, char* resultString) {
  do {
    UsageEnvironment& env = rtspClient->envir(); // alias
    StreamClientState& scs = ((ourRTSPClient*)rtspClient)->scs; // alias


    if (resultCode != 0) {
      env << *rtspClient << "Failed to set up the \"" << *scs.subsession << "\" subsession: " << env.getResultMsg() << "\n";
      break;
    }

    env << *rtspClient << "Set up the \"" << *scs.subsession
	<< "\" subsession (client ports " << scs.subsession->clientPortNum() << "-" << scs.subsession->clientPortNum()+1 << ")\n";

    // Having successfully setup the subsession, create a data sink for it, and call "startPlaying()" on it.
    // (This will prepare the data sink to receive data; the actual flow of data from the client won't start happening until later,
    // after we've sent a RTSP "PLAY" command.)

    scs.subsession->sink = DummySink::createNew(env, *scs.subsession, rtspClient->url(), rtspClient->chnnum());


      // perhaps use your own custom "MediaSink" subclass instead
    if (scs.subsession->sink == NULL) {
      env << *rtspClient << "Failed to create a data sink for the \"" << *scs.subsession
	  << "\" subsession: " << env.getResultMsg() << "\n";
      break;
    }
	

    env << *rtspClient << "Created a data sink for the \"" << *scs.subsession << "\" subsession\n";
    scs.subsession->miscPtr = rtspClient; // a hack to let subsession handle functions get the "RTSPClient" from the subsession 
    scs.subsession->sink->startPlaying(*(scs.subsession->readSource()),
				       subsessionAfterPlaying, scs.subsession);
    // Also set a handler to be called if a RTCP "BYE" arrives for this subsession:
    if (scs.subsession->rtcpInstance() != NULL) {
      scs.subsession->rtcpInstance()->setByeHandler(subsessionByeHandler, scs.subsession);
    }
  } while (0);

  // Set up the next subsession, if any:
  setupNextSubsession(rtspClient);
}

void continueAfterPLAY(RTSPClient* rtspClient, int resultCode, char* resultString) {
  do {
    UsageEnvironment& env = rtspClient->envir(); // alias
    StreamClientState& scs = ((ourRTSPClient*)rtspClient)->scs; // alias
  printf("continueAfterPLAY\r\n");

//	if(  eventLoopWatchVariable1!=0)
//	{
//		shutdownStream(rtspClient);
//		printf("close close vvvvvvvvvvvvvvvvvvvvvvvvvvfffffffffffffff\r\n");
//		
//		
//	}

    if (resultCode != 0) {
      env << *rtspClient << "Failed to start playing session: " << resultString << "\n";
      break;
    }

    // Set a timer to be handled at the end of the stream's expected duration (if the stream does not already signal its end
    // using a RTCP "BYE").  This is optional.  If, instead, you want to keep the stream active - e.g., so you can later
    // 'seek' back within it and do another RTSP "PLAY" - then you can omit this code.
    // (Alternatively, if you don't want to receive the entire stream, you could set this timer for some shorter value.)
    if (scs.duration > 0) {
      unsigned const delaySlop = 2; // number of seconds extra to delay, after the stream's expected duration.  (This is optional.)
      scs.duration += delaySlop;
      unsigned uSecsToDelay = (unsigned)(scs.duration*1000000);
      scs.streamTimerTask = env.taskScheduler().scheduleDelayedTask(uSecsToDelay, (TaskFunc*)streamTimerHandler, rtspClient);
    }

    env << *rtspClient << "Started playing session";
    if (scs.duration > 0) {
      env << " (for up to " << scs.duration << " seconds)";
    }
    env << "...\n";

    return;
  } while (0);

  // An unrecoverable error occurred with this stream.
  shutdownStream(rtspClient);
}


// Implementation of the other event handlers:

void subsessionAfterPlaying(void* clientData) {
  MediaSubsession* subsession = (MediaSubsession*)clientData;
  RTSPClient* rtspClient = (RTSPClient*)(subsession->miscPtr);
  printf("subsessionAfterPlaying\r\n");

  // Begin by closing this subsession's stream:
  Medium::close(subsession->sink);
  subsession->sink = NULL;

  // Next, check whether *all* subsessions' streams have now been closed:
  MediaSession& session = subsession->parentSession();
  MediaSubsessionIterator iter(session);
  while ((subsession = iter.next()) != NULL) {
    if (subsession->sink != NULL) return; // this subsession is still active
  }

  // All subsessions' streams have now been closed, so shutdown the client:
  shutdownStream(rtspClient);
}

void subsessionByeHandler(void* clientData) {
  MediaSubsession* subsession = (MediaSubsession*)clientData;
  RTSPClient* rtspClient = (RTSPClient*)subsession->miscPtr;
  UsageEnvironment& env = rtspClient->envir(); // alias

  env << *rtspClient << "Received RTCP \"BYE\" on \"" << *subsession << "\" subsession\n";

  // Now act as if the subsession had closed:
  subsessionAfterPlaying(subsession);
}

void streamTimerHandler(void* clientData) {
  ourRTSPClient* rtspClient = (ourRTSPClient*)clientData;
  StreamClientState& scs = rtspClient->scs; // alias

  scs.streamTimerTask = NULL;

  // Shut down the stream:
  shutdownStream(rtspClient);
}

void shutdownStream(RTSPClient* rtspClient, int exitCode) {
  UsageEnvironment& env = rtspClient->envir(); // alias
  StreamClientState& scs = ((ourRTSPClient*)rtspClient)->scs; // alias

  // First, check whether any subsessions have still to be closed:
  if (scs.session != NULL) { 
    Boolean someSubsessionsWereActive = False;
    MediaSubsessionIterator iter(*scs.session);
    MediaSubsession* subsession;

    while ((subsession = iter.next()) != NULL) {
      if (subsession->sink != NULL) {
	Medium::close(subsession->sink);
	subsession->sink = NULL;

	if (subsession->rtcpInstance() != NULL) {
	  subsession->rtcpInstance()->setByeHandler(NULL, NULL); // in case the server sends a RTCP "BYE" while handling "TEARDOWN"
	}

	someSubsessionsWereActive = True;
      }
    }

    if (someSubsessionsWereActive) {
      // Send a RTSP "TEARDOWN" command, to tell the server to shutdown the stream.
      // Don't bother handling the response to the "TEARDOWN".
      rtspClient->sendTeardownCommand(*scs.session, NULL);
    }
  }

  env << *rtspClient << "Closing the stream.\n";
  Medium::close(rtspClient);
    // Note that this will also cause this stream's "StreamClientState" structure to get reclaimed.

 // if (--rtspClientCount == 0) {
    // The final stream has ended, so exit the application now.
    // (Of course, if you're embedding this code into your own application, you might want to comment this out,
    // and replace it with "eventLoopWatchVariable = 1;", so that we leave the LIVE555 event loop, and continue running "main()".)
   // exit(exitCode);
 // }
}


// Implementation of "ourRTSPClient":

ourRTSPClient* ourRTSPClient::createNew(UsageEnvironment& env, char const* rtspURL,
					int verbosityLevel, char const* applicationName, portNumBits tunnelOverHTTPPortNum) {
  return new ourRTSPClient(env, rtspURL, verbosityLevel, applicationName, tunnelOverHTTPPortNum);
}

ourRTSPClient::ourRTSPClient(UsageEnvironment& env, char const* rtspURL,
			     int verbosityLevel, char const* applicationName, portNumBits tunnelOverHTTPPortNum)
  : RTSPClient(env,rtspURL, verbosityLevel, applicationName, tunnelOverHTTPPortNum) {
}

ourRTSPClient::~ourRTSPClient() {
}


// Implementation of "StreamClientState":

StreamClientState::StreamClientState()
  : iter(NULL), session(NULL), subsession(NULL), streamTimerTask(NULL), duration(0.0) {
}

StreamClientState::~StreamClientState() {
  delete iter;
  if (session != NULL) {
    // We also need to delete "session", and unschedule "streamTimerTask" (if set)
    UsageEnvironment& env = session->envir(); // alias

    env.taskScheduler().unscheduleDelayedTask(streamTimerTask);
    Medium::close(session);
  }
}


// Implementation of "DummySink":

// Even though we're not going to be doing anything with the incoming data, we still need to receive it.
// Define the size of the buffer that we'll use:
#define DUMMY_SINK_RECEIVE_BUFFER_SIZE 1024000

DummySink* DummySink::createNew(UsageEnvironment& env, MediaSubsession& subsession, char const* streamId,int const chnnum) {
  return new DummySink(env, subsession, streamId,chnnum);
}




DummySink::DummySink(UsageEnvironment& env, MediaSubsession& subsession, char const* streamId,int const chnnum)
  : MediaSink(env),
    fSubsession(subsession)
   {
  fStreamId = strDup(streamId);
  dummychnnum=chnnum;
  memset(vps, 0, sizeof(vps));
    memset(sps, 0, sizeof(sps));
    memset(pps, 0, sizeof(pps));
    memset(sei, 0, sizeof(sei));
    vpsSize = 0;
    spsSize = 0;
    ppsSize = 0;
    seiSize = 0;
	
   
  fReceiveBuffer = new u_int8_t[DUMMY_SINK_RECEIVE_BUFFER_SIZE];
}

DummySink::~DummySink() {
  delete[] fReceiveBuffer;
  delete[] fStreamId;
  
}

void DummySink::afterGettingFrame(void* clientData, unsigned frameSize, unsigned numTruncatedBytes,
				  struct timeval presentationTime, unsigned durationInMicroseconds) {
  DummySink* sink = (DummySink*)clientData;
  sink->afterGettingFrame(frameSize, numTruncatedBytes, presentationTime, durationInMicroseconds);
}

// If you don't want to see debugging output for each received frame, then comment out the following line:
#define DEBUG_PRINT_EACH_RECEIVED_FRAME 1

void DummySink::afterGettingFrame(unsigned frameSize, unsigned numTruncatedBytes,
				  struct timeval presentationTime, unsigned /*durationInMicroseconds*/) {
  // We've just received a frame of data.  (Optionally) print out information about it:

	  
  
//#ifdef DEBUG_PRINT_EACH_RECEIVED_FRAME
//
//
//envir() << "Stream \"" << fStreamId << "\"; ";
//  envir() << fSubsession.mediumName() << "/" << fSubsession.codecName() << ":\tReceived " << frameSize << " bytes";
//  if (numTruncatedBytes > 0) envir() << " (with " << numTruncatedBytes << " bytes truncated)";
//  char uSecsStr[6+1]; // used to output the 'microseconds' part of the presentation time
//  sprintf(uSecsStr, "%06u", (unsigned)presentationTime.tv_usec);
//  envir() << ".\tPresentation time: " << (unsigned)presentationTime.tv_sec << "." << uSecsStr;
//  if (fSubsession.rtpSource() != NULL && !fSubsession.rtpSource()->hasBeenSynchronizedUsingRTCP()) {
//    envir() << "!"; // mark the debugging output to indicate that this presentation time is not RTCP-synchronized
//  }
//#ifdef DEBUG_PRINT_NPT
//  envir() << "\tNPT: " << fSubsession.getNormalPlayTime(presentationTime);
//#endif
//  envir() << "\n";
//#endif
//		 envir() <<"the width:"<< fSubsession.videoWidth()<<"the height:"<< fSubsession.videoHeight()<<"\r\n";		 

	if(strcmp(fSubsession.mediumName(),"video")==0){
	/*	#ifdef DEBUG_PRINT_EACH_RECEIVED_FRAME


  if (fStreamId != NULL) envir() << "Stream \"" << fStreamId << "\"; ";
  envir() << fSubsession.mediumName() << "/" << fSubsession.codecName() << ":\tReceived " << frameSize << " bytes";
  if (numTruncatedBytes > 0) envir() << " (with " << numTruncatedBytes << " bytes truncated)";
  char uSecsStr[6+1]; // used to output the 'microseconds' part of the presentation time
  sprintf(uSecsStr, "%06u", (unsigned)presentationTime.tv_usec);
  envir() << ".\tPresentation time: " << (unsigned)presentationTime.tv_sec << "." << uSecsStr;
  if (fSubsession.rtpSource() != NULL && !fSubsession.rtpSource()->hasBeenSynchronizedUsingRTCP()) {
    envir() << "!"; // mark the debugging output to indicate that this presentation time is not RTCP-synchronized
  }
#ifdef DEBUG_PRINT_NPT
  envir() << "\tNPT: " << fSubsession.getNormalPlayTime(presentationTime);
#endif
  envir() << "\n";
#endif
		 envir() <<"the width:"<< fSubsession.videoWidth()<<"the height:"<< fSubsession.videoHeight()<<"\r\n";
		 */
		SAMPLE_COMM_VDEC_SendStream_clt(frameSize);//debug
	//	envir() <<"the width:"<< fSubsession.videoWidth()<<"the height:"<< fSubsession.videoHeight()<<"\r\n";


	}

  
  // Then continue, to request the next frame of data:
  continuePlaying();
}

Boolean DummySink::continuePlaying() {
  if (fSource == NULL) return False; // sanity check (should not happen)

  // Request the next frame of data from our input source.  "afterGettingFrame()" will get called later, when it arrives:
  fSource->getNextFrame(fReceiveBuffer, DUMMY_SINK_RECEIVE_BUFFER_SIZE,
                        afterGettingFrame, this,
                        onSourceClosure, this);
 // printf("continuePlaying vvvvvvvvvvvvvvvvvvvvvvvvvvfffffffffffffff\r\n");
  	if(  eventLoopWatchVariable1!=0)
	{
		
		printf("eventLoopWatchVariable1 vvvvvvvvvvvvvvvvvvvvvvvvvvfffffffffffffff%d\r\n",rtspClientCount);
		
		for(int i=0;i<rtspClientCount;i++)
		{
			printf("close close vvvvvvvvvvvvvvvvvvvvvvvvvvfffffffffffffff%d\r\n",rtspClientCount);
			shutdownStream( rtspClient[i]);
			
			
		}
		
		eventLoopWatchVariable=1;
		
	}

  return True;
}


void * DummySink::SAMPLE_COMM_VDEC_SendStream_clt(unsigned int  receivebyte)
{
	unsigned int minBufSize = 1920*1080*1.5;//debug
	//unsigned int minBufSize = 3840*2160*1.5;
	

	SVdecStreamCLT stStream;
	int s32Ret;
	
	fflush(stdout);

	unsigned int sendBytes=0;
	stStream.bEndOfFrame  =0;
	stStream.bEndOfStream =0;	
	stStream.u64PTS  = 0;
		

	
		   unsigned char SliceStartCode[4];
	SliceStartCode[0] = 0x00;
    SliceStartCode[1] = 0x00;
    SliceStartCode[2] = 0x01;
  //  SliceStartCode[3] = 0x01;
 	if(strcmp(fSubsession.mediumName(), "video") == 0){
        int bwrite=0;
        if(strcmp(fSubsession.codecName(), "H264") == 0){
                memmove(fReceiveBuffer + sizeof(SliceStartCode),fReceiveBuffer,receivebyte);
                    fReceiveBuffer[0]=0x00;
                    fReceiveBuffer[1]=0x00;
                    fReceiveBuffer[2]=0x00;
                    fReceiveBuffer[3]=0x01;
                    bwrite=1;
					receivebyte=receivebyte+sizeof(SliceStartCode);
       	if(bwrite)
	   		{
   			while(sendBytes<receivebyte)
				{
				int thisTimeSendCount 	= 	minBufSize;
				if(receivebyte-sendBytes < minBufSize)
					thisTimeSendCount = receivebyte-sendBytes;
					stStream.pu8Addr 		= fReceiveBuffer+sendBytes;			
					stStream.u32Len  		= thisTimeSendCount; 
					sendBytes += thisTimeSendCount;
					s32Ret=MpiVdecSendStreamCLT(dummychnnum, &stStream, 0);


					 //printf("dummychnnumafterGettingFrame is %d\r\n",dummychnnum);
				if (0 != s32Ret)
				usleep(100);				
				}
			}
       }
       else if(strcmp(fSubsession.codecName(), "H265") == 0){
                 int type = ((fReceiveBuffer[0]) & 0x7E)>>1;               
                 switch (type) {
                   case 32:
                   {
                       memcpy(vps+sizeof(SliceStartCode), fReceiveBuffer, receivebyte);
                       vps[0]=0x00;
                       vps[1]=0x00;
                       vps[2]=0x00;
                       vps[3]=0x01;
                       vpsSize = receivebyte + sizeof(SliceStartCode);
					     printf("  case 32:\r\n");
                       break;

                   }
                   case 33:
                   {
                       memcpy(sps+sizeof(SliceStartCode), fReceiveBuffer, receivebyte);
                       sps[0]=0x00;
                       sps[1]=0x00;
                       sps[2]=0x00;
                       sps[3]=0x01;
                       spsSize = receivebyte + sizeof(SliceStartCode);
					     printf(" case 33:\r\n");
                       break;
                   }
                   case 34:
                   { printf(" case 34:\r\n");
                       memcpy(pps+sizeof(SliceStartCode), fReceiveBuffer, receivebyte);
                       pps[0]=0x00;
                       pps[1]=0x00;
                       pps[2]=0x00;
                       pps[3]=0x01;
                       ppsSize = receivebyte + sizeof(SliceStartCode);
					     printf(" case 34:\r\n");
                       break;
                   }
                   case 39:
                 //  case 40:
                   {printf(" case 39:\r\n");
                       memcpy(sei+sizeof(SliceStartCode)-1, fReceiveBuffer, receivebyte );
                       sei[0]=0x00;
                       sei[1]=0x00;
                       sei[2]=0x01;
                     
                       seiSize = receivebyte + sizeof(SliceStartCode);
					     printf(" case 39:\r\n");
                       break;
                   }
			
    			  case 1:
				  
                   {printf("case 1:\r\n");
                       receivebyte  = receivebyte  + sizeof(SliceStartCode-1);
				   fReceiveBuffer[0]=0x00;
                    fReceiveBuffer[1]=0x00;
					 fReceiveBuffer[2]=0x01;
					
				
                       bwrite =1;
                       break;
                   }
			//	  case 20:
				 
                   case 19:
                   {  printf("case 20:\r\n");
				   printf("vpsSize:%d\r\n",vpsSize);
				   printf("spsSize:%d\r\n",spsSize);
				   printf("ppsSize :%d\r\n",ppsSize);
				   printf("seiSize:%d\r\n",seiSize);
				   printf("sizeof(SliceStartCode)%d\r\n",sizeof(SliceStartCode-1));
                     memmove(fReceiveBuffer + vpsSize + spsSize+ ppsSize + seiSize+sizeof(SliceStartCode), fReceiveBuffer, receivebyte);                                                             memcpy(fReceiveBuffer, vps, vpsSize);
                     memcpy(fReceiveBuffer + vpsSize, sps, spsSize);
                     memcpy(fReceiveBuffer + vpsSize + spsSize, pps, ppsSize);
                     memcpy(fReceiveBuffer + vpsSize + spsSize + ppsSize, sei, seiSize);
                       int index=vpsSize + spsSize + ppsSize+seiSize;
                       fReceiveBuffer[index+0]=0x00;
                       fReceiveBuffer[index+1]=0x00;
					     fReceiveBuffer[index+2]=0x00;
                       fReceiveBuffer[index+3]=0x01;
                   

                       receivebyte = vpsSize + spsSize + ppsSize + seiSize + sizeof(SliceStartCode) + receivebyte;
                       bwrite = 1;
					    printf(" case 19:\r\n");
                       break;
                   }
                 default:
                   {	printf(" default:\r\n");
                       receivebyte = receivebyte + sizeof(SliceStartCode-1);
					    fReceiveBuffer[0]=0x00;
                    fReceiveBuffer[1]=0x00;
                    fReceiveBuffer[2]=0x01;
                       bwrite =1;
					   printf("default:\r\n");
					   
                       break;
                   }
                   
           }
		if(bwrite)
	   		{
   			while(sendBytes<receivebyte)
				{
				int thisTimeSendCount 	= 	minBufSize;
				if(receivebyte-sendBytes < minBufSize)
					thisTimeSendCount = receivebyte-sendBytes;
					stStream.pu8Addr 		= fReceiveBuffer+sendBytes;			
					stStream.u32Len  		= thisTimeSendCount; 
					sendBytes += thisTimeSendCount;
					s32Ret=MpiVdecSendStreamCLT(dummychnnum, &stStream, 0);
					 printf("dummychnnumafterGettingFrame is %d\r\n",dummychnnum);
				if (0 != s32Ret)
				usleep(100);				
				}
			}
       }
	}
    else if(strcmp(fSubsession.mediumName(), "audio") == 0){
        /*handler audio data*/
	for(int i=0;i<6;i++)
      printf("has recivie audio data\r\n");


    } else
    {
        printf("Stream type is unknown. neither video, nor audio!");
    }

  return (void *)0;
}

      /*
		unsigned char buf[4];
		 int bwrite=0;
	

		if(strcmp(fSubsession.codecName(), "H264") == 0)
			   {   buf[1]= 0x00;
	 buf[2]= 0x00;
	  buf[0]= 0x00;
	   buf[3]= 0x01;
		
		  stStream.pu8Addr 	   = buf;		   
  		  stStream.u32Len		   = 4 ;
				 s32Ret=MpiVdecSendStreamCLT(dummychnnum, &stStream, 0);
bwrite=1;
			if (0 != s32Ret)
			usleep(100);
			}
		else if(strcmp(fSubsession.codecName(), "H265") == 0){
                 int type = ((fReceiveBuffer[0]) & 0x7E)>>1;
				  switch (type) {
                   case 32:
                   { buf[1]= 0x00;
	 buf[2]= 0x00;
	  buf[0]= 0x00;
	   buf[3]= 0x01;
		
				    stStream.pu8Addr 	   = buf;		   
  		  stStream.u32Len		   = 4 ;
                   s32Ret=MpiVdecSendStreamCLT(dummychnnum, &stStream, 0);
			if (0 != s32Ret)
			usleep(100);break;}
				    case 33:
                   { buf[1]= 0x00;
	 buf[2]= 0x00;
	  buf[0]= 0x00;
	   buf[3]= 0x01;
		
					  stStream.pu8Addr 	   = buf;		   
  		  stStream.u32Len		   = 4 ;
                   s32Ret=MpiVdecSendStreamCLT(dummychnnum, &stStream, 0);
			if (0 != s32Ret)
			usleep(100);break;}
					  case 34:
                   { buf[1]= 0x00;
	 buf[2]= 0x00;
	  buf[0]= 0x00;
	   buf[3]= 0x01;
		
					    stStream.pu8Addr 	   = buf;		   
  		  stStream.u32Len		   = 4 ;
                   s32Ret=MpiVdecSendStreamCLT(dummychnnum, &stStream, 0);
			if (0 != s32Ret)
			usleep(100);break;}
					  case 39:
                   case 40:
                   { buf[1]= 0x00;
	 buf[2]= 0x00;
	  buf[0]= 0x00;
	   buf[3]= 0x01;
		
					    stStream.pu8Addr 	   = buf;		   
  		  stStream.u32Len		   = 4 ;
                   s32Ret=MpiVdecSendStreamCLT(dummychnnum, &stStream, 0);
			if (0 != s32Ret)
			usleep(100);break;}
				   case 19:
                   case 20:
                   {
                    buf[1]= 0x00;
	 buf[2]= 0x00;
	  buf[0]= 0x00;
	   buf[3]= 0x01;
		
				     stStream.pu8Addr 	   = buf;		   
  		  stStream.u32Len		   = 4 ;
                   s32Ret=MpiVdecSendStreamCLT(dummychnnum, &stStream, 0);
			if (0 != s32Ret)
			usleep(100);break;
			bwrite =1;

				   }
				   default:
                   {
                     
                       bwrite =1;
					   receivebyte = receivebyte + sizeof(buf[4]);
                       break;
                   }
				   
		}


			}
					
*/
	

	
/*
	char str[8] = {0};

	envir() << ".\buffer is  \r\n";				
	for(int i =0;i<receivebyte;i++)
	{
		sprintf(str, "%02X", fReceiveBuffer[i]);
		envir() << str<<" ";

		if(i%4==0)
			envir()<<" ";

		if(i%8==0)
			envir()<<"   ";

		if(i%16==0)
			envir()<<"   ";

		if(i%32==0)
			envir()<<"\r\n";

		
	}
	envir() << "\r\n";				

	*/


	
	



