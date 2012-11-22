#include "OsgMainApp.hpp"

OsgMainApp::OsgMainApp(){

    _initialized = false;

}
OsgMainApp::~OsgMainApp(){

}


//Initialization function
void OsgMainApp::initOsgWindow(int x,int y,int width,int height){

    __android_log_write(ANDROID_LOG_ERROR, "OSGANDROID",
            "Initializing geometry");
    //
    _scene = new DemoScene();
    _scene->init("", osg::Vec2(width, height), 0);

    _initialized = true;
}

//Draw
void OsgMainApp::draw(){

    _scene->frame();
    
    //clear events for next frame
    _frameTouchBeganEvents = NULL;
    _frameTouchMovedEvents = NULL;
    _frameTouchEndedEvents = NULL;
}
//Events
static bool flipy = true;
void OsgMainApp::touchBeganEvent(int touchid,float x,float y){
    if (!_frameTouchBeganEvents.valid()) {
        if(_scene->getViewer()){
            _frameTouchBeganEvents = _scene->getViewer()->getEventQueue()->touchBegan(touchid, osgGA::GUIEventAdapter::TOUCH_BEGAN, x, flipy ? _bufferHeight-y : y);
        }
    } else {
        _frameTouchBeganEvents->addTouchPoint(touchid, osgGA::GUIEventAdapter::TOUCH_BEGAN, x, flipy ? _bufferHeight-y : y);
    }
}
void OsgMainApp::touchMovedEvent(int touchid,float x,float y){
    if (!_frameTouchMovedEvents.valid()) {
        if(_scene->getViewer()){
            _frameTouchMovedEvents = _scene->getViewer()->getEventQueue()->touchMoved(touchid, osgGA::GUIEventAdapter::TOUCH_MOVED, x, flipy ? _bufferHeight-y : y);
        }
    } else {
        _frameTouchMovedEvents->addTouchPoint(touchid, osgGA::GUIEventAdapter::TOUCH_MOVED, x, flipy ? _bufferHeight-y : y);
    }
}
void OsgMainApp::touchEndedEvent(int touchid,float x,float y,int tapcount){
    if (!_frameTouchEndedEvents.valid()) {
        if(_scene->getViewer()){
            _frameTouchEndedEvents = _scene->getViewer()->getEventQueue()->touchEnded(touchid, osgGA::GUIEventAdapter::TOUCH_ENDED, x, flipy ? _bufferHeight-y : y,tapcount);
        }
    } else {
        _frameTouchEndedEvents->addTouchPoint(touchid, osgGA::GUIEventAdapter::TOUCH_ENDED, x, flipy ? _bufferHeight-y : y,tapcount);
    }
}
void OsgMainApp::keyboardDown(int key){
    _scene->getViewer()->getEventQueue()->keyPress(key);
}
void OsgMainApp::keyboardUp(int key){
    _scene->getViewer()->getEventQueue()->keyRelease(key);
}

void OsgMainApp::clearEventQueue()
{
    //clear our groups
    _frameTouchBeganEvents = NULL;
    _frameTouchMovedEvents = NULL;
    _frameTouchEndedEvents = NULL;
    
    //clear the viewers queue
    _scene->getViewer()->getEventQueue()->clear();
}
