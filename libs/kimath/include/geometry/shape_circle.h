/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2013 CERN
 * @author Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you may find one here:
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 * or you may search the http://www.gnu.org website for the version 2 license,
 * or you may write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#ifndef __SHAPE_CIRCLE_H
#define __SHAPE_CIRCLE_H

#include <geometry/shape.h>
#include <math/box2.h>
#include <math/vector2d.h>

#include <algorithm>

class SHAPE_CIRCLE : public SHAPE
{
public:
    SHAPE_CIRCLE() :
        SHAPE( SH_CIRCLE ), m_radius( 0 )
    {}

    SHAPE_CIRCLE( const VECTOR2I& aCenter, int aRadius ) :
        SHAPE( SH_CIRCLE ), m_radius( aRadius ), m_center( aCenter )
    {}

    SHAPE_CIRCLE( const SHAPE_CIRCLE& aOther ) :
        SHAPE( SH_CIRCLE ),
        m_radius( aOther.m_radius ),
        m_center( aOther.m_center )
    {};

    ~SHAPE_CIRCLE()
    {}

    SHAPE* Clone() const override
    {
        return new SHAPE_CIRCLE( *this );
    }

    SHAPE_CIRCLE& operator=( const SHAPE_CIRCLE& ) = default;

    const BOX2I BBox( int aClearance = 0 ) const override
    {
        const VECTOR2I rc( m_radius + aClearance, m_radius + aClearance );

        return BOX2I( m_center - rc, rc * 2 );
    }

    bool Collide( const SEG& aSeg, int aClearance = 0, int* aActual = nullptr ) const override
    {
        int minDist = aClearance + m_radius;
        ecoord dist_sq = aSeg.SquaredDistance( m_center );

        if( dist_sq == 0 || dist_sq < (ecoord) minDist * minDist )
        {
            if( aActual )
                *aActual = std::max( 0, (int) sqrt( dist_sq ) - m_radius );

            return true;
        }

        return false;
    }

    void SetRadius( int aRadius )
    {
        m_radius = aRadius;
    }

    void SetCenter( const VECTOR2I& aCenter )
    {
        m_center = aCenter;
    }

    int GetRadius() const
    {
        return m_radius;
    }

    const VECTOR2I GetCenter() const
    {
        return m_center;
    }

    void Move( const VECTOR2I& aVector ) override
    {
        m_center += aVector;
    }

    void Rotate( double aAngle, const VECTOR2I& aCenter = { 0, 0 } ) override
    {
        m_center -= aCenter;
        m_center = m_center.Rotate( aAngle );
        m_center += aCenter;
    }

    bool IsSolid() const override
    {
        return true;
    }
private:
    int m_radius;
    VECTOR2I m_center;
};

#endif
