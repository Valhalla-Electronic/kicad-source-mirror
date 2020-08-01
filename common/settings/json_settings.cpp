/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2020 Jon Evans <jon@craftyjon.com>
 * Copyright (C) 2020 KiCad Developers, see AUTHORS.txt for contributors.
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

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <utility>
#include <sstream>

#include <common.h>
#include <gal/color4d.h>
#include <settings/json_settings.h>
#include <settings/nested_settings.h>
#include <settings/parameters.h>
#include <wx/config.h>
#include <wx/debug.h>
#include <wx/filename.h>

extern const char* traceSettings;


JSON_SETTINGS::JSON_SETTINGS( const std::string& aFilename, SETTINGS_LOC aLocation,
                              int aSchemaVersion, bool aCreateIfMissing, bool aCreateIfDefault,
                              bool aWriteFile ) :
        nlohmann::json(),
        m_filename( aFilename ),
        m_legacy_filename( "" ),
        m_location( aLocation ),
        m_createIfMissing( aCreateIfMissing ),
        m_createIfDefault( aCreateIfDefault ),
        m_writeFile( aWriteFile ),
        m_deleteLegacyAfterMigration( true ),
        m_resetParamsIfMissing( true ),
        m_schemaVersion( aSchemaVersion ),
        m_manager( nullptr )
{
    ( *this )[PointerFromString( "meta.filename" )] = GetFullFilename();

    m_params.emplace_back(
            new PARAM<int>( "meta.version", &m_schemaVersion, m_schemaVersion, true ) );
}


JSON_SETTINGS::~JSON_SETTINGS()
{
    for( auto param: m_params )
        delete param;

    m_params.clear();
}


wxString JSON_SETTINGS::GetFullFilename() const
{
    return wxString( m_filename.c_str(), wxConvUTF8 ) + "." + getFileExt();
}


void JSON_SETTINGS::Load()
{
    for( auto param : m_params )
    {
        try
        {
            param->Load( this, m_resetParamsIfMissing );
        }
        catch( ... )
        {
            // Skip unreadable parameters in file:
#ifdef DEBUG
            wxLogMessage( wxString::Format( "param '%s' load err", param->GetJsonPath().c_str() ) );
#endif
        }
    }
}


bool JSON_SETTINGS::LoadFromFile( const std::string& aDirectory )
{
    // First, load all params to default values
    clear();
    Load();

    bool success         = true;
    bool migrated        = false;
    bool legacy_migrated = false;

    LOCALE_IO locale;

    auto migrateFromLegacy = [&] ( wxFileName& aPath ) {
        // Backup and restore during migration so that the original can be mutated if convenient
        bool backed_up = false;
        wxFileName temp;

        if( aPath.IsDirWritable() )
        {
            temp.AssignTempFileName( aPath.GetFullPath() );

            if( !wxCopyFile( aPath.GetFullPath(), temp.GetFullPath() ) )
            {
                wxLogTrace( traceSettings, "%s: could not create temp file for migration",
                        GetFullFilename() );
            }
            else
                backed_up = true;
        }

        wxConfigBase::DontCreateOnDemand();
        auto cfg = std::make_unique<wxFileConfig>( wxT( "" ), wxT( "" ), aPath.GetFullPath() );

        // If migrate fails or is not implemented, fall back to built-in defaults that were
        // already loaded above
        if( !MigrateFromLegacy( cfg.get() ) )
        {
            wxLogTrace( traceSettings,
                        "%s: migrated; not all settings were found in legacy file",
                        GetFullFilename() );
        }
        else
        {
            wxLogTrace( traceSettings, "%s: migrated from legacy format", GetFullFilename() );
        }

        if( backed_up )
        {
            cfg.reset();
            wxCopyFile( temp.GetFullPath(), aPath.GetFullPath() );
            wxRemoveFile( temp.GetFullPath() );
        }

        // Either way, we want to clean up the old file afterwards
        legacy_migrated = true;
    };

    wxFileName path;

    if( aDirectory.empty() )
    {
        path.Assign( m_filename );
        path.SetExt( getFileExt() );
    }
    else
    {
        wxString dir( aDirectory.c_str(), wxConvUTF8 );
#ifdef __WINDOWS__
        path.Assign( dir, m_filename, getFileExt() );
#else
        wxString name( m_filename.c_str(), wxConvUTF8 );
        path.Assign( dir, name, getFileExt() );
#endif
    }

    if( !path.Exists() )
    {
        // Case 1: legacy migration, no .json extension yet
        path.SetExt( getLegacyFileExt() );

        if( path.Exists() )
        {
            migrateFromLegacy( path );
        }
        // Case 2: legacy filename is different from new one
        else if( !m_legacy_filename.empty() )
        {
            path.SetName( m_legacy_filename );

            if( path.Exists() )
                migrateFromLegacy( path );
        }
        else
        {
            success = false;
        }
    }
    else
    {
        try
        {
            FILE* fp = wxFopen( path.GetFullPath(), wxT( "rt" ) );
            *static_cast<nlohmann::json*>( this ) = nlohmann::json::parse( fp, nullptr,
                    /* allow_exceptions = */ true,
                    /* ignore_comments  = */ true );

            // If parse succeeds, check if schema migration is required
            int filever = -1;

            try
            {
                filever = at( PointerFromString( "meta.version" ) ).get<int>();
            }
            catch( ... )
            {
                wxLogTrace(
                        traceSettings, "%s: file version could not be read!", GetFullFilename() );
                success = false;
            }

            if( filever >= 0 && filever < m_schemaVersion )
            {
                wxLogTrace( traceSettings, "%s: attempting migration from version %d to %d",
                            GetFullFilename(), filever, m_schemaVersion );

                if( Migrate() )
                {
                    migrated = true;
                }
                else
                {
                    wxLogTrace( traceSettings, "%s: migration failed!", GetFullFilename() );
                }
            }
            else if( filever > m_schemaVersion )
            {
                wxLogTrace( traceSettings,
                        "%s: warning: file version %d is newer than latest (%d)", GetFullFilename(),
                        filever, m_schemaVersion );
            }
        }
        catch( nlohmann::json::parse_error& error )
        {
            wxLogTrace(
                    traceSettings, "Parse error reading %s: %s", path.GetFullPath(), error.what() );
            wxLogTrace( traceSettings, "Attempting migration in case file is in legacy format" );
            migrateFromLegacy( path );
        }
    }

    // Now that we have new data in the JSON structure, load the params again
    Load();

    // And finally load any nested settings
    for( auto settings : m_nested_settings )
        settings->LoadFromFile();

    wxLogTrace( traceSettings, "Loaded %s with schema %d", GetFullFilename(), m_schemaVersion );

    // If we migrated, clean up the legacy file (with no extension)
    if( legacy_migrated || migrated )
    {
        if( legacy_migrated && m_deleteLegacyAfterMigration && !wxRemoveFile( path.GetFullPath() ) )
        {
            wxLogTrace(
                    traceSettings, "Warning: could not remove legacy file %s", path.GetFullPath() );
        }

        // And write-out immediately so that we don't lose data if the program later crashes.
        SaveToFile( aDirectory );
    }

    return success;
}


