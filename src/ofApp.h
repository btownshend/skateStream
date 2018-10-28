#pragma once

#include <cstdio>
#include "ofMain.h"
#include "ofxNetwork.h"
#include "ofxGui.h"
#include "ofxVideoRecorder.h"
#include "ofxJSON.h"
#include "ofxAssimpModelLoader.h"

// This openFrameworks example is designed to demonstrate how to access the
// webcam.
//
// For more information regarding this example take a look at the README.md.

class ofApp : public ofBaseApp{
    
public:
    
    void setup();
    void update();
    void draw();
    void exit();

    void keyPressed(int key);
    void keyReleased(int key);
    void mouseMoved(int x, int y);
    void mouseDragged(int x, int y, int button);
    void mousePressed(int x, int y, int button);
    void mouseReleased(int x, int y, int button);
    void mouseEntered(int x, int y);
    void mouseExited(int x, int y);
    void windowResized(int w, int h);
    void dragEvent(ofDragInfo dragInfo);
    void gotMessage(ofMessage msg);
    void audioIn(float * input, int bufferSize, int nChannels);
    void startRecording(string tag);
    void stopRecording();
    void loadFiles();
    void setSource(int i);

    // Web cam acquistion
    ofVideoGrabber vidGrabber;
    ofPixels videoInverted;
    ofTexture videoTexture;
    int camWidth;
    int camHeight;
    ofxUDPManager udpConnection;
    static const int portNo = 10552;

    // Video player
    ofVideoPlayer masterPlayer, studentPlayer;
    
    // Video file writer
    ofxVideoRecorder    vidRecorder;
    ofSoundStream       soundStream;
    bool bRecording;
    int sampleRate;
    int channels;

    void recordingComplete(ofxVideoRecorderOutputFileCompleteEventArgs& args);

    ofPixels recordPixels;

    FILE *imuFD=NULL;
    
    // GUI
    ofParameter<float> scrubber;
    ofxPanel gui;
    
    // Setup data
    vector <ofxJSONElement> data;
    
    // 3d model of skateboard
    ofxAssimpModelLoader skateboard;
};
