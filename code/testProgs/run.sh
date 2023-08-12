clear
#LD_LIBRARY_PATH=. ./testRTSPClient rtsp://192.168.1.197:8554/1
#LD_LIBRARY_PATH=. ./testRTSPClient rtsp://admin:clthi3536@192.168.1.64 rtsp://192.168.1.197:8554/1
#gdbserver 192.168.1.198:1234 testRTSPClient 
LD_LIBRARY_PATH=. ./testRTSPClient 

