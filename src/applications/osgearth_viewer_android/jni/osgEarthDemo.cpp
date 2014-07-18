//
//  osgEarthDemo.cpp
//  osgearthDemos
//

#include "DemoScene.h"

#include <osgEarth/MapNode>
#include <osgEarthUtil/Sky>
#include <osgEarthUtil/EarthManipulator>

#include <osgDB/ReadFile>
#include <osgDB/FileUtils>
#include <osgGA/MultiTouchTrackballManipulator>

#include "GLES2ShaderGenVisitor.h"

using namespace osgEarth;
using namespace osgEarth::Util;

//
//Implement init demo for this target
//
void DemoScene::initDemo(const std::string &file)
{
    OSG_ALWAYS << "osgEarthDemo: initDemo" << std::endl;
    
    bool loadFromApk = true;
    //currently gdal_tiff.earth and readymaps.earth don't load from apk, they will load from downloads folder
    //yahoo_maps.earth loads fine from both
    std::string testFile = "yahoo_maps.earth";
    std::string pathToData = "";

    if(loadFromApk && osgDB::Registry::instance()->getReaderWriterForExtension("zip"))
    {
    	//if we have the zip plugin load the assets from inside the apk (place the tests and data folders in you Android projects assets folder)
    	pathToData = _packagePath + "/assets";
    }else{
    	//get the path to this devices downloads folder on the sdcard (place the tests and data folders into your sdcards downloads folder)
    	std::vector<std::string> downloadsPaths;
    	downloadsPaths.push_back("/storage/sdcard0/Download");//nexus7
    	downloadsPaths.push_back("/sdcard/Download");//Galaxy nexus
    	downloadsPaths.push_back("/mnt/sdcard/download");//Galaxy S2
    	for(unsigned int i=0; i<downloadsPaths.size(); i++){
    		if(osgDB::fileExists(downloadsPaths[i])){
    			pathToData = downloadsPaths[i];
    			break;
    		}
    	}
    }

    std::string testFilePath = pathToData + "/tests/" + testFile;
    osg::Node* node = osgDB::readNodeFile(testFilePath);
    if ( !node )
    {
        OSG_ALWAYS << "Unable to load an earth file '" << testFilePath << "'" << std::endl;
        return;
    }

    osg::ref_ptr<osgEarth::Util::MapNode> mapNode = osgEarth::Util::MapNode::findMapNode(node);
    if ( !mapNode.valid() )
    {
        OSG_ALWAYS << "Loaded scene graph does not contain a MapNode - aborting" << std::endl;
        return;
    }
    
    //install earth manipulator
    _viewer->setCameraManipulator(new osgEarth::Util::EarthManipulator());
    
    double hours = 12.0f;
    float ambientBrightness = 0.4f;
    osgEarth::Util::SkyNode* sky = osgEarth::Util::SkyNode::create(mapNode);
    //sky->setAmbientBrightness( ambientBrightness );
    sky->setDateTime( DateTime(2011, 3, 6, hours) );
    sky->attach( _viewer, 0 );
    sky->addChild(mapNode);

    // a root node to hold everything:
    osg::Group* root = new osg::Group();
    root->addChild( sky );
    
    
    _viewer->setSceneData( root );

    _viewer->realize();
 
}