bool JSON_SETTINGS::Store()
{
    bool modified = false;

    for( auto param : m_params )
    {
        modified |= !param->MatchesFile( this );
        param->Store( this );
    }

    return modified;
}


void JSON_SETTINGS::ResetToDefaults()
{
    for( auto param : m_params )
        param->SetDefault();
}


bool JSON_SETTINGS::SaveToFile( const std::string& aDirectory, bool aForce )
{
    if( !m_writeFile )
        return false;

    // Default PROJECT won't have a filename set
    if( m_filename.empty() )
        return false;

    wxFileName path;

    if( aDirectory.empty() )
    {
        path.Assign( m_filename );
        path.SetExt( getFileExt() );
    }
    else
    {
        wxString dir( aDirectory.c_str(), wxConvUTF8 );
        path.Assign( dir, m_filename, getFileExt() );
    }

    if( !m_createIfMissing && !path.FileExists() )
    {
        wxLogTrace( traceSettings,
                "File for %s doesn't exist and m_createIfMissing == false; not saving",
                GetFullFilename() );
        return false;
    }

    if( ( path.FileExists() && !path.IsFileWritable() ) ||
        ( !path.FileExists() && !path.IsDirWritable() ) )
    {
        wxLogTrace( traceSettings, "File for %s is read-only; not saving", GetFullFilename() );
        return false;
    }

    bool modified = false;

    for( auto settings : m_nested_settings )
        modified |= settings->SaveToFile();

    modified |= Store();

    if( !modified && !aForce && path.FileExists() )
    {
        wxLogTrace( traceSettings, "%s contents not modified, skipping save", GetFullFilename() );
        return false;
    }
    else if( !modified && !aForce && !m_createIfDefault )
    {
        wxLogTrace( traceSettings,
                "%s contents still default and m_createIfDefault == false; not saving",
                    GetFullFilename() );
        return false;
    }

    if( !path.DirExists() && !path.Mkdir() )
    {
        wxLogTrace( traceSettings, "Warning: could not create path %s, can't save %s",
                    path.GetPath(), GetFullFilename() );
        return false;
    }

    wxLogTrace( traceSettings, "Saving %s", GetFullFilename() );

    LOCALE_IO dummy;
    bool success = true;

    try
    {
        wxFile   file( path.GetFullPath(), wxFile::write );

        std::stringstream buffer;
        buffer << std::setw( 2 ) << *this << std::endl;

        if( !file.IsOpened() || !file.Write( buffer.str().c_str(), buffer.str().size() ) )
        {
            wxLogTrace( traceSettings, "Warning: could not save %s", GetFullFilename() );
            success = false;
        }
    }
    catch( ... )
    {
        success = false;
    }

    return success;
}


OPT<nlohmann::json> JSON_SETTINGS::GetJson( const std::string& aPath ) const
{
    nlohmann::json::json_pointer ptr = PointerFromString( aPath );

    if( this->contains( ptr ) )
    {
        try
        {
            return OPT<nlohmann::json>{ this->at( ptr ) };
        }
        catch( ... )
        {
        }
    }

    return OPT<nlohmann::json>{};
}


