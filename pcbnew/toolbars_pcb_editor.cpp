/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2012 Jean-Pierre Charras, jean-pierre.charras@ujf-grenoble.fr
 * Copyright (C) 2012 SoftPLC Corporation, Dick Hollenbeck <dick@softplc.com>
 * Copyright (C) 2012 Wayne Stambaugh <stambaughw@gmail.com>
 * Copyright (C) 1992-2019 KiCad Developers, see AUTHORS.txt for contributors.
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

#include <bitmaps.h>
#include <class_board.h>
#include <confirm.h>
#include <dialog_helpers.h>
#include <fctsys.h>
#include <kiface_i.h>
#include <memory>
#include <pcb_edit_frame.h>
#include <pcb_layer_box_selector.h>
#include <pcbnew.h>
#include <pcbnew_id.h>
#include <pcbnew_settings.h>
#include <pgm_base.h>
#include <router/pns_routing_settings.h>
#include <router/router_tool.h>
#include <settings/color_settings.h>
#include <settings/common_settings.h>
#include <tool/action_toolbar.h>
#include <tool/actions.h>
#include <tool/tool_manager.h>
#include <tools/pcb_actions.h>
#include <tools/selection_tool.h>
#include <view/view.h>
#include <wx/wupdlock.h>

#if defined(KICAD_SCRIPTING) || defined(KICAD_SCRIPTING_WXPYTHON)
#include <python_scripting.h>
#endif

#define SEL_LAYER_HELP _( \
        "Show active layer selections\nand select layer pair for route and place via" )


/* Data to build the layer pair indicator button */
static std::unique_ptr<wxBitmap> LayerPairBitmap;

#define BM_LAYERICON_SIZE 24
static const char s_BitmapLayerIcon[BM_LAYERICON_SIZE][BM_LAYERICON_SIZE] =
{
    // 0 = draw pixel with active layer color
    // 1 = draw pixel with top layer color (top/bottom layer used inautoroute and place via)
    // 2 = draw pixel with bottom layer color
    // 3 = draw pixel with via color
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 3, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 3, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 3, 0, 1, 1, 1, 1, 3, 0, 0, 0, 0, 0, 0, 0 },
    { 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 0, 1, 1, 1, 1, 3, 3, 2, 2, 2, 2, 2, 2, 2 },
    { 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 1, 1, 1, 1, 0, 3, 3, 2, 2, 2, 2, 2, 2, 2 },
    { 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 1, 1, 1, 1, 0, 3, 3, 2, 2, 2, 2, 2, 2, 2 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 1, 1, 1, 0, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 3, 3, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 3, 3, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};


