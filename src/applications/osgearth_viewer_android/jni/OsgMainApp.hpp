#ifndef OSGMAINAPP_HPP_
#define OSGMAINAPP_HPP_

//Android log
#include <android/log.h>
#include <iostream>
#include <cstdlib>
#include <math.h>

//Standard libraries
#include <string>

//osg
#include <osgViewer/Viewer>

//osgearth
#include <osgEarth/Viewpoint>
#include <osgEarthUtil/EarthManipulator>
#include <osgEarthUtil/AutoClipPlaneHandler>
#include <osgEarthUtil/ObjectLocator>
#include <osgEarthUtil/SkyNode>
#include <osgEarthUtil/EarthManipulator>
#include <osgEarthUtil/ExampleResources>

#include "OsgAndroidNotifyHandler.hpp"
#include "EarthMultiTouchManipulator.h"

//Static plugins Macro
#include "osgPlugins.h"


#define  LOG_TAG    "osgNativeLib"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)



class OsgMainApp{
private:
    osg::ref_ptr<osgViewer::Viewer> _viewer;
    bool _initialized;
    
    OsgAndroidNotifyHandler *_notifyHandler;

public:
    OsgMainApp();
    ~OsgMainApp();

    //Initialization function
    void initOsgWindow(int x,int y,int width,int height);
    //Draw
    void draw();
    //Events
    void mouseButtonPressEvent(float x,float y,int button);
    void mouseButtonReleaseEvent(float x,float y,int button);
    void mouseMoveEvent(float x,float y);
    void keyboardDown(int key);
    void keyboardUp(int key);

};


#endif /* OSGMAINAPP_HPP_ */
