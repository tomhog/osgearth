/* -*-c++-*- */
/* osgEarth - Dynamic map generation toolkit for OpenSceneGraph
 * Copyright 2008-2013 Pelican Mapping
 * http://osgearth.org
 *
 * osgEarth is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 */
// 2014-2015 GAJ Geospatial Enterprises, Orlando FL
// Created FeatureSourceCDB for Incorporation of Common Database (CDB) support within osgEarth
// 2016-2017 Visual Awareness Technologies and Consulting Inc. St Petersburg FL

#include "CDBFeatureOptions"
#include <CDB_TileLib/CDB_Tile>

#include <osgEarth/Version>
#include <osgEarth/Registry>
#include <osgEarth/XmlUtils>
#include <osgEarth/FileUtils>
#include <osgEarthFeatures/FeatureSource>
#include <osgEarthFeatures/Filter>
#include <osgEarthFeatures/BufferFilter>
#include <osgEarthFeatures/ScaleFilter>
#include <osgEarthFeatures/OgrUtils>
#include <osgEarthUtil/TFS>
#include <osg/Notify>
#include <osgDB/FileNameUtils>
#include <osgDB/FileUtils>
#include <osgDB/Archive>
#include <list>
#include <vector>
#include <stdio.h>
#include <stdlib.h>

#ifdef _MSC_VER
#if _MSC_VER < 1800
#define round osg::round
#endif
#endif

#include <ogr_api.h>
#include <ogr_core.h>
#include <ogrsf_frmts.h>

#ifdef WIN32
#include <windows.h>
#endif

#define LC "[CDB FeatureSource] "

#if 0
#ifdef _DEBUG
#define _SAVE_OGR_OUTPUT
#endif
#endif

using namespace osgEarth;
using namespace osgEarth::Util;
using namespace osgEarth::Features;
using namespace osgEarth::Drivers;

#define OGR_SCOPED_LOCK GDAL_SCOPED_LOCK


struct CDBFeatureEntryData {
	int	CDBLod;
	std::string FullReferenceName;
};

struct CDBUnReffedFeatureEntryData {
	int CDBLod;
	std::string ModelZipName;
	std::string TextureZipName;
	std::string ArchiveFileName;
};

typedef CDBFeatureEntryData CDBFeatureEntry;
typedef CDBUnReffedFeatureEntryData CDBUnrefEntry;
typedef std::vector<CDBFeatureEntry> CDBFeatureEntryVec;
typedef std::vector<CDBUnrefEntry> CDBUnrefEntryVec;
typedef std::map<std::string, CDBFeatureEntryVec> CDBEntryMap;
typedef std::map<std::string, CDBUnrefEntryVec> CDBUnrefEntryMap;

static CDBEntryMap				_CDBInstances;
static CDBUnrefEntryMap			_CDBUnReffedInstances;

static __int64 _s_CDB_FeatureID = 0;
/**
 * A FeatureSource that reads Common Database Layers
 * 
 */
class CDBFeatureSource : public FeatureSource
{
public:
    CDBFeatureSource(const CDBFeatureOptions& options ) :
      FeatureSource( options ),
      _options     ( options ),
	  _CDB_inflated (false),
	  _CDB_geoTypical(false),
	  _CDB_GS_uses_GTtex(false),
	  _CDB_No_Second_Ref(true),
	  _CDB_Edit_Support(false),
	  _GT_LOD0_FullStack(false),
	  _GS_LOD0_FullStack(false),
	  _cur_Feature_Cnt(0),
	  _rootString(""),
	  _cacheDir(""),
	  _dataSet("_S001_T001_"),
	  _CDBLodNum(0),
	  _BE_Verbose(false),
	  _M_Contains_ABS_Z(false)
#ifdef _SAVE_OGR_OUTPUT
	,_OGR_Output(NULL),
	_OGR_OutputName("C:\\Temp\\GeoSpecificModelCapture.gpkg"),
	_OGR_OutputDriver("GPKG"),
	_OGR_OutputLayerName("GeoSpecificModelData"),
	_OGR_OutputLayer(NULL)
#endif
	{
    }

