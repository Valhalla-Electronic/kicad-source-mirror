/*
 * KiRouter - a push-and-(sometimes-)shove PCB router
 *
 * Copyright (C) 2013-2014 CERN
 * Copyright (C) 2016-2020 KiCad Developers, see AUTHORS.txt for contributors.
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __PNS_VIA_H
#define __PNS_VIA_H

#include <geometry/shape_line_chain.h>
#include <geometry/shape_circle.h>
#include <math/box2.h>

#include "../class_track.h"

#include "pns_item.h"

namespace PNS {

class NODE;

// uniquely identifies a VIA within a NODE without using pointers. Used to
// simplify (or complexifiy, depending on the point of view) the pointer management
// in PNS::NODE. Sooner or later I'll have to fix it for good using smart pointers - twl
struct VIA_HANDLE
{
    bool valid = false;
    VECTOR2I pos;
    LAYER_RANGE layers;
    int net;
};

class VIA : public ITEM
{
public:
    VIA() :
        ITEM( VIA_T )
    {
        m_diameter = 2; // Dummy value
        m_drill    = 0;
        m_viaType  = VIATYPE::THROUGH;
    }

    VIA( const VECTOR2I& aPos, const LAYER_RANGE& aLayers, int aDiameter, int aDrill, int aNet = -1,
            VIATYPE aViaType = VIATYPE::THROUGH )
            : ITEM( VIA_T )
    {
        SetNet( aNet );
        SetLayers( aLayers );
        m_pos = aPos;
        m_diameter = aDiameter;
        m_drill = aDrill;
        m_shape = SHAPE_CIRCLE( aPos, aDiameter / 2 );
        m_alternateShape = SHAPE_CIRCLE( m_pos, aDrill / 2 );
        m_viaType = aViaType;
    }


    VIA( const VIA& aB ) :
        ITEM( aB )
    {
        SetNet( aB.Net() );
        SetLayers( aB.Layers() );
        m_pos = aB.m_pos;
        m_diameter = aB.m_diameter;
        m_shape = SHAPE_CIRCLE( m_pos, m_diameter / 2 );
        m_alternateShape = SHAPE_CIRCLE( m_pos, aB.m_drill / 2 );
        m_marker = aB.m_marker;
        m_rank = aB.m_rank;
        m_drill = aB.m_drill;
        m_viaType = aB.m_viaType;
    }

    static inline bool ClassOf( const ITEM* aItem )
    {
        return aItem && VIA_T == aItem->Kind();
    }


    const VECTOR2I& Pos() const
    {
        return m_pos;
    }

    void SetPos( const VECTOR2I& aPos )
    {
        m_pos = aPos;
        m_shape.SetCenter( aPos );
    }

    VIATYPE ViaType() const
    {
        return m_viaType;
    }

    void SetViaType( VIATYPE aViaType )
    {
        m_viaType = aViaType;
    }

    int Diameter() const
    {
        return m_diameter;
    }

    void SetDiameter( int aDiameter )
    {
        m_diameter = aDiameter;
        m_shape.SetRadius( m_diameter / 2 );
    }

    int Drill() const
    {
        return m_drill;
    }

    void SetDrill( int aDrill )
    {
        m_drill = aDrill;
    }

    bool PushoutForce( NODE* aNode,
            const VECTOR2I& aDirection,
            VECTOR2I& aForce,
            bool aSolidsOnly = true,
            int aMaxIterations = 10 );

    const SHAPE* Shape() const override
    {
        return &m_shape;
    }

    const SHAPE* AlternateShape() const override
    {
        return &m_alternateShape;
    }

    VIA* Clone() const override;

    const SHAPE_LINE_CHAIN Hull( int aClearance = 0, int aWalkaroundThickness = 0, int aLayer = -1 ) const override;

    virtual VECTOR2I Anchor( int n ) const override
    {
        return m_pos;
    }

    virtual int AnchorCount() const override
    {
        return 1;
    }

    OPT_BOX2I ChangedArea( const VIA* aOther ) const;

    const VIA_HANDLE MakeHandle() const;

private:
    int          m_diameter;
    int          m_drill;
    VECTOR2I     m_pos;
    SHAPE_CIRCLE m_shape;
    SHAPE_CIRCLE m_alternateShape;
    VIATYPE      m_viaType;
};

}

#endif