void PCB_EDIT_FRAME::PrepareLayerIndicator( bool aForceRebuild )
{
    int        ii, jj;
    COLOR4D    active_layer_color, top_color, bottom_color, via_color, background_color;
    bool       change = aForceRebuild;

    int requested_scale = Pgm().GetCommonSettings()->m_Appearance.icon_scale;

    if( m_prevIconVal.previous_requested_scale != requested_scale )
    {
        m_prevIconVal.previous_requested_scale = requested_scale;
        change = true;
    }

    active_layer_color = GetColorSettings()->GetColor( GetActiveLayer() );

    if( m_prevIconVal.previous_active_layer_color != active_layer_color )
    {
        m_prevIconVal.previous_active_layer_color = active_layer_color;
        change = true;
    }

    top_color = GetColorSettings()->GetColor( GetScreen()->m_Route_Layer_TOP );

    if( m_prevIconVal.previous_Route_Layer_TOP_color != top_color )
    {
        m_prevIconVal.previous_Route_Layer_TOP_color = top_color;
        change = true;
    }

    bottom_color = GetColorSettings()->GetColor( GetScreen()->m_Route_Layer_BOTTOM );

    if( m_prevIconVal.previous_Route_Layer_BOTTOM_color != bottom_color )
    {
        m_prevIconVal.previous_Route_Layer_BOTTOM_color = bottom_color;
        change = true;
    }

    int via_type = static_cast<int>( GetDesignSettings().m_CurrentViaType );
    via_color = GetColorSettings()->GetColor( LAYER_VIAS + via_type );

    if( m_prevIconVal.previous_via_color != via_color )
    {
        m_prevIconVal.previous_via_color = via_color;
        change = true;
    }

    background_color = GetColorSettings()->GetColor( LAYER_PCB_BACKGROUND );

    if( m_prevIconVal.previous_background_color != background_color )
    {
        m_prevIconVal.previous_background_color = background_color;
        change = true;
    }

    if( change || !LayerPairBitmap )
    {
        LayerPairBitmap = std::make_unique<wxBitmap>( 24, 24 );

        // Draw the icon, with colors according to the active layer and layer pairs for via
        // command (change layer)
        wxMemoryDC iconDC;
        iconDC.SelectObject( *LayerPairBitmap );
        wxBrush    brush;
        wxPen      pen;
        int buttonColor = -1;

        brush.SetStyle( wxBRUSHSTYLE_SOLID );
        brush.SetColour( background_color.WithAlpha(1.0).ToColour() );
        iconDC.SetBrush( brush );
        iconDC.DrawRectangle( 0, 0, BM_LAYERICON_SIZE, BM_LAYERICON_SIZE );

        for( ii = 0; ii < BM_LAYERICON_SIZE; ii++ )
        {
            for( jj = 0; jj < BM_LAYERICON_SIZE; jj++ )
            {
                if( s_BitmapLayerIcon[ii][jj] != buttonColor )
                {
                    switch( s_BitmapLayerIcon[ii][jj] )
                    {
                    default:
                    case 0: pen.SetColour( active_layer_color.ToColour() ); break;
                    case 1: pen.SetColour( top_color.ToColour() );          break;
                    case 2: pen.SetColour( bottom_color.ToColour() );       break;
                    case 3: pen.SetColour( via_color.ToColour() );          break;
                    }

                    buttonColor = s_BitmapLayerIcon[ii][jj];
                    iconDC.SetPen( pen );
                }

                iconDC.DrawPoint( jj, ii );
            }
        }

        // Deselect the bitmap from the DC in order to delete the MemoryDC safely without
        // deleting the bitmap
        iconDC.SelectObject( wxNullBitmap );

        // Scale the bitmap
        const int scale = ( requested_scale <= 0 ) ? KiIconScale( this ) : requested_scale;
        wxImage image = LayerPairBitmap->ConvertToImage();

        // "NEAREST" causes less mixing of colors
        image.Rescale( scale * image.GetWidth() / 4, scale * image.GetHeight() / 4,
                       wxIMAGE_QUALITY_NEAREST );

        LayerPairBitmap = std::make_unique<wxBitmap>( image );

        if( m_mainToolBar )
        {
            m_mainToolBar->SetToolBitmap( PCB_ACTIONS::selectLayerPair, *LayerPairBitmap );
            m_mainToolBar->Refresh();
        }
    }
}