    /** Destruct the object, cleaning up and OGR handles. */
    virtual ~CDBFeatureSource()
    {               
#ifdef _SAVE_OGR_OUTPUT
		if (_OGR_Output)
			_OGR_Output->Close_File();
#endif
		//nop
    }

    //override
	Status initialize(const osgDB::Options* dbOptions)
	{
		_dbOptions = dbOptions ? osg::clone(dbOptions) : 0L;

		//		osgEarth::CachePolicy::NO_CACHE.apply(_dbOptions.get());
				//ToDo when working reenable  the cache disable for development 
#ifdef _SAVE_OGR_OUTPUT
		bool geospecific = true;
		if (_options.geoTypical().isSet())
		{
			if (_options.geoTypical().value())
			{
				geospecific = false;
			}
		}
		if (geospecific)
		{
			_OGR_Output = OGR_File::GetInstance();
			_OGR_Output->SetName_and_Driver(_OGR_OutputName, _OGR_OutputDriver);
			GDALDataset *poDataSet = _OGR_Output->Open_Output();
			if (!poDataSet)
			{
				OE_WARN << "Unable to open output GeoPackage file" << std::endl;
			}
		}
#endif

		FeatureProfile* Feature_Profile = NULL;

		const Profile * CDBFeatureProfile = NULL;
		if (_options.inflated().isSet())
			_CDB_inflated = _options.inflated().value();
		if (_options.GS_uses_GTtex().isSet())
			_CDB_GS_uses_GTtex = _options.GS_uses_GTtex().value();
		if (_options.Edit_Support().isSet())
			_CDB_Edit_Support = _options.Edit_Support().value();
		if (_options.No_Second_Ref().isSet())
			_CDB_No_Second_Ref = _options.No_Second_Ref().value();
		if (_options.GT_LOD0_FullStack().isSet())
			_GT_LOD0_FullStack = _options.GT_LOD0_FullStack().value();
		if (_options.GS_LOD0_FullStack().isSet())
			_GS_LOD0_FullStack = _options.GS_LOD0_FullStack().value();
		if (_options.geoTypical().isSet())
		{
			_CDB_geoTypical = _options.geoTypical().value();
			if (_CDB_geoTypical)
			{
				if (!_CDB_inflated)
				{
					OE_WARN << "GeoTypical option was set without CDB_Inflated: Forcing Inflated" << std::endl;
					_CDB_inflated = true;
				}
			}
		}
		if (_options.Verbose().isSet())
		{
			bool verbose = _options.Verbose().value();
			if (verbose)
				_BE_Verbose = true;
		}
		if (_options.ABS_Z_in_M().isSet())
		{
			bool z_in_m = _options.ABS_Z_in_M().value();
			if (z_in_m)
				_M_Contains_ABS_Z = true;
		}

		if (_options.Limits().isSet())
		{
			std::string cdbLimits = _options.Limits().value();
			double	min_lon,
				max_lon,
				min_lat,
				max_lat;

			int count = sscanf(cdbLimits.c_str(), "%lf,%lf,%lf,%lf", &min_lon, &min_lat, &max_lon, &max_lat);
			if (count == 4)
			{
				//CDB tiles always filter to geocell boundaries
				min_lon = round(min_lon);
				min_lat = round(min_lat);
				max_lat = round(max_lat);
				max_lon = round(max_lon);
				if ((max_lon > min_lon) && (max_lat > min_lat))
				{
					unsigned tiles_x = (unsigned)(max_lon - min_lon);
					unsigned tiles_y = (unsigned)(max_lat - min_lat);
					osg::ref_ptr<const SpatialReference> src_srs;
					src_srs = SpatialReference::create("EPSG:4326");
					CDBFeatureProfile = osgEarth::Profile::create(src_srs, min_lon, min_lat, max_lon, max_lat, tiles_x, tiles_y);

					//			   Below works but same as no limits
					//			   setProfile(osgEarth::Profile::create(src_srs, -180.0, -90.0, 180.0, 90.0, min_lon, min_lat, max_lon, max_lat, 90U, 45U));
				}
			}
			if (!CDBFeatureProfile)
				OE_WARN << "Invalid Limits received by CDB Driver: Not using Limits" << std::endl;

		}
		int minLod, maxLod;
		if (_options.minLod().isSet())
			minLod = _options.minLod().value();
		else
			minLod = 2;

		if (_options.maxLod().isSet())
		{
			maxLod = _options.maxLod().value();
			if (maxLod < minLod)
				minLod = maxLod;
		}
		else
			maxLod = minLod;

		// Always a WGS84 unprojected lat/lon profile.
		if (!CDBFeatureProfile)
			CDBFeatureProfile = osgEarth::Profile::create("EPSG:4326", "", 90U, 45U);

		Feature_Profile = new FeatureProfile(CDBFeatureProfile->getExtent());
		Feature_Profile->setTiled(true);
		// Should work for now 
		Feature_Profile->setFirstLevel(minLod);
		Feature_Profile->setMaxLevel(maxLod);
		Feature_Profile->setProfile(CDBFeatureProfile);

		// Make sure the root directory is set
		if (!_options.rootDir().isSet())
		{
			OE_WARN << "CDB root directory not set!" << std::endl;
		}
		else
		{
			_rootString = _options.rootDir().value();
		}

		bool errorset = false;
		std::string Errormsg = "";

		//Find a jpeg2000 driver for the image layer.
		if (!CDB_Tile::Initialize_Tile_Drivers(Errormsg))
		{
			errorset = true;
		}

		if (_GT_LOD0_FullStack)
		{
			if (_CDB_geoTypical)
				CDB_Tile::Set_LOD0_GT_Stack(true);
		}

		if (_GS_LOD0_FullStack)
		{
			if (!_CDB_geoTypical)
				CDB_Tile::Set_LOD0_GS_Stack(true);
		}

		if (Feature_Profile)
		{
			setFeatureProfile(Feature_Profile);
		}
		else
		{
			return Status::Error(Status::ResourceUnavailable, "CDBFeatureSource Failed to establish a valid feature profile");
		}
		return Status::OK();
    }

	



