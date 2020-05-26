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

#ifndef _PARAMETERS_H
#define _PARAMETERS_H

#include <string>
#include <utility>
#include <math/util.h>

#include <core/optional.h>
#include <settings/json_settings.h>


class PARAM_BASE
{
public:
    PARAM_BASE( std::string aJsonPath, bool aReadOnly ) :
            m_path( std::move( aJsonPath ) ),
            m_readOnly( aReadOnly )
    {}

    virtual ~PARAM_BASE() = default;

    /**
     * Loads the value of this parameter from JSON to the underlying storage
     * @param aSettings is the JSON_SETTINGS object to load from.
     */
    virtual void Load( JSON_SETTINGS* aSettings ) const = 0;

    /**
     * Stores the value of this parameter to the given JSON_SETTINGS object
     * @param aSettings is the JSON_SETTINGS object to store into.
     */
    virtual void Store( JSON_SETTINGS* aSettings ) const = 0;

    virtual void SetDefault() = 0;

    /**
     * Checks whether or not this param has been changed from its default value
     * @return true if the parameter in memory matches its default value
     */
    virtual bool IsDefault() const = 0;

    /**
     * Checks whether the parameter in memory matches the one in a given JSON file
     * @param aSettings is a JSON_SETTINGS to check the JSON file contents of
     * @return true if the parameter in memory matches its value in the file
     */
    virtual bool MatchesFile( JSON_SETTINGS* aSettings ) const = 0;

    /**
     * @return the path name of the parameter used to store it in the json file
     * mainly usefull in error messages
     */
    const std::string& GetJsonPath() { return m_path; }

protected:
    /**
     * the string used to store the param in json files
     */
    std::string m_path;

    ///! True if the parameter pointer should never be overwritten
    bool m_readOnly;
};


template<typename ValueType>
class PARAM : public PARAM_BASE
{
public:
    PARAM( const std::string& aJsonPath, ValueType* aPtr, ValueType aDefault,
           bool aReadOnly = false ) :
            PARAM_BASE( aJsonPath, aReadOnly ),
            m_ptr( aPtr ),
            m_default( aDefault ),
            m_min(),
            m_max(),
            m_use_minmax( false )
    { }

    PARAM( const std::string& aJsonPath, ValueType* aPtr, ValueType aDefault, ValueType aMin,
           ValueType aMax, bool aReadOnly = false ) :
            PARAM_BASE( aJsonPath, aReadOnly ),
            m_ptr( aPtr ),
            m_default( aDefault ),
            m_min( aMin ),
            m_max( aMax ),
            m_use_minmax( true )
    { }

    void Load( JSON_SETTINGS* aSettings ) const override
    {
        if( m_readOnly )
            return;

        ValueType val = m_default;

        if( OPT<ValueType> optval = aSettings->Get<ValueType>( m_path ) )
        {
            val = *optval;

            if( m_use_minmax )
            {
                if( m_max < val || val < m_min )
                    val = m_default;
            }
        }

        *m_ptr = val;
    }

    void Store( JSON_SETTINGS* aSettings ) const override
    {
        aSettings->Set<ValueType>( m_path, *m_ptr );
    }

    ValueType GetDefault() const
    {
        return m_default;
    }

    void SetDefault() override
    {
        *m_ptr = m_default;
    }

    bool IsDefault() const override
    {
        return *m_ptr == m_default;
    }

    bool MatchesFile( JSON_SETTINGS* aSettings ) const override
    {
        if( OPT<ValueType> optval = aSettings->Get<ValueType>( m_path ) )
            return *optval == *m_ptr;

        return false;
    }

private:
    ValueType* m_ptr;
    ValueType m_default;
    ValueType m_min;
    ValueType m_max;
    bool m_use_minmax;
};


/**
 * Like a normal param, but with custom getter and setter functions
 * @tparam ValueType is the value to store
 */
template<typename ValueType>
class PARAM_LAMBDA : public PARAM_BASE
{
public:
    PARAM_LAMBDA( const std::string& aJsonPath,  std::function<ValueType()> aGetter,
                  std::function<void( ValueType )> aSetter, ValueType aDefault,
                  bool aReadOnly = false ) :
            PARAM_BASE( aJsonPath, aReadOnly ),
            m_default( aDefault ),
            m_getter( aGetter ),
            m_setter( aSetter )
    { }

    void Load( JSON_SETTINGS* aSettings ) const override
    {
        if( m_readOnly )
            return;

        ValueType val = m_default;

        if( std::is_same<ValueType, nlohmann::json>::value )
        {
            if( OPT<nlohmann::json> optval = aSettings->GetJson( m_path ) )
                val = *optval;
        }
        else
        {
            if( OPT<ValueType> optval = aSettings->Get<ValueType>( m_path ) )
                val = *optval;
        }

        m_setter( val );
    }

    void Store( JSON_SETTINGS* aSettings) const override
    {
        try
        {
            aSettings->Set<ValueType>( m_path, m_getter() );
        }
        catch( ... )
        {
        }
    }