void PCB_EDIT_FRAME::ReCreateHToolbar()
{
    // Note:
    // To rebuild the aui toolbar, the more easy way is to clear ( calling m_mainToolBar.Clear() )
    // all wxAuiToolBarItems.
    // However the wxAuiToolBarItems are not the owners of controls managed by
    // them and therefore do not delete them
    // So we do not recreate them after clearing the tools.

    wxWindowUpdateLocker dummy( this );

    if( m_mainToolBar )
        m_mainToolBar->ClearToolbar();
    else
        m_mainToolBar = new ACTION_TOOLBAR( this, ID_H_TOOLBAR, wxDefaultPosition, wxDefaultSize,
                                            KICAD_AUI_TB_STYLE | wxAUI_TB_HORZ_LAYOUT );

    // Set up toolbar
    if( Kiface().IsSingle() )
    {
        m_mainToolBar->Add( ACTIONS::doNew );
        m_mainToolBar->Add( ACTIONS::open );
    }

    m_mainToolBar->Add( ACTIONS::save );

    m_mainToolBar->AddScaledSeparator( this );
    m_mainToolBar->Add( PCB_ACTIONS::boardSetup );

    m_mainToolBar->AddScaledSeparator( this );
    m_mainToolBar->Add( ACTIONS::pageSettings );
    m_mainToolBar->Add( ACTIONS::print );
    m_mainToolBar->Add( ACTIONS::plot );

    m_mainToolBar->AddScaledSeparator( this );
    m_mainToolBar->Add( ACTIONS::undo );
    m_mainToolBar->Add( ACTIONS::redo );

    m_mainToolBar->AddScaledSeparator( this );
    m_mainToolBar->Add( ACTIONS::find );

    m_mainToolBar->AddScaledSeparator( this );
    m_mainToolBar->Add( ACTIONS::zoomRedraw );
    m_mainToolBar->Add( ACTIONS::zoomInCenter );
    m_mainToolBar->Add( ACTIONS::zoomOutCenter );
    m_mainToolBar->Add( ACTIONS::zoomFitScreen );
    m_mainToolBar->Add( ACTIONS::zoomTool, ACTION_TOOLBAR::TOGGLE, ACTION_TOOLBAR::CANCEL );

    m_mainToolBar->AddScaledSeparator( this );
    m_mainToolBar->Add( ACTIONS::showFootprintEditor );
    m_mainToolBar->Add( ACTIONS::showFootprintBrowser );

    m_mainToolBar->AddScaledSeparator( this );
    m_mainToolBar->Add( ACTIONS::updatePcbFromSchematic );
    m_mainToolBar->Add( PCB_ACTIONS::runDRC );

    m_mainToolBar->AddScaledSeparator( this );

    if( m_SelLayerBox == nullptr )
    {
        m_SelLayerBox = new PCB_LAYER_BOX_SELECTOR( m_mainToolBar, ID_TOOLBARH_PCB_SELECT_LAYER );
        m_SelLayerBox->SetBoardFrame( this );
    }

    ReCreateLayerBox( false );
    m_mainToolBar->AddControl( m_SelLayerBox );

    m_mainToolBar->Add( PCB_ACTIONS::selectLayerPair );
    PrepareLayerIndicator( true );    // Force rebuild of the bitmap with the active layer colors

    m_mainToolBar->AddScaledSeparator( this );
    m_mainToolBar->Add( PCB_ACTIONS::showEeschema );

    // Access to the scripting console
#if defined(KICAD_SCRIPTING_WXPYTHON)
    if( IsWxPythonLoaded() )
    {
        m_mainToolBar->AddScaledSeparator( this );
        m_mainToolBar->Add( PCB_ACTIONS::showPythonConsole, ACTION_TOOLBAR::TOGGLE );

#if defined(KICAD_SCRIPTING) && defined(KICAD_SCRIPTING_ACTION_MENU)
        AddActionPluginTools();
#endif
    }
#endif

    // after adding the buttons to the toolbar, must call Realize() to reflect the changes
    m_mainToolBar->Realize();
}


void PCB_EDIT_FRAME::ReCreateOptToolbar()
{
    // Note:
    // To rebuild the aui toolbar, the more easy way is to clear ( calling m_mainToolBar.Clear() )
    // all wxAuiToolBarItems.
    // However the wxAuiToolBarItems are not the owners of controls managed by
    // them and therefore do not delete them
    // So we do not recreate them after clearing the tools.

    wxWindowUpdateLocker dummy( this );

    if( m_optionsToolBar )
        m_optionsToolBar->ClearToolbar();
    else
        m_optionsToolBar = new ACTION_TOOLBAR( this, ID_OPT_TOOLBAR,
                                               wxDefaultPosition, wxDefaultSize,
                                               KICAD_AUI_TB_STYLE | wxAUI_TB_VERTICAL );

    m_optionsToolBar->Add( ACTIONS::toggleGrid,               ACTION_TOOLBAR::TOGGLE );

    m_optionsToolBar->Add( PCB_ACTIONS::togglePolarCoords,    ACTION_TOOLBAR::TOGGLE );
    m_optionsToolBar->Add( ACTIONS::imperialUnits,            ACTION_TOOLBAR::TOGGLE );
    m_optionsToolBar->Add( ACTIONS::metricUnits,              ACTION_TOOLBAR::TOGGLE );
    m_optionsToolBar->Add( ACTIONS::toggleCursorStyle,        ACTION_TOOLBAR::TOGGLE );

    m_optionsToolBar->AddScaledSeparator( this );
    m_optionsToolBar->Add( PCB_ACTIONS::showRatsnest,         ACTION_TOOLBAR::TOGGLE );
    m_optionsToolBar->Add( PCB_ACTIONS::ratsnestLineMode,     ACTION_TOOLBAR::TOGGLE );

    m_optionsToolBar->AddScaledSeparator( this );
    m_optionsToolBar->Add( PCB_ACTIONS::zoneDisplayEnable,    ACTION_TOOLBAR::TOGGLE );
    m_optionsToolBar->Add( PCB_ACTIONS::zoneDisplayDisable,   ACTION_TOOLBAR::TOGGLE );
    m_optionsToolBar->Add( PCB_ACTIONS::zoneDisplayOutlines,  ACTION_TOOLBAR::TOGGLE );

    m_optionsToolBar->AddScaledSeparator( this );
    m_optionsToolBar->Add( PCB_ACTIONS::padDisplayMode,       ACTION_TOOLBAR::TOGGLE );
    m_optionsToolBar->Add( PCB_ACTIONS::viaDisplayMode,       ACTION_TOOLBAR::TOGGLE );
    m_optionsToolBar->Add( PCB_ACTIONS::trackDisplayMode,     ACTION_TOOLBAR::TOGGLE );
    m_optionsToolBar->Add( ACTIONS::highContrastMode,         ACTION_TOOLBAR::TOGGLE );

    // Tools to show/hide toolbars:
    m_optionsToolBar->AddScaledSeparator( this );
    m_optionsToolBar->Add( PCB_ACTIONS::showLayersManager,    ACTION_TOOLBAR::TOGGLE  );
    m_optionsToolBar->Add( PCB_ACTIONS::showMicrowaveToolbar, ACTION_TOOLBAR::TOGGLE  );

    m_optionsToolBar->Realize();
}