bool JSON_SETTINGS::Migrate()
{
    wxLogTrace( traceSettings, "Migrate() not implemented for %s", typeid( *this ).name() );
    return false;
}


bool JSON_SETTINGS::MigrateFromLegacy( wxConfigBase* aLegacyConfig )
{
    wxLogTrace( traceSettings,
            "MigrateFromLegacy() not implemented for %s", typeid( *this ).name() );
    return false;
}


nlohmann::json::json_pointer JSON_SETTINGS::PointerFromString( std::string aPath )
{
    std::replace( aPath.begin(), aPath.end(), '.', '/' );
    aPath.insert( 0, "/" );

    nlohmann::json::json_pointer p;

    try
    {
        p = nlohmann::json::json_pointer( aPath );
    }
    catch( ... )
    {
        wxASSERT_MSG( false, wxT( "Invalid pointer path in PointerFromString!" ) );
    }

    return p;
}


template<typename ValueType>
bool JSON_SETTINGS::fromLegacy( wxConfigBase* aConfig, const std::string& aKey,
                             const std::string& aDest )
{
    ValueType val;

    if( aConfig->Read( aKey, &val ) )
    {
        try
        {
            ( *this )[PointerFromString( aDest )] = val;
        }
        catch( ... )
        {
            wxASSERT_MSG( false, wxT( "Could not write value in fromLegacy!" ) );
            return false;
        }

        return true;
    }

    return false;
}


// Explicitly declare these because we only support a few types anyway, and it means we can keep
// wxConfig detail out of the header file
template bool JSON_SETTINGS::fromLegacy<int>( wxConfigBase*, const std::string&,
                                              const std::string& );

template bool JSON_SETTINGS::fromLegacy<double>( wxConfigBase*, const std::string&,
                                              const std::string& );

template bool JSON_SETTINGS::fromLegacy<bool>( wxConfigBase*, const std::string&,
                                               const std::string& );


bool JSON_SETTINGS::fromLegacyString( wxConfigBase* aConfig, const std::string& aKey,
                                      const std::string& aDest )
{
    wxString str;

    if( aConfig->Read( aKey, &str ) )
    {
        try
        {
            ( *this )[PointerFromString( aDest )] = str.ToUTF8();
        }
        catch( ... )
        {
            wxASSERT_MSG( false, wxT( "Could not write value in fromLegacyString!" ) );
            return false;
        }

        return true;
    }

    return false;
}


bool JSON_SETTINGS::fromLegacyColor( wxConfigBase* aConfig, const std::string& aKey,
    const std::string& aDest )
{
    wxString str;

    if( aConfig->Read( aKey, &str ) )
    {
        KIGFX::COLOR4D color;
        color.SetFromWxString( str );

        try
        {
            nlohmann::json js = nlohmann::json::array( { color.r, color.g, color.b, color.a } );
            ( *this )[PointerFromString( aDest )] = js;
        }
        catch( ... )
        {
            wxASSERT_MSG( false, wxT( "Could not write value in fromLegacyColor!" ) );
            return false;
        }

        return true;
    }

    return false;
}


void JSON_SETTINGS::AddNestedSettings( NESTED_SETTINGS* aSettings )
{
    wxLogTrace( traceSettings, "AddNestedSettings %s", aSettings->GetFilename() );
    m_nested_settings.push_back( aSettings );
}


void JSON_SETTINGS::ReleaseNestedSettings( NESTED_SETTINGS* aSettings )
{
    if( !aSettings )
        return;

    auto it = std::find_if( m_nested_settings.begin(), m_nested_settings.end(),
                            [&aSettings]( const JSON_SETTINGS* aPtr ) {
                              return aPtr == aSettings;
                            } );

    if( it != m_nested_settings.end() )
    {
        wxLogTrace( traceSettings, "Flush and release %s", ( *it )->GetFilename() );
        ( *it )->SaveToFile();
        m_nested_settings.erase( it );
    }

    aSettings->SetParent( nullptr );
}


// Specializations to allow conversion between wxString and std::string via JSON_SETTINGS API

template<> OPT<wxString> JSON_SETTINGS::Get( const std::string& aPath ) const
{
    if( OPT<nlohmann::json> opt_json = GetJson( aPath ) )
        return wxString( opt_json->get<std::string>().c_str(), wxConvUTF8 );

    return NULLOPT;
}


template<> void JSON_SETTINGS::Set<wxString>( const std::string& aPath, wxString aVal )
{
    ( *this )[PointerFromString( std::move( aPath ) ) ] = aVal.ToUTF8();
}

// Specializations to allow directly reading/writing wxStrings from JSON

void to_json( nlohmann::json& aJson, const wxString& aString )
{
    aJson = aString.ToUTF8();
}


void from_json( const nlohmann::json& aJson, wxString& aString )
{
    aString = wxString( aJson.get<std::string>().c_str(), wxConvUTF8 );
}