    ValueType GetDefault() const
    {
        return m_default;
    }

    void SetDefault() override
    {
        m_setter( m_default );
    }

    bool IsDefault() const override
    {
        return m_getter() == m_default;
    }

    bool MatchesFile( JSON_SETTINGS* aSettings ) const override
    {
        if( std::is_same<ValueType, nlohmann::json>::value )
        {
            if( OPT<nlohmann::json> optval = aSettings->GetJson( m_path ) )
                return *optval == m_default;
        }
        else
        {
            if( OPT<ValueType> optval = aSettings->Get<ValueType>( m_path ) )
                return *optval == m_default;
        }

        // Not in file
        return false;
    }

private:
    ValueType m_default;

    std::function<ValueType()> m_getter;

    std::function<void( ValueType )> m_setter;
};


/**
 * Represents a parameter that has a scaling factor between the value in the file and the
 * value used internally (i.e. the value pointer).  This basically only makes sense to use
 * with int or double as ValueType.
 * @tparam ValueType is the internal type: the file always stores a double.
 */
template<typename ValueType>
class PARAM_SCALED: public PARAM_BASE
{
public:
    PARAM_SCALED( const std::string& aJsonPath, ValueType* aPtr, ValueType aDefault,
                  double aScale = 1.0, bool aReadOnly = false ) :
            PARAM_BASE( aJsonPath, aReadOnly ),
            m_ptr( aPtr ),
            m_default( aDefault ),
            m_min(),
            m_max(),
            m_use_minmax( false ),
            m_scale( aScale )
    { }

    PARAM_SCALED( const std::string& aJsonPath, ValueType* aPtr, ValueType aDefault,
                  ValueType aMin, ValueType aMax, double aScale = 1.0, bool aReadOnly = false ) :
            PARAM_BASE( aJsonPath, aReadOnly ),
            m_ptr( aPtr ),
            m_default( aDefault ),
            m_min( aMin ),
            m_max( aMax ),
            m_use_minmax( true ),
            m_scale( aScale )
    { }

    void Load( JSON_SETTINGS* aSettings ) const override
    {
        if( m_readOnly )
            return;

        double dval = m_default * m_scale;

        if( OPT<double> optval = aSettings->Get<double>( m_path ) )
            dval = *optval;

        ValueType val = KiROUND<ValueType>( dval / m_scale );

        if( m_use_minmax )
        {
            if( val > m_max || val < m_min )
                val = m_default;
        }

        *m_ptr = val;
    }

    void Store( JSON_SETTINGS* aSettings) const override
    {
        aSettings->Set<double>( m_path, *m_ptr * m_scale );
    }

    ValueType GetDefault() const
    {
        return m_default;
    }

    virtual void SetDefault() override
    {
        *m_ptr = m_default;
    }

    bool IsDefault() const override
    {
        return *m_ptr == m_default;
    }

    bool MatchesFile( JSON_SETTINGS* aSettings ) const override
    {
        if( OPT<double> optval = aSettings->Get<double>( m_path ) )
            return *optval == ( *m_ptr * m_scale );

        return false;
    }

private:
    ValueType* m_ptr;
    ValueType m_default;
    ValueType m_min;
    ValueType m_max;
    bool m_use_minmax;
    double m_scale;
};

template<typename Type>
class PARAM_LIST : public PARAM_BASE
{
public:
    PARAM_LIST( const std::string& aJsonPath, std::vector<Type>* aPtr,
                std::initializer_list<Type> aDefault, bool aReadOnly = false ) :
            PARAM_BASE( aJsonPath, aReadOnly ),
            m_ptr( aPtr ),
            m_default( aDefault )
    { }

    PARAM_LIST( const std::string& aJsonPath, std::vector<Type>* aPtr,
                std::vector<Type> aDefault, bool aReadOnly = false ) :
            PARAM_BASE( aJsonPath, aReadOnly ),
            m_ptr( aPtr ),
            m_default( aDefault )
    { }

    void Load( JSON_SETTINGS* aSettings ) const override
    {
        if( m_readOnly )
            return;

        std::vector<Type> val = m_default;

        if( OPT<nlohmann::json> js = aSettings->GetJson( m_path ) )
        {
            if( js->is_array() )
            {
                val.clear();

                for( const auto& el : js->items() )
                    val.push_back( el.value().get<Type>() );
            }
        }

        *m_ptr = val;
    }

    void Store( JSON_SETTINGS* aSettings) const override
    {
        nlohmann::json js = nlohmann::json::array();

        for( const auto& el : *m_ptr )
            js.push_back( el );

        aSettings->Set<nlohmann::json>( m_path, js );
    }

    void SetDefault() override
    {
        *m_ptr = m_default;
    }

    bool IsDefault() const override
    {
        return *m_ptr == m_default;
    }