void PCB_EDIT_FRAME::ReCreateVToolbar()
{
    wxWindowUpdateLocker dummy( this );

    if( m_drawToolBar )
        m_drawToolBar->ClearToolbar();
    else
        m_drawToolBar = new ACTION_TOOLBAR( this, ID_V_TOOLBAR, wxDefaultPosition, wxDefaultSize,
                                            KICAD_AUI_TB_STYLE | wxAUI_TB_VERTICAL );

    m_drawToolBar->Add( ACTIONS::selectionTool,            ACTION_TOOLBAR::TOGGLE );
    m_drawToolBar->Add( PCB_ACTIONS::highlightNetTool,     ACTION_TOOLBAR::TOGGLE );
    m_drawToolBar->Add( PCB_ACTIONS::localRatsnestTool,    ACTION_TOOLBAR::TOGGLE );

    m_drawToolBar->AddScaledSeparator( this );
    m_drawToolBar->Add( PCB_ACTIONS::placeModule,          ACTION_TOOLBAR::TOGGLE );
    m_drawToolBar->Add( PCB_ACTIONS::routeSingleTrack,     ACTION_TOOLBAR::TOGGLE );
    m_drawToolBar->Add( PCB_ACTIONS::drawVia,              ACTION_TOOLBAR::TOGGLE );
    m_drawToolBar->Add( PCB_ACTIONS::drawZone,             ACTION_TOOLBAR::TOGGLE );
    m_drawToolBar->Add( PCB_ACTIONS::drawZoneKeepout,      ACTION_TOOLBAR::TOGGLE );

    m_drawToolBar->AddScaledSeparator( this );
    m_drawToolBar->Add( PCB_ACTIONS::drawLine,             ACTION_TOOLBAR::TOGGLE );
    m_drawToolBar->Add( PCB_ACTIONS::drawRectangle,        ACTION_TOOLBAR::TOGGLE );
    m_drawToolBar->Add( PCB_ACTIONS::drawCircle,           ACTION_TOOLBAR::TOGGLE );
    m_drawToolBar->Add( PCB_ACTIONS::drawArc,              ACTION_TOOLBAR::TOGGLE );
    m_drawToolBar->Add( PCB_ACTIONS::drawPolygon,          ACTION_TOOLBAR::TOGGLE );
    m_drawToolBar->Add( PCB_ACTIONS::placeText,            ACTION_TOOLBAR::TOGGLE );
    m_drawToolBar->Add( PCB_ACTIONS::drawAlignedDimension, ACTION_TOOLBAR::TOGGLE );
    // TODO: re-insert when we have a multi-select tool button
    // m_drawToolBar->Add( PCB_ACTIONS::drawLeader,           ACTION_TOOLBAR::TOGGLE );
    m_drawToolBar->Add( PCB_ACTIONS::placeTarget,          ACTION_TOOLBAR::TOGGLE );
    m_drawToolBar->Add( ACTIONS::deleteTool,               ACTION_TOOLBAR::TOGGLE );

    m_drawToolBar->AddScaledSeparator( this );
    // TODO: re-insert when we have a multi-select tool button
    // m_drawToolBar->Add( PCB_ACTIONS::drillOrigin,          ACTION_TOOLBAR::TOGGLE );
    m_drawToolBar->Add( PCB_ACTIONS::gridSetOrigin,        ACTION_TOOLBAR::TOGGLE );
    m_drawToolBar->Add( ACTIONS::measureTool,              ACTION_TOOLBAR::TOGGLE );

    SELECTION_TOOL* selTool   = m_toolManager->GetTool<SELECTION_TOOL>();
    ACTION_MENU*    routeMenu = new ACTION_MENU( false, selTool );
    routeMenu->Add( PCB_ACTIONS::routerHighlightMode,  ACTION_MENU::CHECK );
    routeMenu->Add( PCB_ACTIONS::routerShoveMode,      ACTION_MENU::CHECK );
    routeMenu->Add( PCB_ACTIONS::routerWalkaroundMode, ACTION_MENU::CHECK );

    routeMenu->AppendSeparator();
    routeMenu->Add( PCB_ACTIONS::routerSettingsDialog );
    m_drawToolBar->AddToolContextMenu( PCB_ACTIONS::routeSingleTrack, routeMenu );

    ACTION_MENU* zoneMenu = new ACTION_MENU( false, selTool );
    zoneMenu->Add( PCB_ACTIONS::zoneFillAll );
    zoneMenu->Add( PCB_ACTIONS::zoneUnfillAll );
    m_drawToolBar->AddToolContextMenu( PCB_ACTIONS::drawZone, zoneMenu );

    m_drawToolBar->Realize();
}


