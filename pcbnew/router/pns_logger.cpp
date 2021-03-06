/*
 * KiRouter - a push-and-(sometimes-)shove PCB router
 *
 * Copyright (C) 2013-2014 CERN
 * Copyright (C) 2016 KiCad Developers, see AUTHORS.txt for contributors.
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

#include "pns_logger.h"
#include "pns_item.h"
#include "pns_via.h"
#include "pns_line.h"
#include "pns_segment.h"
#include "pns_solid.h"

#include <geometry/shape.h>
#include <geometry/shape_line_chain.h>
#include <geometry/shape_rect.h>
#include <geometry/shape_circle.h>
#include <geometry/shape_simple.h>

namespace PNS {

LOGGER::LOGGER( )
{
}


LOGGER::~LOGGER()
{
}


void LOGGER::Clear()
{
    m_events.clear();
}


void LOGGER::Save( const std::string& aFilename )
{
    FILE* f = fopen( aFilename.c_str(), "wb" );

    wxLogTrace( "PNS", "Saving to '%s' [%p]", aFilename.c_str(), f );

    for( const auto evt : m_events )
    {
        uint64_t id = 0;
        if( evt.item && evt.item->Parent() )
        {
            const char* idString = evt.item->Parent()->m_Uuid.AsString().c_str();
            fprintf( f, "event %d %d %d %s\n", evt.type, evt.p.x, evt.p.y, idString );
        }
    }

    fclose( f );
}


void LOGGER::Log( LOGGER::EVENT_TYPE evt, VECTOR2I pos, const ITEM* item )
{
    LOGGER::EVENT_ENTRY ent;

    ent.type = evt;
    ent.p = pos;
    ent.item = item;

    m_events.push_back( ent );

}

}