    FeatureCursor* createFeatureCursor( const Symbology::Query& query )
    {
        FeatureCursor* result = 0L;
		_cur_Feature_Cnt = 0;
		// Make sure the root directory is set
		if (!_options.rootDir().isSet())
		{
			OE_WARN << "CDB root directory not set!" << std::endl;
			return result;
		}
		const osgEarth::TileKey key = query.tileKey().get();
		const GeoExtent key_extent = key.getExtent();
		CDB_Tile_Type tiletype;
		if (_CDB_geoTypical)
			tiletype = GeoTypicalModel;
		else
			tiletype = GeoSpecificModel;
		CDB_Tile_Extent tileExtent(key_extent.north(), key_extent.south(), key_extent.east(), key_extent.west());
		CDB_Tile *mainTile = NULL;
		bool subtile = false;
		if (CDB_Tile::Get_Lon_Step(tileExtent.South) == 1.0)
		{
			mainTile = new CDB_Tile(_rootString, _cacheDir, tiletype, _dataSet, &tileExtent, false, false, false);
		}
		else
		{
			CDB_Tile_Extent  CDBTile_Tile_Extent = CDB_Tile::Actual_Extent_For_Tile(tileExtent);
			mainTile = new CDB_Tile(_rootString, _cacheDir, tiletype, _dataSet, &CDBTile_Tile_Extent, false, false, false);
			mainTile->Set_SpatialFilter_Extent(tileExtent);
			subtile = true;
			if (_BE_Verbose)
			{
				printf("Sourcetile: North %lf South %lf East %lf West %lf \n", CDBTile_Tile_Extent.North, CDBTile_Tile_Extent.South,
					CDBTile_Tile_Extent.East, CDBTile_Tile_Extent.West);
			}
		}
		_CDBLodNum = mainTile->CDB_LOD_Num();
		if (_BE_Verbose)
		{
			printf("CDB Feature Cursor called with CDB LOD %d Tile\n", _CDBLodNum);
		}
		int Files2check = mainTile->Model_Sel_Count();
		std::string base;
		int FilesChecked = 0;
		bool dataOK = false;

		FeatureList features;
		bool have_a_file = false;
		if (Files2check > 0)
		{
			base = mainTile->FileName(FilesChecked);
			// check the blacklist:
			if (Registry::instance()->isBlacklisted(base))
			{
				Files2check = 0;
				if (_BE_Verbose)
				{
					printf("Tile %s is blacklisted\n", base.c_str());
				}
			}
		}

		while (FilesChecked < Files2check)
		{
			bool have_file = mainTile->Init_Model_Tile(FilesChecked);

			OE_DEBUG << query.tileKey().get().str() << "=" << base << std::endl;

			if (have_file)
			{
				if (_BE_Verbose)
				{
					if (_CDB_geoTypical)
					{
						printf("Feature tile loding GeoTypical Tile %s\n", base.c_str());
						if (subtile)
							printf("Subtile: North %lf South %lf East %lf West %lf \n", tileExtent.North, tileExtent.South,
								tileExtent.East, tileExtent.West);
					}
					else
					{
						printf("Feature tile loding GeoSpecific Tile %s\n", base.c_str());
						if (subtile)
							printf("Subtile: North %lf South %lf East %lf West %lf \n", tileExtent.North, tileExtent.South,
								tileExtent.East, tileExtent.West);
					}
				}
				bool fileOk = getFeatures(mainTile, base, features, FilesChecked);
				if (fileOk)
				{
					if (_BE_Verbose)
					{
						printf("File %s has %d Features\n", base.c_str(), (int)features.size());
					}
					OE_INFO << LC << "Features " << features.size() << base << std::endl;
					have_a_file = true;
				}

				if (fileOk)
					dataOK = true;
				else
					Registry::instance()->blacklist(base);
			}
			++FilesChecked;
		}

		if (!have_a_file)
		{
			if(Files2check > 0)
				Registry::instance()->blacklist(base);
		}

		delete mainTile;

		result = dataOK ? new FeatureListCursor( features ) : 0L;

        return result;
    }