/* Create the auxiliary vertical right toolbar, showing tools for microwave applications
 */
void PCB_EDIT_FRAME::ReCreateMicrowaveVToolbar()
{
    wxWindowUpdateLocker dummy(this);

    if( m_microWaveToolBar )
        m_microWaveToolBar->ClearToolbar();
    else
        m_microWaveToolBar = new ACTION_TOOLBAR( this, ID_MICROWAVE_V_TOOLBAR, wxDefaultPosition,
                                                 wxDefaultSize,
                                                 KICAD_AUI_TB_STYLE | wxAUI_TB_VERTICAL );

    // Set up toolbar
    m_microWaveToolBar->Add( PCB_ACTIONS::microwaveCreateLine,          ACTION_TOOLBAR::TOGGLE );
    m_microWaveToolBar->Add( PCB_ACTIONS::microwaveCreateGap,           ACTION_TOOLBAR::TOGGLE );

    m_microWaveToolBar->AddScaledSeparator( this );
    m_microWaveToolBar->Add( PCB_ACTIONS::microwaveCreateStub,          ACTION_TOOLBAR::TOGGLE );
    m_microWaveToolBar->Add( PCB_ACTIONS::microwaveCreateStubArc,       ACTION_TOOLBAR::TOGGLE );
    m_microWaveToolBar->Add( PCB_ACTIONS::microwaveCreateFunctionShape, ACTION_TOOLBAR::TOGGLE );

    m_microWaveToolBar->Realize();
}


