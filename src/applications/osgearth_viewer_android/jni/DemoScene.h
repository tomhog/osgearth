//
//  DemoScene.h
//  osgearthDemos
//
//

#pragma once

#include "osgPlugins.h"
#include <osgViewer/Viewer>

//
#include <osgDB/FileUtils>
#include "GLES2ShaderGenVisitor.h"
//

#ifndef ANDROID
#import <UIKit/UIKit.h>//to acces app delegate
#else
typedef void UIView;
#endif

class DemoScene : public osg::Referenced
{
public:
    DemoScene();
    
    void init(const std::string& file, osg::Vec2 viewSize, UIView* view=0);
    
    void frame();
    
    //return the view
    osgViewer::Viewer* getViewer(){
        return _viewer.get();
    }
    
    void setDataPath(std::string dataPath, std::string packagePath);
    
protected:
    virtual ~DemoScene();
    
    //
    // implemented by new file in each target so we can recycle
    // all the base code for each app
    void initDemo(const std::string& file);
    
protected:
    
    osg::ref_ptr<osgViewer::Viewer> _viewer;
    
    //paths for android
    std::string _dataPath;
    std::string _packagePath;

};