    /**
    * Gets the Feature with the given FID
    * @returns
    *     The Feature with the given FID or NULL if not found.
    */
    virtual Feature* getFeature( FeatureID fid )
    {
        return 0;
    }

    virtual bool isWritable() const
    {
        return false;
    }

    virtual const FeatureSchema& getSchema() const
    {
        //TODO:  Populate the schema from the DescribeFeatureType call
        return _schema;
    }

    virtual osgEarth::Symbology::Geometry::Type getGeometryType() const
    {
        return Geometry::TYPE_UNKNOWN;
    }

private:


	bool getFeatures(CDB_Tile *mainTile, const std::string& buffer, FeatureList& features, int sel)
	{
		// find the right driver for the given mime type
		OGR_SCOPED_LOCK;
		// find the right driver for the given mime type
		bool have_archive = false;
		bool have_texture_zipfile = false;
#ifdef _DEBUG
		int fubar = 0;
#endif
		std::string TileNameStr;
		if (_CDB_Edit_Support)
		{
			TileNameStr = osgDB::getSimpleFileName(buffer);
			TileNameStr = osgDB::getNameLessExtension(TileNameStr);
		}

		const SpatialReference* srs = SpatialReference::create("EPSG:4326");

		osg::ref_ptr<osgDB::Options> localoptions = _dbOptions->cloneOptions();
		std::string ModelTextureDir = "";
		std::string ModelZipFile = "";
		std::string TextureZipFile = "";
		std::string ModelZipDir = "";
		if (_CDB_inflated)
		{
			if (!_CDB_geoTypical)
			{
				if (!mainTile->Model_Texture_Directory(ModelTextureDir))
					return false;
			}
		}
		else
		{
			if (!_CDB_geoTypical)
			{
				have_archive = mainTile->Model_Geometry_Name(ModelZipFile);
				if (!have_archive)
					return false;
				have_texture_zipfile = mainTile->Model_Texture_Archive(TextureZipFile);
			}
		}
		if (!_CDB_geoTypical)
			ModelZipDir = mainTile->Model_ZipDir();

		bool done = false;
		while (!done)
		{
			OGRFeature * feat_handle;
			std::string FullModelName;
			std::string ArchiveFileName;
			std::string ModelKeyName;
			bool Model_in_Archive = false;
			bool valid_model = true;
			feat_handle = mainTile->Next_Valid_Feature(sel, _CDB_inflated, ModelKeyName, FullModelName, ArchiveFileName, Model_in_Archive);
			if (feat_handle == NULL)
			{
				done = true;
				break;
			}
			if (!Model_in_Archive)
				valid_model = false;

			double ZoffsetPos = 0.0;
			if (_M_Contains_ABS_Z)
			{
				OGRGeometry *geo = feat_handle->GetGeometryRef();
				if (wkbFlatten(geo->getGeometryType()) == wkbPoint)
				{
					OGRPoint * poPoint = (OGRPoint *)geo;
					double Mpos = poPoint->getM();
					ZoffsetPos = poPoint->getZ(); //Used as altitude offset
					poPoint->setZ(Mpos+ ZoffsetPos);
						
				}
			}

#if OSGEARTH_VERSION_GREATER_OR_EQUAL (2,7,0)
			osg::ref_ptr<Feature> f = OgrUtils::createFeature((OGRFeatureH)feat_handle, getFeatureProfile());
#else
			osg::ref_ptr<Feature> f = OgrUtils::createFeature(feat_handle, srs);
#endif
			f->setFID(_s_CDB_FeatureID);
			++_s_CDB_FeatureID;

			f->set("osge_basename", ModelKeyName);
#ifdef _DEBUG
			int dbgpos = ModelKeyName.find("Ambulance");
			if (dbgpos != std::string::npos)
			{
				++fubar;
			}
#endif
			if (_CDB_Edit_Support)
			{
				std::stringstream format_stream;
				format_stream << TileNameStr << "_" << std::setfill('0')
					<< std::setw(5) << abs(_cur_Feature_Cnt);

				f->set("name", ModelKeyName);
				std::string transformName = "xform_" + format_stream.str();
				f->set("transformname", transformName);
				std::string mtypevalue;
				if (_CDB_geoTypical)
					mtypevalue = "geotypical";
				else
					mtypevalue = "geospecific";
				f->set("modeltype", mtypevalue);
				f->set("tilename", buffer);
				if (!_CDB_geoTypical)
					f->set("selection", sel);
				else
					f->set("selection", mainTile->Realsel(sel));

				CDB_Model_Runtime_Class FeatureClass = mainTile->Current_Feature_Class_Data();
				f->set("bsr", FeatureClass.bsr);
				f->set("bbw", FeatureClass.bbw);
				f->set("bbl", FeatureClass.bbl);
				f->set("bbh", FeatureClass.bbh);
				f->set("zoffset", ZoffsetPos);

			}
			++_cur_Feature_Cnt;
			if (!_CDB_inflated)
			{
				f->set("osge_modelzip", ModelZipFile);
			}//end else !cdb_inflated
			if (valid_model)
			{
				if (_CDB_geoTypical)
				{
					if (_CDB_No_Second_Ref)
					{
						if (f->hasAttr("inst"))
						{
							int instanceType = f->getInt("inst");
							if (instanceType == 1)
							{
								valid_model = false;
							}
						}

					}
				}
			}
			if (valid_model)
			{
				//Ok we have everthing needed to load this model at this lod
				//Set the atribution to tell osgearth to load the model
				if (have_archive)
				{
					//Normal CDB path
					f->set("osge_modelname", ArchiveFileName);
					if(have_texture_zipfile)
						f->set("osge_texturezip", TextureZipFile);
					f->set("osge_gs_uses_gt", ModelZipDir);
					std::string referencedName;
					bool instanced = false;
					int LOD = 0;
					if (find_PreInstance(ModelKeyName, referencedName, instanced, LOD))
					{
						if(LOD < _CDBLodNum)
							f->set("osge_referencedName", referencedName);
					}
				}
				else
				{
					//GeoTypical or CDB database in development path
					f->set("osge_modelname", FullModelName);
					if (!_CDB_geoTypical)
						f->set("osge_modeltexture", ModelTextureDir);
				}
#ifdef _DEBUG
				OE_DEBUG << LC << "Model File " << FullModelName << " Set to Load" << std::endl;
#endif
			}
			else
			{
				if (!_CDB_geoTypical)
				{
					//The model does not exist at this lod. It should have been loaded previously
					//Look up the exact name used when creating the model at the lower lod
					std::string referencedName;
					bool instanced = false;
					int LOD;
					if (find_PreInstance(ModelKeyName, referencedName, instanced, LOD))
					{
						f->set("osge_modelname", referencedName);
						if (_CDB_No_Second_Ref)
							valid_model = false;
					}
					else
					{
						if (instanced)
						{
							valid_model = false;
						}
						else
						{
							if (have_archive)
							{
								bool urefinstance = false;
								if (find_UnRefInstance(ModelKeyName, ModelZipFile, ArchiveFileName, TextureZipFile, urefinstance))
								{
									//Set the attribution for osgearth to load the previously unreference model
									//Normal CDB path
									valid_model = true;
									//A little paranoid verification
									if (!validate_name(ModelZipFile))
									{
										valid_model = false;
									}
									else
										f->set("osge_modelzip", ModelZipFile);

									f->set("osge_modelname", ArchiveFileName);

									if (!_CDB_GS_uses_GTtex)
									{
										have_texture_zipfile = true;
										if (!TextureZipFile.empty())
										{
											if (!validate_name(TextureZipFile))
												have_texture_zipfile = false;
											else
												f->set("osge_texturezip", TextureZipFile);
										}
										else
											have_texture_zipfile = false;
									}
									else
										f->set("osge_gs_uses_gt", ModelZipDir);
								}
								else
								{
									//Its toast and will be a red flag in the database
									OE_INFO << LC << "Model File " << FullModelName << " not found in archive" << std::endl;
								}
							}
							else
							{
								//Its toast and will be a red flag in the database
								OE_INFO << LC << " GeoTypical Model File " << FullModelName << " not found " << std::endl;
							}
						}
					}
				}
			}
			if (f.valid() && !isBlacklisted(f->getFID()))
			{
				if (valid_model && !_CDB_geoTypical)
				{
					//We need to record this instance so that this model reference can be found when referenced in 
					//higher lods. In order for osgearth to find the model we must have the exact model name that was used
					//in either a filename or archive reference
					CDBEntryMap::iterator mi = _CDBInstances.find(ModelKeyName);
					CDBFeatureEntry NewCDBEntry;
					NewCDBEntry.CDBLod = _CDBLodNum;
					if (have_archive)
						NewCDBEntry.FullReferenceName = ArchiveFileName;
					else
						NewCDBEntry.FullReferenceName = FullModelName;

					if (mi == _CDBInstances.end())
					{
						CDBFeatureEntryVec NewCDBEntryMap;
						NewCDBEntryMap.push_back(NewCDBEntry);
						_CDBInstances.insert(std::pair<std::string, CDBFeatureEntryVec>(ModelKeyName, NewCDBEntryMap));
					}
					else
					{
						CDBFeatureEntryVec CurentCDBEntryMap = _CDBInstances[ModelKeyName];
						bool can_insert = true;
						for (CDBFeatureEntryVec::iterator vi = CurentCDBEntryMap.begin(); vi != CurentCDBEntryMap.end(); ++vi)
						{
							if (vi->CDBLod == _CDBLodNum)
							{
								can_insert = false;
								break;
							}
						}
						if (can_insert)
							_CDBInstances[ModelKeyName].push_back(NewCDBEntry);
					}
				}
//test
				if (valid_model)
				{
#ifdef _SAVE_OGR_OUTPUT
					if (_OGR_Output)
					{
						if (!_OGR_OutputLayer)
						{
							_OGR_OutputLayer = _OGR_Output->Get_Or_Create_Layer(_OGR_OutputLayerName, f);
						}
						if (_OGR_OutputLayer)
						{
							_OGR_Output->Add_Feature_to_Layer(_OGR_OutputLayer, f);
						}
					}
#endif
					features.push_back(f.release());
				}
				else
					f.release();
			}
			OGR_F_Destroy(feat_handle);
		}
		if (have_archive)
		{
			//Verify all models in the archive have been referenced
			//If not store them in unreferenced
			std::string Header = mainTile->Model_HeaderName();
			osgDB::Archive::FileNameList * archiveFileList = mainTile->Model_Archive_List();

			for (osgDB::Archive::FileNameList::const_iterator f = archiveFileList->begin(); f != archiveFileList->end(); ++f)
			{
				const std::string archiveFileName = *f;
				std::string KeyName = mainTile->Model_KeyNameFromArchiveName(archiveFileName, Header);
				if (!KeyName.empty())
				{
					CDBEntryMap::iterator mi = _CDBInstances.find(KeyName);
					if (mi == _CDBInstances.end())
					{
						//The model is not in our refernced models so add it to the unreferenced list
						//so we can find it later when it is referenced.
						//This really shouldn't happen and perhaps we will make this an option to speed things 
						//up in the future but there are unfortunatly published datasets with this condition
						//Colorodo Springs is and example
						CDBUnrefEntryMap::iterator ui = _CDBUnReffedInstances.find(KeyName);
						CDBUnrefEntry NewCDBUnRefEntry;
						NewCDBUnRefEntry.CDBLod = _CDBLodNum;
						NewCDBUnRefEntry.ArchiveFileName = archiveFileName;
						NewCDBUnRefEntry.ModelZipName = ModelZipFile;
						NewCDBUnRefEntry.TextureZipName = TextureZipFile;
						if (ui == _CDBUnReffedInstances.end())
						{
							CDBUnrefEntryVec NewCDBUnRefEntryMap;
							NewCDBUnRefEntryMap.push_back(NewCDBUnRefEntry);
							_CDBUnReffedInstances.insert(std::pair<std::string, CDBUnrefEntryVec>(KeyName, NewCDBUnRefEntryMap));
						}
						else
						{
							CDBUnrefEntryVec CurentCDBUnRefEntryMap = _CDBUnReffedInstances[KeyName];
							bool can_insert = true;
							for (CDBUnrefEntryVec::iterator vi = CurentCDBUnRefEntryMap.begin(); vi != CurentCDBUnRefEntryMap.end(); ++vi)
							{
								if (vi->CDBLod == _CDBLodNum)
								{
									can_insert = false;
									break;
								}
							}
							if (can_insert)
								_CDBUnReffedInstances[KeyName].push_back(NewCDBUnRefEntry);
						}
					}
				}
			}
		}
		return true;
	}