    bool MatchesFile( JSON_SETTINGS* aSettings ) const override
    {
        if( OPT<nlohmann::json> js = aSettings->GetJson( m_path ) )
        {
            if( js->is_array() )
            {
                std::vector<Type> val;

                for( const auto& el : js->items() )
                    val.emplace_back( el.value().get<Type>() );

                return val == *m_ptr;
            }
        }

        return false;
    }

protected:
    std::vector<Type>* m_ptr;

    std::vector<Type> m_default;
};

/**
 * Represents a list of strings holding directory paths.
 * Normalizes paths to unix directory separator style in the file.
 */
class PARAM_PATH_LIST : public PARAM_LIST<wxString>
{
public:
    PARAM_PATH_LIST( const std::string& aJsonPath, std::vector<wxString>* aPtr,
                     std::initializer_list<wxString> aDefault, bool aReadOnly = false ) :
            PARAM_LIST( aJsonPath, aPtr, aDefault, aReadOnly )
    { }

    PARAM_PATH_LIST( const std::string& aJsonPath, std::vector<wxString>* aPtr,
                     std::vector<wxString> aDefault, bool aReadOnly = false ) :
            PARAM_LIST( aJsonPath, aPtr, aDefault, aReadOnly )
    { }

    void Load( JSON_SETTINGS* aSettings ) const override
    {
        if( m_readOnly )
            return;

        PARAM_LIST::Load( aSettings );

        for( size_t i = 0; i < m_ptr->size(); i++ )
            ( *m_ptr )[i] = fromFileFormat( ( *m_ptr )[i] );
    }

    void Store( JSON_SETTINGS* aSettings) const override
    {
        nlohmann::json js = nlohmann::json::array();

        for( const auto& el : *m_ptr )
            js.push_back( toFileFormat( el ) );

        aSettings->Set<nlohmann::json>( m_path, js );
    }

    bool MatchesFile( JSON_SETTINGS* aSettings ) const override
    {
        if( OPT<nlohmann::json> js = aSettings->GetJson( m_path ) )
        {
            if( js->is_array() )
            {
                std::vector<wxString> val;

                for( const auto& el : js->items() )
                    val.emplace_back( fromFileFormat( el.value().get<wxString>() ) );

                return val == *m_ptr;
            }
        }

        return false;
    }

private:
    wxString toFileFormat( const wxString& aString ) const
    {
        wxString ret = aString;
        ret.Replace( wxT( "\\" ), wxT( "/" ) );
        return ret;
    }

    wxString fromFileFormat( const wxString& aString ) const
    {
        wxString ret = aString;
#ifdef __WINDOWS__
        ret.Replace( wxT( "/" ), wxT( "\\" ) );
#endif
        return ret;
    }
};

/**
 * Represents a map of <std::string, Value>.  The key parameter has to be a string in JSON.
 *
 * The key must be stored in UTF-8 format, so any translated strings or strings provided by the
 * user as a key must be converted to UTF-8 at the site where they are placed in the underlying
 * map that this PARAM_MAP points to.
 *
 * Values must also be in UTF-8, but if you use wxString as the value type, this conversion will
 * be done automatically by the to_json and from_json helpers defined in json_settings.cpp
 *
 * @tparam Value is the value type of the map
 */
template<typename Value>
class PARAM_MAP : public PARAM_BASE
{
public:
    PARAM_MAP( const std::string& aJsonPath, std::map<std::string, Value>* aPtr,
               std::initializer_list<std::pair<const std::string, Value>> aDefault,
               bool aReadOnly = false ) :
            PARAM_BASE( aJsonPath, aReadOnly ),
            m_ptr( aPtr ),
            m_default( aDefault )
    { }

    void Load( JSON_SETTINGS* aSettings ) const override
    {
        if( m_readOnly )
            return;

        std::map<std::string, Value> val = m_default;

        if( OPT<nlohmann::json> js = aSettings->GetJson( m_path ) )
        {
            if( js->is_object() )
            {
                val.clear();

                for( const auto& el : js->items() )
                    val[ el.key() ] = el.value().get<Value>();
            }
        }

        *m_ptr = val;
    }

    void Store( JSON_SETTINGS* aSettings) const override
    {
        nlohmann::json js( {} );

        for( const auto& el : *m_ptr )
            js[ el.first ] = el.second;

        aSettings->Set<nlohmann::json>( m_path, js );
    }

    virtual void SetDefault() override
    {
        *m_ptr = m_default;
    }

    bool IsDefault() const override
    {
        return *m_ptr == m_default;
    }

    bool MatchesFile( JSON_SETTINGS* aSettings ) const override
    {
        if( OPT<nlohmann::json> js = aSettings->GetJson( m_path ) )
        {
            if( js->is_object() )
            {
                std::map<std::string, Value> val;

                for( const auto& el : js->items() )
                    val[ el.key() ] = el.value().get<Value>();

                return val == *m_ptr;
            }
        }

        return false;
    }

private:
    std::map<std::string, Value>* m_ptr;

    std::map<std::string, Value> m_default;
};

#endif