void PCB_EDIT_FRAME::ReCreateAuxiliaryToolbar()
{
    wxWindowUpdateLocker dummy( this );

    if( m_auxiliaryToolBar )
    {
        UpdateTrackWidthSelectBox( m_SelTrackWidthBox );
        UpdateViaSizeSelectBox( m_SelViaSizeBox );
        UpdateGridSelectBox();

        // combobox sizes can have changed: apply new best sizes
        wxAuiToolBarItem* item = m_auxiliaryToolBar->FindTool( ID_AUX_TOOLBAR_PCB_TRACK_WIDTH );
        item->SetMinSize( m_SelTrackWidthBox->GetBestSize() );
        item = m_auxiliaryToolBar->FindTool( ID_AUX_TOOLBAR_PCB_VIA_SIZE );
        item->SetMinSize( m_SelViaSizeBox->GetBestSize() );

        m_auxiliaryToolBar->Realize();
        m_auimgr.Update();
        return;
    }

    m_auxiliaryToolBar = new ACTION_TOOLBAR( this, ID_AUX_TOOLBAR,
                                             wxDefaultPosition, wxDefaultSize,
                                             KICAD_AUI_TB_STYLE | wxAUI_TB_HORZ_LAYOUT );

    /* Set up toolbar items */

    // Creates box to display and choose tracks widths:
    if( m_SelTrackWidthBox == nullptr )
        m_SelTrackWidthBox = new wxChoice( m_auxiliaryToolBar, ID_AUX_TOOLBAR_PCB_TRACK_WIDTH,
                                           wxDefaultPosition, wxDefaultSize, 0, NULL );

    UpdateTrackWidthSelectBox( m_SelTrackWidthBox );
    m_auxiliaryToolBar->AddControl( m_SelTrackWidthBox );

    // Creates box to display and choose vias diameters:

    if( m_SelViaSizeBox == nullptr )
        m_SelViaSizeBox = new wxChoice( m_auxiliaryToolBar, ID_AUX_TOOLBAR_PCB_VIA_SIZE,
                                        wxDefaultPosition, wxDefaultSize, 0, NULL );

    UpdateViaSizeSelectBox( m_SelViaSizeBox );
    m_auxiliaryToolBar->AddControl( m_SelViaSizeBox );
    m_auxiliaryToolBar->AddScaledSeparator( this );

    // Creates box to display and choose strategy to handle tracks an vias sizes:
    m_auxiliaryToolBar->AddTool( ID_AUX_TOOLBAR_PCB_SELECT_AUTO_WIDTH, wxEmptyString,
                                 KiScaledBitmap( auto_track_width_xpm, this ),
                                 _( "Auto track width: when starting on an existing track "
                                    "use its width\notherwise, use current width setting" ),
                                 wxITEM_CHECK );

    // Add the box to display and select the current grid size:
    m_auxiliaryToolBar->AddScaledSeparator( this );

    if( m_gridSelectBox == nullptr )
        m_gridSelectBox = new wxChoice( m_auxiliaryToolBar, ID_ON_GRID_SELECT,
                                        wxDefaultPosition, wxDefaultSize, 0, NULL );

    UpdateGridSelectBox();

    m_auxiliaryToolBar->AddControl( m_gridSelectBox );

    //  Add the box to display and select the current Zoom
    m_auxiliaryToolBar->AddScaledSeparator( this );

    if( m_zoomSelectBox == nullptr )
        m_zoomSelectBox = new wxChoice( m_auxiliaryToolBar, ID_ON_ZOOM_SELECT,
                                        wxDefaultPosition, wxDefaultSize, 0, NULL );

    UpdateZoomSelectBox();
    m_auxiliaryToolBar->AddControl( m_zoomSelectBox );

    // after adding the buttons to the toolbar, must call Realize()
    m_auxiliaryToolBar->Realize();
}


void PCB_EDIT_FRAME::UpdateTrackWidthSelectBox( wxChoice* aTrackWidthSelectBox, bool aEdit )
{
    if( aTrackWidthSelectBox == NULL )
        return;

    wxString msg;
    bool     mmFirst = GetUserUnits() != EDA_UNITS::INCHES;

    aTrackWidthSelectBox->Clear();

    for( unsigned ii = 0; ii < GetDesignSettings().m_TrackWidthList.size(); ii++ )
    {
        int size = GetDesignSettings().m_TrackWidthList[ii];

        double valueMils = To_User_Unit( EDA_UNITS::INCHES, size ) * 1000;
        double value_mm = To_User_Unit( EDA_UNITS::MILLIMETRES, size );

        if( mmFirst )
            msg.Printf( _( "Track: %.3f mm (%.2f mils)" ), value_mm, valueMils );
        else
            msg.Printf( _( "Track: %.2f mils (%.3f mm)" ), valueMils, value_mm );

        // Mark the netclass track width value (the first in list)
        if( ii == 0 )
            msg << wxT( " *" );

        aTrackWidthSelectBox->Append( msg );
    }

    if( aEdit )
    {
        aTrackWidthSelectBox->Append( wxT( "---" ) );
        aTrackWidthSelectBox->Append( _( "Edit Pre-defined Sizes..." ) );
    }

    if( GetDesignSettings().GetTrackWidthIndex() >= GetDesignSettings().m_TrackWidthList.size() )
        GetDesignSettings().SetTrackWidthIndex( 0 );

    aTrackWidthSelectBox->SetSelection( GetDesignSettings().GetTrackWidthIndex() );
}