	bool find_PreInstance(std::string &ModelKeyName, std::string &ModelReferenceName, bool &instanced, int &LOD)
	{
		//The model does not exist at this lod. It should have been loaded previously
		//Look up the exact name used when creating the model at the lower lod
		CDBEntryMap::iterator mi = _CDBInstances.find(ModelKeyName);
		if (mi != _CDBInstances.end())
		{
			//Ok we found the model select the best lod. It must be lower than our current lod
			//If the model is not found here then we will simply ignore the model until we get to an lod in which
			//we find the model. If we selected to start at an lod higher than 0 there will be quite a few models
			//that fall into this catagory
			instanced = true;
			CDBFeatureEntryVec CurentCDBEntryMap = _CDBInstances[ModelKeyName];
			bool have_lod = false;
			CDBFeatureEntryVec::iterator ci;
			int mind = 1000;
			for (CDBFeatureEntryVec::iterator vi = CurentCDBEntryMap.begin(); vi != CurentCDBEntryMap.end(); ++vi)
			{
				if (vi->CDBLod <= _CDBLodNum)
				{
					int cind = _CDBLodNum - vi->CDBLod;
					if (cind < mind)
					{
						LOD = vi->CDBLod;
						have_lod = true;
						ci = vi;
					}
				}
			}
			if (have_lod)
			{
				//Set the attribution for osgearth to find the referenced model
				ModelReferenceName = ci->FullReferenceName;
#ifdef _DEBUG
				OE_DEBUG << LC << "Model File " << ModelReferenceName << " referenced" << std::endl;
#endif
				return true;
			}
			else
			{
				OE_INFO << LC << "No Instance of " << ModelKeyName << " found to reference" << std::endl;
				return false;
			}
		}
		else
			instanced = false;
		return false;
	}

