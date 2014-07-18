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
    
   _viewer->setCameraManipulator(new osgEarth::Util::EarthManipulator());

   std::string testFile = "gdal_tiff.earth";
   std::string pathToData = "";

   if(osgDB::Registry::instance()->getReaderWriterForExtension("zip"))
   {
	   //if we have the zip plugin load the assets from inside the apk (place the tests and data folders in you Android projects assets folder)
	   pathToData = _packagePath + "/assets";
   }else{
       //get the path to this devices downloads folder on the sdcard (place the tests and data folders into your sdcards downloads folder)
	   std::vector<std::string> downloadsPaths;
	   downloadsPaths.push_back("/storage/sdcard0/Download");//nexus7
	   downloadsPaths.push_back("/sdcard/Download");//Galaxy nexus
	   downloadsPaths.push_back("/mnt/sdcard/download");//Galaxy S2
	   for(unsigned int i=0; i<downloadsPaths.size(); i++)
	   {
		   if(osgDB::fileExists(downloadsPaths[i]))
		   {
			   pathToData = downloadsPaths[i];
			   break;
		   }

	   }
   }

   std::string testFilePath = pathToData + "/tests/" + testFile;

   OSG_ALWAYS << "Attempting to load osgearth test file '" << testFilePath << "'" << std::endl;
   osg::Node* node = osgDB::readNodeFile(testFilePath);

	/*osg::Node* node = osgDB::readNodeFile("/storage/sdcard0/Download/tests/gdal_tiff.earth");//nexus7
    if(!node) node = osgDB::readNodeFile("/sdcard/Download/tests/gdal_tiff.earth");//Galaxy nexus
    if(!node) node = osgDB::readNodeFile("/mnt/sdcard/download/tests/gdal_tiff.earth");//Galaxy S2
    */
    if ( !node )
    {
        OSG_ALWAYS << "Unable to load an earth file from the command line." << std::endl;
        return;
    }

    osg::ref_ptr<osgEarth::Util::MapNode> mapNode = osgEarth::Util::MapNode::findMapNode(node);
    if ( !mapNode.valid() )
    {
        OSG_ALWAYS << "Loaded scene graph does not contain a MapNode - aborting" << std::endl;
        return;
    }
    
    // warn about not having an earth manip
    osgEarth::Util::EarthManipulator* manip = dynamic_cast<osgEarth::Util::EarthManipulator*>(_viewer->getCameraManipulator());
    if ( manip == 0L )
    {
        OSG_ALWAYS << "Helper used before installing an EarthManipulator" << std::endl;
    }
    
    // a root node to hold everything:
    osg::Group* root = new osg::Group();
    //root->addChild( mapNode.get() );

    /*
    osg::Node* model = osgDB::readNodeFile("/storage/sdcard0/Download/data/tree.osg");
    osgUtil::GLES2ShaderGenVisitor shaderGen;
	model->accept(shaderGen);
    root->addChild(model);

    //_viewer->setCameraManipulator(  new osgGA::MultiTouchTrackballManipulator() );
	*/

    osg::Light* light = new osg::Light( 0 );
    light->setPosition( osg::Vec4(0, -1, 0, 0 ) );
    light->setAmbient( osg::Vec4(0.4f, 0.4f, 0.4f ,1.0) );
    light->setDiffuse( osg::Vec4(1,1,1,1) );
    light->setSpecular( osg::Vec4(0,0,0,1) );
    //root->getOrCreateStateSet()->setAttribute(light);
    //_viewer->setLight(light);
    
    //have to add these
    osg::Material* material = new osg::Material();
    material->setAmbient(osg::Material::FRONT, osg::Vec4(0.4,0.4,0.4,1.0));
    material->setDiffuse(osg::Material::FRONT, osg::Vec4(0.9,0.9,0.9,1.0));
    material->setSpecular(osg::Material::FRONT, osg::Vec4(0.4,0.4,0.4,1.0));
    //root->getOrCreateStateSet()->setAttribute(material);
    mapNode->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);
    
    double hours = 12.0f;
    float ambientBrightness = 0.4f;
    osgEarth::Util::SkyNode* sky = osgEarth::Util::SkyNode::create(mapNode);
    //sky->setAmbientBrightness( ambientBrightness );
    sky->setDateTime( DateTime(2011, 3, 6, hours) );
    sky->attach( _viewer, 0 );
    root->addChild( sky );
    sky->addChild(mapNode);
    
    //for some reason we have to do this as global stateset doesn't
    //appear to be in the statesetstack
    //root->getOrCreateStateSet()->setAttribute(_viewer->getLight());
    
    _viewer->setSceneData( root );

    _viewer->realize();
 
}
