/* -*-c++-*- */
/* osgEarth - Dynamic map generation toolkit for OpenSceneGraph
 * Copyright 2008-2010 Pelican Mapping
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
#include <osgEarthFeatures/FeatureSourceNode>
#include <algorithm>

using namespace osgEarth;
using namespace osgEarth::Features;

#define LC "[FeatureSourceNode] "

// for testing:
#undef  OE_DEBUG
#define OE_DEBUG OE_INFO

//-----------------------------------------------------------------------------

FeatureSourceMultiNode::Collect::Collect( FeatureIDDrawSetMap& index ) :
osg::NodeVisitor( osg::NodeVisitor::TRAVERSE_ALL_CHILDREN ),
_index          ( index ),
_psets          ( 0 )
{
    _index.clear();
}

void
FeatureSourceMultiNode::Collect::apply( osg::Geode& geode )
{
    for( unsigned i = 0; i < geode.getNumDrawables(); ++i )
    {
        osg::Geometry* geom = dynamic_cast<osg::Geometry*>( geode.getDrawable(i) );
        if ( geom )
        {
            osg::Geometry::PrimitiveSetList& psets = geom->getPrimitiveSetList();
            for( unsigned p = 0; p < psets.size(); ++p )
            {
                osg::PrimitiveSet* pset = psets[p];
                RefFeatureID* fid = dynamic_cast<RefFeatureID*>( pset->getUserData() );
                if ( fid )
                {
                    FeatureDrawSet& drawSet = _index[*fid];
                    drawSet[geom].push_back( pset );
                    _psets++;
                }
            }
        }
    }
    traverse( geode );
}

//-----------------------------------------------------------------------------


FeatureSourceNode::FeatureSourceNode(FeatureSource * featureSource, FeatureID fid)
	: _featureSource(featureSource), _fid(fid)
{
}


FeatureSourceMultiNode::FeatureSourceMultiNode(FeatureSource * featureSource)
	: FeatureSourceNode(featureSource)
{
}


// Rebuilds the feature index based on all the tagged primitive sets found in a graph
void
FeatureSourceMultiNode::reindex( osg::Node* graph )
{
    _drawSets.clear();
    if ( graph )
    {
        Collect c(_drawSets);
        graph->accept( c );

        OE_DEBUG << LC 
            << "Reindexed, drawables = " << _drawSets.size()
            << ", total prim sets = " << c._psets << std::endl;
    }
}


// Tags all the primitive sets in a Drawable with the specified FeatureID
void
FeatureSourceMultiNode::tagPrimitiveSets(osg::Drawable* drawable, FeatureID fid) const
{
    if ( drawable == 0L )
        return;

    osg::Geometry* geom = drawable->asGeometry();
    if ( !geom )
        return;

    RefFeatureID* rfid = 0L;

    PrimitiveSetList& plist = geom->getPrimitiveSetList();
    for( PrimitiveSetList::iterator p = plist.begin(); p != plist.end(); ++p )
    {
        if ( !rfid )
            rfid = new RefFeatureID(fid);

        p->get()->setUserData( rfid );
    }
}


bool
FeatureSourceMultiNode::getFID(osg::PrimitiveSet* primSet, FeatureID& output) const
{
    const RefFeatureID* fid = dynamic_cast<const RefFeatureID*>( primSet->getUserData() );
    if ( fid )
    {
        output = *fid;
        return true;
    }

    OE_DEBUG << LC << "getFID failed b/c the primSet was not tagged with a RefFeatureID" << std::endl;
    return false;
}


bool
FeatureSourceMultiNode::getFID(osg::Drawable* drawable, int primIndex, FeatureID& output) const
{
    if ( primIndex < 0 )
        return false;

    for( FeatureIDDrawSetMap::const_iterator i = _drawSets.begin(); i != _drawSets.end(); ++i )
    {
        const FeatureDrawSet& drawSet = i->second;
        FeatureDrawSet::const_iterator d = drawSet.find(drawable);
        if ( d != drawSet.end() )
        {
            const osg::Geometry* geom = drawable->asGeometry();
            if ( geom )
            {
                const PrimitiveSetList& geomPrimSets = geom->getPrimitiveSetList();

                unsigned encounteredPrims = 0;
                for( PrimitiveSetList::const_iterator p = geomPrimSets.begin(); p != geomPrimSets.end(); ++p )
                {
                    const osg::PrimitiveSet* pset = p->get();
                    unsigned numPrims = pset->getNumPrimitives();
                    encounteredPrims += numPrims;

                    if ( encounteredPrims >= (unsigned)primIndex )
                    {
                        const RefFeatureID* fid = dynamic_cast<const RefFeatureID*>( pset->getUserData() );
                        if ( fid )
                        {
                            output = *fid;
                            return true;
                        }
                        else
                        {
                            OE_WARN << LC << "INTERNAL ERROR: found primset, but it's not tagged with a FID" << std::endl;
                            return false;
                        }
                    }
                }
            }
        }
    }

    return false;
}



FeatureSourceMultiNode::FeatureDrawSet&
FeatureSourceMultiNode::getDrawSet(const FeatureID& fid )
{
    static FeatureDrawSet s_empty;

    FeatureIDDrawSetMap::iterator i = _drawSets.find(fid);
    return i != _drawSets.end() ? i->second : s_empty;
}