	bool find_UnRefInstance(std::string &ModelKeyName, std::string &ModelZipFile, std::string &ArchiveFileName, std::string &TextureZipFile, bool &instance)
	{
		//now check and see if it is an unrefernced model from a lower LOD
		CDBUnrefEntryMap::iterator ui = _CDBUnReffedInstances.find(ModelKeyName);
		if (ui != _CDBUnReffedInstances.end())
		{
			instance = true;
			//ok we found it here
			CDBUnrefEntryVec CurentCDBUnRefMap = _CDBUnReffedInstances[ModelKeyName];
			bool have_lod = false;
			CDBUnrefEntryVec::iterator ci;
			int mind = 1000;
			for (CDBUnrefEntryVec::iterator vi = CurentCDBUnRefMap.begin(); vi != CurentCDBUnRefMap.end(); ++vi)
			{
				if (vi->CDBLod <= _CDBLodNum)
				{
					int cind = _CDBLodNum - vi->CDBLod;
					if (cind < mind)
					{
						have_lod = true;
						ci = vi;
					}
				}
			}
			if (have_lod)
			{
				//Set the attribution for osgearth to load the previously unreference model
				//Normal CDB path
				ModelZipFile = ci->ModelZipName;
				ArchiveFileName = ci->ArchiveFileName;

				if (!_CDB_GS_uses_GTtex)
				{
					TextureZipFile = ci->TextureZipName;
				}
#ifdef _DEBUG
				OE_DEBUG << LC << "Previously unrefferenced Model File " << ci->ArchiveFileName << " set to load" << std::endl;
#endif
				//Ok the model is now set to load and will be added to the referenced list 
				//lets remove it from the unreferenced list
				CurentCDBUnRefMap.erase(ci);
				if (CurentCDBUnRefMap.size() == 0)
				{
					_CDBUnReffedInstances.erase(ui);
				}
				return true;
			}
			else
			{
				OE_INFO << LC << "No Instance of " << ModelKeyName << " found to reference" << std::endl;
				return false;
			}

		}
		else
			instance = false;

		return false;
	}