void PCB_EDIT_FRAME::UpdateViaSizeSelectBox( wxChoice* aViaSizeSelectBox, bool aEdit )
{
    if( aViaSizeSelectBox == NULL )
        return;

    aViaSizeSelectBox->Clear();

    bool mmFirst = GetUserUnits() != EDA_UNITS::INCHES;

    for( unsigned ii = 0; ii < GetDesignSettings().m_ViasDimensionsList.size(); ii++ )
    {
        VIA_DIMENSION viaDimension = GetDesignSettings().m_ViasDimensionsList[ii];
        wxString      msg, mmStr, milsStr;

        double diam = To_User_Unit( EDA_UNITS::MILLIMETRES, viaDimension.m_Diameter );
        double hole = To_User_Unit( EDA_UNITS::MILLIMETRES, viaDimension.m_Drill );

        if( hole > 0 )
            mmStr.Printf( _( "%.2f / %.2f mm" ), diam, hole );
        else
            mmStr.Printf( _( "%.2f mm" ), diam );

        diam = To_User_Unit( EDA_UNITS::INCHES, viaDimension.m_Diameter ) * 1000;
        hole = To_User_Unit( EDA_UNITS::INCHES, viaDimension.m_Drill ) * 1000;

        if( hole > 0 )
            milsStr.Printf( _( "%.1f / %.1f mils" ), diam, hole );
        else
            milsStr.Printf( _( "%.1f mils" ), diam );

        msg.Printf( _( "Via: %s (%s)" ), mmFirst ? mmStr : milsStr, mmFirst ? milsStr : mmStr );

        // Mark the netclass via size value (the first in list)
        if( ii == 0 )
            msg << wxT( " *" );

        aViaSizeSelectBox->Append( msg );
    }

    if( aEdit )
    {
        aViaSizeSelectBox->Append( wxT( "---" ) );
        aViaSizeSelectBox->Append( _( "Edit Pre-defined Sizes..." ) );
    }

    if( GetDesignSettings().GetViaSizeIndex() >= GetDesignSettings().m_ViasDimensionsList.size() )
        GetDesignSettings().SetViaSizeIndex( 0 );

    aViaSizeSelectBox->SetSelection( GetDesignSettings().GetViaSizeIndex() );
}


void PCB_EDIT_FRAME::ReCreateLayerBox( bool aForceResizeToolbar )
{
    if( m_SelLayerBox == NULL || m_mainToolBar == NULL )
        return;

    m_SelLayerBox->SetToolTip( _( "+/- to switch" ) );
    m_SelLayerBox->Resync();

    if( aForceResizeToolbar )
    {
        // the layer box can have its size changed
        // Update the aui manager, to take in account the new size
        m_auimgr.Update();
    }
}


void PCB_EDIT_FRAME::ToggleLayersManager()
{
    // show auxiliary Vertical layers and visibility manager toolbar
    m_show_layer_manager_tools = !m_show_layer_manager_tools;
    m_auimgr.GetPane( "LayersManager" ).Show( m_show_layer_manager_tools );
    m_auimgr.GetPane( "SelectionFilter" ).Show( m_show_layer_manager_tools );
    m_auimgr.Update();
}


void PCB_EDIT_FRAME::ToggleMicrowaveToolbar()
{
    m_show_microwave_tools = !m_show_microwave_tools;
    m_auimgr.GetPane( "MicrowaveToolbar" ).Show( m_show_microwave_tools );
    m_auimgr.Update();
}


void PCB_EDIT_FRAME::OnUpdateSelectTrackWidth( wxUpdateUIEvent& aEvent )
{
    if( aEvent.GetId() == ID_AUX_TOOLBAR_PCB_TRACK_WIDTH )
    {
        if( m_SelTrackWidthBox->GetSelection() != (int) GetDesignSettings().GetTrackWidthIndex() )
            m_SelTrackWidthBox->SetSelection( GetDesignSettings().GetTrackWidthIndex() );
    }
}


void PCB_EDIT_FRAME::OnUpdateSelectViaSize( wxUpdateUIEvent& aEvent )
{
    if( aEvent.GetId() == ID_AUX_TOOLBAR_PCB_VIA_SIZE )
    {
        if( m_SelViaSizeBox->GetSelection() != (int) GetDesignSettings().GetViaSizeIndex() )
            m_SelViaSizeBox->SetSelection( GetDesignSettings().GetViaSizeIndex() );
    }
}


void PCB_EDIT_FRAME::OnUpdateLayerSelectBox( wxUpdateUIEvent& aEvent )
{
    if( m_SelLayerBox->GetLayerSelection() != GetActiveLayer() )
        m_SelLayerBox->SetLayerSelection( GetActiveLayer() );
}
