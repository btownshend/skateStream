#include <string.h>
#include <strstream>
#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup(){
    camWidth = 1280;  // try to grab at this size.
    camHeight = 800;

    
    //GUI Setup
    gui.setup();
    //gui.setup("GUI","settings.xml",10,10); // most of the time you don't need a name
    gui.add(scrubber.set("scrubber", 140, 10, 300));
    
    //get back a list of devices.
    vector<ofVideoDevice> devices = vidGrabber.listDevices();

    for(size_t i = 0; i < devices.size(); i++){
        if(devices[i].bAvailable){
            //log the device
            ofLogNotice() << devices[i].id << ": " << devices[i].deviceName;
        }else{
            //log the device and note it as unavailable
            ofLogNotice() << devices[i].id << ": " << devices[i].deviceName << " - unavailable ";
        }
    }

    vidGrabber.setDeviceID(0);
    vidGrabber.setDesiredFrameRate(30);
    vidGrabber.initGrabber(camWidth, camHeight);

    ofSetVerticalSync(true);

    udpConnection.Create();
    udpConnection.SetEnableBroadcast(true);
    udpConnection.BindMcast((char *)"0.0.0.0", portNo);
    udpConnection.SetNonBlocking(true);

    // Video write setup
    //    vidRecorder.setFfmpegLocation(ofFilePath::getAbsolutePath("ffmpeg")); // use this is you have ffmpeg installed in your data folder

    // override the default codecs if you like
    // run 'ffmpeg -codecs' to find out what your implementation supports (or -formats on some older versions)
    vidRecorder.setVideoCodec("mpeg4");
    vidRecorder.setVideoBitrate("800k");
    vidRecorder.setAudioCodec("mp3");
    vidRecorder.setAudioBitrate("192k");

    soundStream.setup(this, 0, channels, sampleRate, 256, 4);

    ofAddListener(vidRecorder.outputFileCompleteEvent, this, &ofApp::recordingComplete);
    bRecording = false;

    // Frame rate
    ofSetFrameRate(30);

}

//--------------------------------------------------------------
void ofApp::exit(){
    ofRemoveListener(vidRecorder.outputFileCompleteEvent, this, &ofApp::recordingComplete);
    vidRecorder.close();
}


// trim from end (in place)
static inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

void ofApp::startRecording(string tag) {
    string ts=ofGetTimestampString();
    string basepath="/tmp";
    string imuFileName=basepath+"/imu_"+tag+"_"+ts+".csv";
    string vidFileName=basepath+"/vid_"+tag+"_"+ts+".mov";
    
    printf("New files: %s and %s\n", imuFileName.c_str(),vidFileName.c_str());
    imuFD=std::fopen(imuFileName.c_str(),"w");
    
    
    bRecording = !bRecording;
    if(bRecording && !vidRecorder.isInitialized()) {
        vidRecorder.setup(vidFileName, vidGrabber.getWidth(), vidGrabber.getHeight(), 30, sampleRate, channels);
        //          vidRecorder.setup(fileName+ofGetTimestampString()+fileExt, vidGrabber.getWidth(), vidGrabber.getHeight(), 30); // no audio
        //            vidRecorder.setup(fileName+ofGetTimestampString()+fileExt, 0,0,0, sampleRate, channels); // no video
        //          vidRecorder.setupCustomOutput(vidGrabber.getWidth(), vidGrabber.getHeight(), 30, sampleRate, channels, "-vcodec mpeg4 -b 1600k -acodec mp2 -ab 128k -f mpegts udp://localhost:1234"); // for custom ffmpeg output string (streaming, etc)
        
        // Start recording
        vidRecorder.start();
    }
    else if(!bRecording && vidRecorder.isInitialized()) {
        vidRecorder.setPaused(true);
    }
    else if(bRecording && vidRecorder.isInitialized()) {
        vidRecorder.setPaused(false);
    }
    
    // Flush any pending IMU data
    // Get UDP messages from accelerometer
    char udpMessage[100000];
    int nflushed=0;
    
    while (true) {
        udpConnection.Receive(udpMessage,100000);
        string message=udpMessage;
        rtrim(message);
        if (message == "") {
            break;
        }
        nflushed++;
    }
    printf("Flushed %d frames\n", nflushed);
}

void ofApp::stopRecording() {
    assert(imuFD!=NULL);
    fclose(imuFD);
    imuFD=NULL;
    bRecording = false;
    vidRecorder.close();
}