	bool validate_name(std::string &filename)
	{
#ifdef _WIN32
		DWORD ftyp = ::GetFileAttributes(filename.c_str());
		if (ftyp == INVALID_FILE_ATTRIBUTES)
		{
			DWORD error = ::GetLastError();
			if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
			{
				OE_DEBUG << LC << "Model File " << filename << " not found" << std::endl;
				return false;
			}
		}
		return true;
#else
		int ftyp = ::access(filename.c_str(), F_OK);
		if (ftyp == 0)
		{
			return  true;
		}
		else
		{
			return false;
		}
#endif
	}


	const CDBFeatureOptions         _options;
    FeatureSchema                   _schema;
	bool							_CDB_inflated;
	bool							_CDB_geoTypical;
	bool							_CDB_GS_uses_GTtex;
	bool							_CDB_No_Second_Ref;
	bool							_CDB_Edit_Support;
	bool							_GS_LOD0_FullStack;
	bool							_GT_LOD0_FullStack;
	bool							_BE_Verbose;
	bool							_M_Contains_ABS_Z;
    osg::ref_ptr<CacheBin>          _cacheBin;
    osg::ref_ptr<osgDB::Options>    _dbOptions;
	int								_CDBLodNum;
	std::string						_rootString;
	std::string						_cacheDir;
	std::string						_dataSet;
	int								_cur_Feature_Cnt;
#ifdef _SAVE_OGR_OUTPUT
	OGR_File *						_OGR_Output;
	std::string						_OGR_OutputName;
	std::string						_OGR_OutputDriver;
	std::string						_OGR_OutputLayerName;
	OGRLayer *						_OGR_OutputLayer;
#endif
};


class CDBFeatureSourceFactory : public FeatureSourceDriver
{
public:
    CDBFeatureSourceFactory()
    {
        supportsExtension( "osgearth_feature_cdb", "CDB feature driver for osgEarth" );
    }

    virtual const char* className()
    {
        return "CDB Feature Reader";
    }

    virtual ReadResult readObject(const std::string& file_name, const Options* options) const
    {
        if ( !acceptsExtension(osgDB::getLowerCaseFileExtension( file_name )))
            return ReadResult::FILE_NOT_HANDLED;

        return ReadResult( new CDBFeatureSource( getFeatureSourceOptions(options) ) );
    }
};

REGISTER_OSGPLUGIN(osgearth_feature_cdb, CDBFeatureSourceFactory)

