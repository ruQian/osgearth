/* -*-c++-*- */
/* osgEarth - Dynamic map generation toolkit for OpenSceneGraph
 * Copyright 2008-2009 Pelican Ventures, Inc.
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

#include <osgEarth/CacheSeed>
#include <osgEarth/Mercator>

using namespace osgEarth;

void CacheSeed::seed(MapConfig *map)
{
    //Create a TileBuilder for the map
    _tileBuilder = TileBuilder::create( map, map->getFilename() );
    
    osg::ref_ptr<TileKey> key = _tileBuilder->getDataProfile().createTileKey( "" );

    //Set the default bounds to the entire profile if the user didn't override the bounds
    if (_bounds._min.x() == 0 && _bounds._min.y() == 0 &&
        _bounds._max.x() == 0 && _bounds._max.y() == 0)
    {
        key->getGeoExtents(_bounds._min.x(), _bounds._min.y(), _bounds._max.x(), _bounds._max.y());
    }


    bool hasCaches = false;
    int src_min_level = INT_MAX;
    int src_max_level = 0;

    //Assumes the the TileSource will perform the caching for us when we call createImage
    for (TileSourceList::iterator itr = _tileBuilder->getImageSources().begin(); itr != _tileBuilder->getImageSources().end(); ++itr)
    {
        TileSource* src = itr->get();
        if (!dynamic_cast<CachedTileSource*>(src))
        {
            osg::notify(osg::NOTICE) << "Warning:  Image " << src->getName() << " has no cache." << std::endl;
        }
        else
        {
            hasCaches = true;
            if ( src->getMinLevel() < src_min_level )
                src_min_level = src->getMinLevel();
            if ( src->getMaxLevel() > src_max_level )
                src_max_level = src->getMaxLevel();
        }
    }

    for (TileSourceList::iterator itr = _tileBuilder->getHeightFieldSources().begin(); itr != _tileBuilder->getHeightFieldSources().end(); ++itr)
    {
        TileSource* src = itr->get();
        if (!dynamic_cast<CachedTileSource*>(src))
        {
            osg::notify(osg::NOTICE) << "Warning:  Heightfield " << src->getName() << " has no cache." << std::endl;
        }
        else
        {
            hasCaches = true;
            if ( src->getMinLevel() < src_min_level )
                src_min_level = src->getMinLevel();
            if ( src->getMaxLevel() > src_max_level )
                src_max_level = src->getMaxLevel();
        }
    }

    if (!hasCaches)
    {
        osg::notify(osg::NOTICE) << "There are no caches specified for the given map.  Please configure a cache in the mapconfig" << std::endl;
        return;
    }

    if ( src_max_level > 0 && src_max_level < _maxLevel )
    {
        _maxLevel = src_max_level;
    }

    osg::notify(osg::NOTICE) << "Maximum cache level will be " << _maxLevel << std::endl;

    processKey( _tileBuilder.get(), key.get() );
}


void CacheSeed::processKey(TileBuilder* tile_builder, TileKey *key)
{
    unsigned int lod = key->getLevelOfDetail();

//    osg::notify(osg::NOTICE) << "Checking key = " << key->str() << std::endl;

    if ( _minLevel <= lod && _maxLevel >= lod )
    {
        if (lod > 0 || !key->isGeodetic())
        {
            //Assumes the the TileSource will perform the caching for us when we call createImage
            for (TileSourceList::iterator itr = tile_builder->getImageSources().begin(); itr != tile_builder->getImageSources().end(); ++itr)
            {
                TileSource* source = itr->get();
                if ( lod >= source->getMinLevel() && source->getMaxLevel() <= lod )
                {
                    osg::notify(osg::NOTICE) << "Caching " << source->getName() << ", tile = " << key->str() << std::endl;
                    osg::ref_ptr<osg::Image> image = tile_builder->createImage(key, source);
                }
            }

            for (TileSourceList::iterator itr = tile_builder->getHeightFieldSources().begin(); itr != tile_builder->getHeightFieldSources().end(); ++itr)
            {
                //TODO:  Handle compatible but non exact heightfield keys (JB)
                TileSource* source = itr->get();
                if ( lod >=source->getMinLevel() && source->getMaxLevel() <= lod )
                {
                    osg::notify(osg::NOTICE) << "Caching " << source->getName() << ", tile = " << key->str() << std::endl;
                    osg::ref_ptr<osg::HeightField> heightField = source->createHeightField(key);
                }
            }
        }
    }

    if (key->getLevelOfDetail() <= _maxLevel)
    {
        osg::ref_ptr<TileKey> k0 = key->getSubkey(0);
        osg::ref_ptr<TileKey> k1 = key->getSubkey(1);
        osg::ref_ptr<TileKey> k2;
        osg::ref_ptr<TileKey> k3;

        if (key->getLevelOfDetail() > 0 || !key->isGeodetic())
        {
            k2 = key->getSubkey(2);
            k3 = key->getSubkey(3);
        }

        //Check to see if the bounds intersects ANY of the tile's children.  If it does, then process all of the children
        //for this level
        if ((k0.valid() && _bounds.intersects(k0.get())) ||
            (k1.valid() && _bounds.intersects(k1.get())) ||
            (k2.valid() && _bounds.intersects(k2.get())) ||
            (k3.valid() && _bounds.intersects(k3.get())))
        {
            if (k0.valid()) processKey(tile_builder, k0.get()); 
            if (k1.valid()) processKey(tile_builder, k1.get()); 
            if (k2.valid()) processKey(tile_builder, k2.get()); 
            if (k3.valid()) processKey(tile_builder, k3.get()); 
        }
    }
}