//--------------------------------------------------------------
void ofApp::update(){
    static float lasttime=0;
    vidGrabber.update();
    if(vidGrabber.isFrameNew() && bRecording){
        bool success = vidRecorder.addFrame(vidGrabber.getPixels());
        if (!success) {
            ofLogWarning("This frame was not added!");
        }
    }

    // Check if the video recorder encountered any error while writing video frame or audio smaples.
    if (vidRecorder.hasVideoError()) {
        ofLogWarning("The video recorder failed to write some frames!");
    }

    if (vidRecorder.hasAudioError()) {
        ofLogWarning("The video recorder failed to write some audio samples!");
    }

    // Get UDP messages from accelerometer
    char udpMessage[100000];
    int nmsg=0;
    static int noData=0;
    
    while (true) {
        udpConnection.Receive(udpMessage,100000);
        string message=udpMessage;
        rtrim(message);
        if (message == "") {
            if (nmsg>0)
                printf("[x%d]\n",nmsg);
            break;
        }
        noData=0;   // Reset no data counter
        if (nmsg==0)
            printf("Got msg: %s ",message.c_str());
        nmsg++;
        std::stringstream ss(message);
        std::string token;
        std::vector<string> tokens;

        while (std::getline(ss, token, ',')) {
            tokens.push_back(token);
        }

        // Split out time
        float time=stof(tokens[0]);
        string tag=tokens.back();
        rtrim(tag);
        printf("time=%f,tag=%s\n",time,tag.c_str());
        if (imuFD!=NULL && time-lasttime > 5) {
            // Paused
            printf("Time jump\n");
            stopRecording();
        }
        lasttime=time;
        // Write to file
        if (imuFD==NULL)
            startRecording(tag);
            // Don't add to imu file yet -- flushed some data while starting recording
        else
            fprintf(imuFD,"%s\n",message.c_str());
    }
    noData++;
    if (imuFD!=NULL &&noData>60) {
        // No data for a while, close files
        printf("No data for %d frames\n",noData);
        stopRecording();
    }
}

//--------------------------------------------------------------
void ofApp::draw(){
    ofBackground(100, 100, 100);

    // Draw video windows
    ofSetHexColor(0xffffff);
    vidGrabber.draw(20, 20);
    //vidGrabber.draw(20+camWidth+20, 20);

    // GUI
    gui.draw();

        stringstream ss;
    ss << "video queue size: " << vidRecorder.getVideoQueueSize() << endl
    << "audio queue size: " << vidRecorder.getAudioQueueSize() << endl
    << "FPS: " << ofGetFrameRate() << endl
    << (bRecording?"pause":"start") << " recording: r" << endl
    << (bRecording?"close current video file: c":"") << endl;
    ofDrawBitmapString(ss.str(),15,15);
    if(bRecording){
	ofSetColor(255, 0, 0);
	ofDrawCircle(ofGetWidth() - 20, 20, 5);
    }

}


//--------------------------------------------------------------
void ofApp::audioIn(float *input, int bufferSize, int nChannels){
    if(bRecording)
        vidRecorder.addAudioSamples(input, bufferSize, nChannels);
}

//--------------------------------------------------------------
void ofApp::recordingComplete(ofxVideoRecorderOutputFileCompleteEventArgs& args){
    cout << "The recoded video file is now complete." << endl;
}



//--------------------------------------------------------------
void ofApp::keyPressed(int key){
    // in fullscreen mode, on a pc at least, the
    // first time video settings the come up
    // they come up *under* the fullscreen window
    // use alt-tab to navigate to the settings
    // window. we are working on a fix for this...
    
    // Video settings no longer works in 10.7
    // You'll need to compile with the 10.6 SDK for this
    // For Xcode 4.4 and greater, see this forum post on instructions on installing the SDK
    // http://forum.openframeworks.cc/index.php?topic=10343
    if(key == 's' || key == 'S'){
        vidGrabber.videoSettings();
    }
}

//--------------------------------------------------------------
void ofApp::keyReleased(int key){
    
    if(key=='r'){
        startRecording("Manual");
    }
    if(key=='c'){
        stopRecording();
    }

}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y ){

}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseEntered(int x, int y){

}

//--------------------------------------------------------------
void ofApp::mouseExited(int x, int y){

}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h){

}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo){ 

}
